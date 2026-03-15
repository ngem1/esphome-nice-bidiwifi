#include "t4_bus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "driver/uart.h"
#include "esp_timer.h"

namespace esphome {
namespace nice_bidiwifi {

static const char *const TAG = "nice_bidiwifi.bus";

void T4Bus::setup(int rx_pin, int tx_pin) {
  uart_config_t uart_config = {};
  uart_config.baud_rate = T4_BAUD_RATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 0;
  uart_config.source_clk = UART_SCLK_DEFAULT;

  ESP_ERROR_CHECK(uart_param_config(uart_port_, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uart_port_, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(uart_port_, 256, 0, 0, nullptr, 0));

  ESP_LOGI(TAG, "UART%d initialized: RX=%d TX=%d @ %d baud", uart_port_, rx_pin, tx_pin, T4_BAUD_RATE);
}

void T4Bus::reset_rx_() {
  rx_state_ = RX_WAIT_SYNC;
  rx_buffer_.clear();
  rx_expected_size_ = 0;
}

bool T4Bus::receive(T4RxPacket &packet) {
  uint8_t byte;
  while (uart_read_bytes(uart_port_, &byte, 1, 0) > 0) {
    switch (rx_state_) {
      case RX_WAIT_SYNC:
        // Accept any number of break bytes (0x00), then look for start code
        if (byte == 0x00) {
          rx_state_ = RX_WAIT_START;
        } else if (byte == T4_START_CODE || byte == T4_START_CODE_EXT) {
          // Start code without break — still accept (robust to missed breaks)
          rx_buffer_.clear();
          rx_buffer_.push_back(byte);
          rx_state_ = RX_READ_SIZE;
        }
        break;

      case RX_WAIT_START:
        if (byte == T4_START_CODE || byte == T4_START_CODE_EXT) {
          rx_buffer_.clear();
          rx_buffer_.push_back(byte);
          rx_state_ = RX_READ_SIZE;
        } else if (byte == 0x00) {
          // Additional break bytes — stay in WAIT_START
        } else {
          reset_rx_();
        }
        break;

      case RX_READ_SIZE:
        rx_buffer_.push_back(byte);
        if (byte > 128) {  // max packet body size (protocol allows up to ~250)
          ESP_LOGW(TAG, "Invalid packet size: %d", byte);
          reset_rx_();
        } else {
          rx_expected_size_ = byte + 3;  // body_size value + start_code byte + size byte + trailing size byte
          rx_state_ = RX_READ_DATA;
        }
        break;

      case RX_READ_DATA:
        rx_buffer_.push_back(byte);
        if (rx_buffer_.size() >= rx_expected_size_) {
          if (validate_packet_()) {
            packet.data = rx_buffer_;
            packet.valid = true;
            ESP_LOGV(TAG, "RX: %s", format_hex_pretty(rx_buffer_).c_str());
            reset_rx_();
            return true;
          } else {
            reset_rx_();
          }
        }
        break;

      default:
        reset_rx_();
        break;
    }
  }
  return false;
}

bool T4Bus::validate_packet_() {
  if (rx_buffer_.size() < 10)
    return false;

  uint8_t *d = rx_buffer_.data();
  uint8_t packet_size = d[1];

  // Check trailing size byte matches
  if (d[rx_buffer_.size() - 1] != packet_size) {
    ESP_LOGW(TAG, "Trailing size mismatch: %02X != %02X", d[rx_buffer_.size() - 1], packet_size);
    return false;
  }

  // CRC1: XOR of bytes 2-7 (to_row, to_addr, from_row, from_addr, msg_type, msg_size)
  uint8_t crc1 = t4_xor_checksum(d, 2, 6);
  if (d[8] != crc1) {
    ESP_LOGW(TAG, "CRC1 mismatch: %02X != %02X", d[8], crc1);
    return false;
  }

  // CRC2: XOR of message bytes (from byte 9 to second-to-last)
  size_t msg_start = 9;
  size_t msg_end = rx_buffer_.size() - 2;  // before crc2 and trailing size
  if (msg_end > msg_start) {
    uint8_t crc2 = t4_xor_checksum(d, msg_start, msg_end - msg_start);
    if (d[msg_end] != crc2) {
      ESP_LOGW(TAG, "CRC2 mismatch: %02X != %02X", d[msg_end], crc2);
      return false;
    }
  }

  return true;
}

void T4Bus::send(const std::vector<uint8_t> &data) {
  // Send UART break (~520us at 19200 baud)
  // Achieved by sending 0x00 at half the baud rate, creating a long low pulse
  uart_wait_tx_done(uart_port_, pdMS_TO_TICKS(100));
  uart_set_baudrate(uart_port_, 9200);
  const uint8_t brk = 0x00;
  uart_write_bytes(uart_port_, &brk, 1);
  uart_wait_tx_done(uart_port_, pdMS_TO_TICKS(100));
  uart_set_baudrate(uart_port_, T4_BAUD_RATE);

  // Send packet data
  uart_write_bytes(uart_port_, data.data(), data.size());
  uart_wait_tx_done(uart_port_, pdMS_TO_TICKS(100));

  last_tx_time_ = esp_timer_get_time() / 1000LL;  // convert us to ms

  ESP_LOGV(TAG, "TX: %s", format_hex_pretty(data).c_str());
}

bool T4Bus::ready_to_send() const {
  int64_t now = esp_timer_get_time() / 1000LL;
  return (now - last_tx_time_) >= T4_TX_INTERVAL_MS;
}

}  // namespace nice_bidiwifi
}  // namespace esphome
