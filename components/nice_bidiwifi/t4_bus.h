#pragma once

#include <vector>
#include <cstdint>
#include "driver/uart.h"
#include "t4_protocol.h"

namespace esphome {
namespace nice_bidiwifi {

// Receive state machine states
enum T4RxState : uint8_t {
  RX_WAIT_SYNC,     // Waiting for 0x00
  RX_WAIT_START,    // Waiting for 0x55 or 0xF0
  RX_READ_SIZE,     // Reading packet size byte
  RX_READ_DATA,     // Reading packet body (validation runs inline when complete)
};

// Parsed packet from bus
struct T4RxPacket {
  std::vector<uint8_t> data;  // Full packet including start code and size
  bool valid{false};
};

class T4Bus {
 public:
  // Initialize ESP-IDF UART
  void setup(int rx_pin, int tx_pin);

  // Call from loop() -- processes incoming bytes, returns true when valid packet received
  bool receive(T4RxPacket &packet);

  // Send packet with UART break prefix
  void send(const std::vector<uint8_t> &data);

  // Check if enough time has passed for next TX
  bool ready_to_send() const;

 protected:
  uart_port_t uart_port_{UART_NUM_2};

  // RX state machine
  T4RxState rx_state_{RX_WAIT_SYNC};
  std::vector<uint8_t> rx_buffer_;
  uint8_t rx_expected_size_{0};

  void reset_rx_();
  bool validate_packet_();

  // TX timing (ms, wraps safely with unsigned subtraction)
  int64_t last_tx_time_{0};
};

}  // namespace nice_bidiwifi
}  // namespace esphome
