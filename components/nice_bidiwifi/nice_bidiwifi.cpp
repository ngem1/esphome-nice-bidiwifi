#include "nice_bidiwifi.h"
#include "nice_number.h"
#include "driver/gpio.h"

namespace esphome {
namespace nice_bidiwifi {

static const char *const TAG = "nice_bidiwifi";

void NiceBidiWiFi::setup() {
  ESP_LOGI(TAG, "Setting up Nice BiDi-WiFi hub...");
  this->bus_.setup(this->rx_pin_, this->tx_pin_);
  this->setup_leds_();
  this->load_learned_timings_();
}

void NiceBidiWiFi::dump_config() {
  ESP_LOGCONFIG(TAG, "Nice BiDi-WiFi Hub:");
  ESP_LOGCONFIG(TAG, "  UART RX Pin: %d", this->rx_pin_);
  ESP_LOGCONFIG(TAG, "  UART TX Pin: %d", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  Hub Address: 0x%02X%02X", this->hub_address_.row, this->hub_address_.address);
  ESP_LOGCONFIG(TAG, "  LED1: R=%d G=%d  LED2: R=%d G=%d  LED3: %d",
                this->led1_red_pin_, this->led1_green_pin_,
                this->led2_red_pin_, this->led2_green_pin_, this->led3_pin_);
  if (this->device_found_) {
    ESP_LOGCONFIG(TAG, "  Device Address: 0x%02X%02X", this->device_address_.row, this->device_address_.address);
    ESP_LOGCONFIG(TAG, "  Motor Type: %s", t4_motor_type_name(this->motor_type_));
    ESP_LOGCONFIG(TAG, "  Product: %s", this->product_.c_str());
    ESP_LOGCONFIG(TAG, "  Manufacturer: %s", this->manufacturer_.c_str());
    ESP_LOGCONFIG(TAG, "  Hardware: %s", this->hardware_ver_.c_str());
    ESP_LOGCONFIG(TAG, "  Firmware: %s", this->firmware_ver_.c_str());
  }
}

void NiceBidiWiFi::loop() {
  uint32_t now = millis();

  // 1. Receive packets from bus
  T4RxPacket packet;
  while (this->bus_.receive(packet)) {
    if (packet.valid) {
      this->last_bus_activity_time_ = now;
      this->parse_packet_(packet);
    }
  }

  // 2. Process TX queue
  if (this->bus_.ready_to_send() && !this->tx_queue_.empty()) {
    this->bus_.send(this->tx_queue_.front());
    this->tx_queue_.pop();
    this->last_bus_activity_time_ = now;
  }

  // 3. Discovery with exponential backoff (delay 5s to let API/logs connect first)
  if (!this->device_found_ && now > 5000 && (now - this->last_discovery_time_ >= this->get_discovery_interval_())) {
    this->discover_devices_();
    this->last_discovery_time_ = now;
    this->discovery_attempts_++;
  }

  // 4. Initialization after discovery
  if (this->device_found_ && !this->init_complete_) {
    this->init_device_(this->device_address_.row, this->device_address_.address, DEV_CONTROLLER);
    this->init_complete_ = true;
  }

  // 5. Position polling during movement (skip for Robus — breaks if polled while moving)
  if (this->device_found_ && this->init_complete_ && !this->is_robus_ &&
      (this->operation_state_ == STA_OPENING || this->operation_state_ == STA_CLOSING) &&
      (now - this->last_position_poll_time_ >= T4_POSITION_POLL_MS)) {
    this->request_position_();
    this->last_position_poll_time_ = now;
  }

  // 6. Periodic status refresh (recovers from missed packets)
  if (this->device_found_ && this->init_complete_ &&
      (now - this->last_status_refresh_time_ >= T4_STATUS_REFRESH_MS)) {
    this->request_status_refresh_();
    this->last_status_refresh_time_ = now;
  }

  // 7. Periodic I/O state polling (limit switches, photocell)
  if (this->device_found_ && this->init_complete_ &&
      (now - this->last_io_poll_time_ >= T4_IO_POLL_MS)) {
    this->request_io_state_();
    this->last_io_poll_time_ = now;
  }

  // 8. Time-based position estimation during movement (fallback when encoder unavailable)
  if (this->device_found_ && this->init_complete_ &&
      (this->operation_state_ == STA_OPENING || this->operation_state_ == STA_CLOSING)) {
    this->estimate_time_based_position_();
  }

  // 9. Update status LEDs (~20Hz for smooth blink patterns)
  if (now - this->last_led_update_time_ >= 50) {
    this->update_leds_();
    this->last_led_update_time_ = now;
  }
}

// --- Command API ---

void NiceBidiWiFi::send_control_cmd(uint8_t cmd) {
  ESP_LOGI(TAG, "Sending control command: %s (0x%02X)", t4_control_cmd_name(cmd), cmd);
  auto pkt = t4_build_cmd(this->device_address_, this->hub_address_, cmd);
  this->tx_queue_.push(std::move(pkt));
}

void NiceBidiWiFi::send_inf_cmd(uint8_t device_type, uint8_t register_id, uint8_t run_cmd,
                                 uint8_t next_data, const std::vector<uint8_t> &data) {
  auto pkt = t4_build_inf(this->device_address_, this->hub_address_,
                           device_type, register_id, run_cmd, next_data, data);
  this->tx_queue_.push(std::move(pkt));
}

void NiceBidiWiFi::send_set_register(uint8_t register_id, const std::vector<uint8_t> &data) {
  this->send_inf_cmd(DEV_CONTROLLER, register_id, RUN_SET, 0x00, data);
}

void NiceBidiWiFi::send_get_register(uint8_t register_id) {
  this->send_inf_cmd(DEV_CONTROLLER, register_id, RUN_GET);
}

void NiceBidiWiFi::send_raw(const std::vector<uint8_t> &data) {
  this->tx_queue_.push(data);
}

// --- Discovery & Initialization ---

void NiceBidiWiFi::discover_devices_() {
  ESP_LOGI(TAG, "Discovering devices on bus...");
  // Query WHO on broadcast (all device types)
  auto who = t4_build_inf(T4_BROADCAST, this->hub_address_, DEV_ALL, REG_WHO, RUN_GET);
  this->tx_queue_.push(std::move(who));
  // Query WHO specifically for OXI/radio devices (profile 0x0A)
  auto who_radio = t4_build_inf(T4_BROADCAST, this->hub_address_, DEV_RADIO, REG_WHO, RUN_GET);
  this->tx_queue_.push(std::move(who_radio));
  // Query PRODUCT on broadcast
  auto prod = t4_build_inf(T4_BROADCAST, this->hub_address_, DEV_ALL, REG_PRODUCT, RUN_GET);
  this->tx_queue_.push(std::move(prod));
}

void NiceBidiWiFi::init_device_(uint8_t addr1, uint8_t addr2, uint8_t device_type) {
  T4Address addr{addr1, addr2};
  ESP_LOGI(TAG, "Initializing device at 0x%02X%02X (type 0x%02X)", addr1, addr2, device_type);

  if (device_type == DEV_CONTROLLER) {
    // Query all controller info
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_TYPE_M, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_MANUFACTURER, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_FIRMWARE, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_PRODUCT, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_HARDWARE, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_DESCRIPTION, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_POS_MAX, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_POS_MIN, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_MAX_OPN, RUN_GET));
    this->request_position_();
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_STATUS, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_AUTOCLS, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_PH_CLS_ON, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_ALW_CLS_ON, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_TOTAL_COUNT, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_MAINT_COUNT, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_DIAG_IO, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_STANDBY_ACT, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_PEAK_ON, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_BLINK_ON, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_KEY_LOCK, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, REG_DIAG_BB, RUN_GET));
    // Query all registered number registers at init
    for (auto *num : this->numbers_) {
      ESP_LOGI(TAG, "Querying number register 0x%02X at init", num->get_register_id());
      this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_CONTROLLER, num->get_register_id(), RUN_GET));
    }
  } else if (device_type == DEV_RADIO) {
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_PRODUCT, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_HARDWARE, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_FIRMWARE, RUN_GET));
    this->tx_queue_.push(t4_build_inf(addr, this->hub_address_, DEV_ALL, REG_DESCRIPTION, RUN_GET));
  }
}

void NiceBidiWiFi::request_position_() {
  auto pkt = t4_build_inf(this->device_address_, this->hub_address_,
                           DEV_CONTROLLER, REG_CUR_POS, RUN_GET);
  this->tx_queue_.push(std::move(pkt));
}

void NiceBidiWiFi::request_status_refresh_() {
  this->tx_queue_.push(t4_build_inf(this->device_address_, this->hub_address_,
                                     DEV_CONTROLLER, REG_STATUS, RUN_GET));
}

void NiceBidiWiFi::request_io_state_() {
  this->tx_queue_.push(t4_build_inf(this->device_address_, this->hub_address_,
                                     DEV_CONTROLLER, REG_DIAG_IO, RUN_GET));
}

void NiceBidiWiFi::detect_device_type_() {
  if (this->product_.find("WLA") != std::string::npos) {
    this->is_walky_ = true;
    ESP_LOGI(TAG, "Detected Walky — using 1-byte position values");
  }
  if (this->product_.find("ROB") != std::string::npos) {
    this->is_robus_ = true;
    ESP_LOGI(TAG, "Detected Robus — position polling disabled during movement");
  }
}

uint32_t NiceBidiWiFi::get_discovery_interval_() const {
  uint8_t idx = this->discovery_attempts_;
  if (idx >= T4_DISCOVERY_BACKOFF_COUNT) {
    idx = T4_DISCOVERY_BACKOFF_COUNT - 1;
  }
  return T4_DISCOVERY_BACKOFF[idx];
}

void NiceBidiWiFi::request_status_confirmation_() {
  ESP_LOGD(TAG, "Requesting status confirmation (I/O + status)");
  this->awaiting_confirmation_ = true;
  this->request_io_state_();
  this->request_status_refresh_();
}

void NiceBidiWiFi::request_continuation_(uint8_t device_type, uint8_t register_id, uint8_t next_offset) {
  ESP_LOGD(TAG, "Requesting continuation for register 0x%02X at offset 0x%02X", register_id, next_offset);
  this->send_inf_cmd(device_type, register_id, RUN_GET, next_offset);
}

void NiceBidiWiFi::save_learned_timings_() {
  LearnedTimings data{};
  data.open_duration = this->last_open_duration_ms_;
  data.close_duration = this->last_close_duration_ms_;
  data.valid = (data.open_duration > 0 || data.close_duration > 0);
  if (this->timing_pref_.save(&data)) {
    ESP_LOGI(TAG, "Saved learned timings (open=%ums, close=%ums)", data.open_duration, data.close_duration);
  }
}

void NiceBidiWiFi::load_learned_timings_() {
  this->timing_pref_ = global_preferences->make_preference<LearnedTimings>(fnv1_hash("nice_bidiwifi_timing"));
  LearnedTimings data{};
  if (this->timing_pref_.load(&data) && data.valid) {
    if (data.open_duration >= T4_MIN_LEARNED_DURATION && data.open_duration <= T4_MAX_LEARNED_DURATION) {
      this->last_open_duration_ms_ = data.open_duration;
    }
    if (data.close_duration >= T4_MIN_LEARNED_DURATION && data.close_duration <= T4_MAX_LEARNED_DURATION) {
      this->last_close_duration_ms_ = data.close_duration;
    }
    ESP_LOGI(TAG, "Loaded learned timings (open=%ums, close=%ums)",
             this->last_open_duration_ms_, this->last_close_duration_ms_);
  }
}

void NiceBidiWiFi::confirm_position_from_io_(uint8_t io_byte) {
  bool limit_close = (io_byte & 0x01) != 0;
  bool limit_open = (io_byte & 0x02) != 0;

  if (limit_close && this->operation_state_ == STA_CLOSED) {
    // Confirmed fully closed
    this->pos_current_ = this->pos_close_;
    ESP_LOGD(TAG, "Position confirmed: CLOSED via limit switch");
    this->notify_state_change_();
  } else if (limit_open && this->operation_state_ == STA_OPENED) {
    // Confirmed fully open
    this->pos_current_ = this->pos_open_;
    ESP_LOGD(TAG, "Position confirmed: OPEN via limit switch");
    this->notify_state_change_();
  }
  this->awaiting_confirmation_ = false;
}

void NiceBidiWiFi::handle_movement_transition_(uint8_t old_state, uint8_t new_state) {
  uint32_t now_ms = millis();

  // Track movement start
  if ((new_state == STA_OPENING || new_state == STA_CLOSING) &&
      old_state != STA_OPENING && old_state != STA_CLOSING) {
    this->movement_start_time_ = now_ms;
    this->position_at_movement_start_ = this->get_position_percent();
  }

  // Learn duration when movement completes fully
  if ((old_state == STA_OPENING || old_state == STA_CLOSING) &&
      new_state != STA_OPENING && new_state != STA_CLOSING &&
      this->movement_start_time_ > 0) {
    uint32_t duration = now_ms - this->movement_start_time_;

    if (duration >= T4_MIN_LEARNED_DURATION && duration <= T4_MAX_LEARNED_DURATION) {
      if (old_state == STA_OPENING && new_state == STA_OPENED) {
        // Check deviation from existing value
        if (this->last_open_duration_ms_ == 0 ||
            std::abs(static_cast<float>(duration) - static_cast<float>(this->last_open_duration_ms_)) /
            static_cast<float>(this->last_open_duration_ms_) <= T4_LEARNING_DEVIATION + 0.5f) {
          this->last_open_duration_ms_ = duration;
          ESP_LOGI(TAG, "Learned open duration: %u ms", duration);
          this->save_learned_timings_();
        }
      } else if (old_state == STA_CLOSING && new_state == STA_CLOSED) {
        if (this->last_close_duration_ms_ == 0 ||
            std::abs(static_cast<float>(duration) - static_cast<float>(this->last_close_duration_ms_)) /
            static_cast<float>(this->last_close_duration_ms_) <= T4_LEARNING_DEVIATION + 0.5f) {
          this->last_close_duration_ms_ = duration;
          ESP_LOGI(TAG, "Learned close duration: %u ms", duration);
          this->save_learned_timings_();
        }
      }
    }
    this->movement_start_time_ = 0;
  }

  // Status confirmation when movement stops unexpectedly
  if (new_state == STA_STOPPED && (old_state == STA_OPENING || old_state == STA_CLOSING)) {
    ESP_LOGD(TAG, "Unexpected stop during movement — requesting confirmation");
    this->movement_start_time_ = 0;
    this->request_status_confirmation_();
  }

  // Query limit switches when movement ends
  if (new_state == STA_OPENED || new_state == STA_CLOSED || new_state == STA_STOPPED) {
    this->request_io_state_();
  }
}

void NiceBidiWiFi::estimate_time_based_position_() {
  // Only estimate if we haven't received fresh encoder data recently (>2s)
  uint32_t now = millis();
  if (now - this->last_encoder_update_time_ < 2000) {
    return;  // Encoder data is fresh, no estimation needed
  }

  // Need learned durations to estimate
  uint32_t duration_ms = 0;
  if (this->operation_state_ == STA_OPENING && this->last_open_duration_ms_ > 0) {
    duration_ms = this->last_open_duration_ms_;
  } else if (this->operation_state_ == STA_CLOSING && this->last_close_duration_ms_ > 0) {
    duration_ms = this->last_close_duration_ms_;
  }

  if (duration_ms == 0 || this->movement_start_time_ == 0) {
    return;  // No learned timing available
  }

  float elapsed = static_cast<float>(now - this->movement_start_time_);
  float progress = elapsed / static_cast<float>(duration_ms);
  if (progress > 1.0f) progress = 1.0f;

  float estimated_pos;
  if (this->operation_state_ == STA_OPENING) {
    estimated_pos = this->position_at_movement_start_ + progress * (1.0f - this->position_at_movement_start_);
  } else {
    estimated_pos = this->position_at_movement_start_ - progress * this->position_at_movement_start_;
  }

  if (estimated_pos < 0.0f) estimated_pos = 0.0f;
  if (estimated_pos > 1.0f) estimated_pos = 1.0f;

  // Publish estimated position to sensors
  if (this->position_sensor_ != nullptr)
    this->position_sensor_->publish_state(estimated_pos * 100.0f);

  for (auto &callback : this->state_callbacks_) {
    callback();
  }
}

// --- Packet Parsing ---

void NiceBidiWiFi::parse_packet_(const T4RxPacket &packet) {
  const auto &d = packet.data;

  // Minimum packet size check
  if (d.size() < 10) {
    return;
  }

  // Packet layout (after start_code and size byte):
  //   d[0] = start_code (0x55)
  //   d[1] = body_size
  //   d[2] = to_row, d[3] = to_addr
  //   d[4] = from_row, d[5] = from_addr
  //   d[6] = msg_type, d[7] = msg_size
  //   d[8] = crc1
  //   d[9..] = message body

  // Filter TX echoes — ignore packets originating from our own hub address
  if (d[4] == this->hub_address_.row && d[5] == this->hub_address_.address) {
    return;
  }

  uint8_t body_size = d[1];
  uint8_t msg_type = d[6];
  uint8_t msg_size = d[7];

  // Check for error response (0xFD marker)
  if (body_size == 0x0D && d.size() > 13 && d[13] == 0xFD) {
    uint8_t err_dev = (d.size() > 9) ? d[9] : 0;
    uint8_t err_reg = (d.size() > 10) ? d[10] : 0;
    ESP_LOGW(TAG, "Error: device=0x%02X register=0x%02X (%s) not supported", err_dev, err_reg, t4_register_name(err_reg));
    return;
  }

  if (msg_type == MSG_INF) {
    // INF packet with data
    if (d.size() < 14) {
      return;
    }

    uint8_t response_type = d[11];  // run_cmd echo
    uint8_t next_data = d[12];

    // Check response type
    // GET response: run_cmd was 0x99, response is 0x99-0x80=0x19 or 0x99-0x81=0x18
    // SET response: run_cmd was 0xA9, response is 0xA9-0x80=0x29
    if (response_type == 0x19 || response_type == 0x18) {
      // GET response — always parse what we have
      this->parse_evt_packet_(d);
      // If incomplete (0x18), request continuation
      if (response_type == 0x18 && next_data != 0x00) {
        uint8_t device_type = d[9];
        uint8_t register_id = d[10];
        this->request_continuation_(device_type, register_id, next_data);
      }
    } else if (response_type == 0x29) {
      // SET response (ACK)
      ESP_LOGD(TAG, "SET acknowledged for register 0x%02X", d[10]);
    } else if (body_size > 0x0D) {
      // Larger packet — treat as RSP
      this->parse_rsp_packet_(d);
    }
  } else if (msg_type == MSG_CMD) {
    // CMD packet
    if (d.size() >= 12) {
      uint8_t cmd_menu = d[9];
      if (cmd_menu == DEV_CONTROLLER || cmd_menu == MENU_CUR_MAN) {
        this->parse_cmd_status_(d);
      }
    }
  }

  // OXI packet sniffing — detect remote control commands on the bus
  if (d.size() >= 14 && d[9] == DEV_RADIO) {
    this->parse_oxi_packet_(d);
  }

  // Also check for RSP-style larger packets regardless of type
  if (msg_type == MSG_INF && body_size > 0x0D) {
    this->parse_rsp_packet_(d);
  }
}

void NiceBidiWiFi::parse_evt_packet_(const std::vector<uint8_t> &d) {
  if (d.size() < 14) {
    return;
  }

  uint8_t device_type = d[9];
  uint8_t register_id = d[10];
  uint8_t response_type = d[11];
  uint8_t data_len = d[12];     // number of payload bytes starting at d[14]
  uint8_t next_data = d[13];    // next offset for continuation (non-zero = more data)
  std::string str_data;
  if (d.size() > 16 && data_len > 0) {
    size_t str_start = 14;
    size_t str_end = d.size() - 2;  // before crc2 and trailing size byte
    if (str_end > str_start) {
      std::string chunk(d.begin() + str_start, d.begin() + str_end);
      // If this is a continuation of a previous multi-part response, accumulate
      if (this->pending_register_ == register_id && !this->pending_string_data_.empty()) {
        this->pending_string_data_ += chunk;
      } else {
        this->pending_string_data_ = chunk;
        this->pending_register_ = register_id;
      }
      // Only use the final result when response is complete (0x19) or no more data
      if (response_type == 0x19 || next_data == 0x00) {
        str_data = this->pending_string_data_;
        this->pending_string_data_.clear();
        this->pending_register_ = 0;
      }
    }
  }

  if (device_type == DEV_CONTROLLER) {
    switch (register_id) {
      case REG_TYPE_M:
        if (data_len >= 1 && d.size() > 14) {
          this->motor_type_ = d[14];
          ESP_LOGI(TAG, "Motor type: %s (0x%02X)", t4_motor_type_name(this->motor_type_), this->motor_type_);
        }
        break;

      case REG_STATUS:
        if (data_len >= 1 && d.size() > 14) {
          uint8_t old_state = this->operation_state_;
          this->operation_state_ = d[14];
          if (old_state != this->operation_state_) {
            ESP_LOGI(TAG, "Status: %s (0x%02X)", t4_auto_status_name(this->operation_state_), this->operation_state_);
            this->handle_movement_transition_(old_state, this->operation_state_);
            this->notify_state_change_();
          }
        }
        break;

      case REG_STANDBY_ACT:
        if (data_len >= 1 && d.size() > 14) {
          this->standby_ = (d[14] != 0);
          ESP_LOGI(TAG, "Standby: %s", this->standby_ ? "ON" : "OFF");
        }
        break;

      case REG_PEAK_ON:
        if (data_len >= 1 && d.size() > 14) {
          this->peak_ = (d[14] != 0);
          ESP_LOGI(TAG, "Peak mode: %s", this->peak_ ? "ON" : "OFF");
        }
        break;

      case REG_BLINK_ON:
        if (data_len >= 1 && d.size() > 14) {
          this->preflash_ = (d[14] != 0);
          ESP_LOGI(TAG, "Pre-flash: %s", this->preflash_ ? "ON" : "OFF");
        }
        break;

      case REG_MAX_OPN:
      case REG_POS_MAX:
      case REG_POS_MIN:
      case REG_CUR_POS: {
        // Position registers: extract value based on response format
        // data_len=1: 1-byte position (Walky)
        // data_len=2: 2-byte position (d[14]<<8 | d[15])
        // data_len=3: [prefix][position][fine] — position in d[15] (MC800 format)
        uint16_t pos_val = 0;
        bool valid = false;
        if (data_len == 1 && d.size() > 14) {
          pos_val = d[14];
          valid = true;
        } else if (data_len == 3 && d.size() > 16) {
          // 3-byte: d[14]=prefix/flag, d[15]=position, d[16]=fine
          pos_val = d[15];
          valid = true;
        } else if (data_len >= 2 && d.size() > 15) {
          pos_val = (d[14] << 8) | d[15];
          valid = true;
        }
        if (valid) {
          switch (register_id) {
            case REG_MAX_OPN:
              this->pos_open_ = pos_val;
              ESP_LOGI(TAG, "Position open: %u (data_len=%u)", this->pos_open_, data_len);
              break;
            case REG_POS_MAX:
              this->pos_max_ = pos_val;
              ESP_LOGI(TAG, "Position max: %u (data_len=%u)", this->pos_max_, data_len);
              break;
            case REG_POS_MIN:
              this->pos_close_ = pos_val;
              ESP_LOGI(TAG, "Position close (min): %u (data_len=%u)", this->pos_close_, data_len);
              break;
            case REG_CUR_POS:
              this->update_position_(pos_val);
              break;
          }
        }
        break;
      }

      case REG_AUTOCLS:
        if (data_len >= 1 && d.size() > 14) {
          this->auto_close_ = (d[14] != 0);
          ESP_LOGI(TAG, "Auto close: %s", this->auto_close_ ? "ON" : "OFF");
        }
        break;

      case REG_PH_CLS_ON:
        if (data_len >= 1 && d.size() > 14) {
          this->photo_close_ = (d[14] != 0);
          ESP_LOGI(TAG, "Photo close: %s", this->photo_close_ ? "ON" : "OFF");
        }
        break;

      case REG_ALW_CLS_ON:
        if (data_len >= 1 && d.size() > 14) {
          this->always_close_ = (d[14] != 0);
          ESP_LOGI(TAG, "Always close: %s", this->always_close_ ? "ON" : "OFF");
        }
        break;

      case REG_TOTAL_COUNT:
        if (data_len >= 2 && d.size() > 15) {
          this->maneuver_count_ = (d[14] << 8) | d[15];
          ESP_LOGI(TAG, "Maneuver count: %u", this->maneuver_count_);
          if (this->maneuver_count_sensor_ != nullptr)
            this->maneuver_count_sensor_->publish_state(this->maneuver_count_);
        }
        break;

      case REG_MAINT_COUNT:
        if (data_len >= 2 && d.size() > 15) {
          this->maintenance_count_ = (d[14] << 8) | d[15];
          ESP_LOGI(TAG, "Maintenance count: %u", this->maintenance_count_);
          if (this->maintenance_count_sensor_ != nullptr)
            this->maintenance_count_sensor_->publish_state(this->maintenance_count_);
        }
        break;

      case REG_DIAG_IO:
        ESP_LOGV(TAG, "Diagnostic I/O data received (%d bytes)", data_len);
        // Parse limit switch states from data byte at offset 16 (d[14+2] = d[16])
        if (d.size() > 16) {
          uint8_t io_byte = d[16];
          if (this->limit_close_sensor_ != nullptr)
            this->limit_close_sensor_->publish_state((io_byte & 0x01) != 0);
          if (this->limit_open_sensor_ != nullptr)
            this->limit_open_sensor_->publish_state((io_byte & 0x02) != 0);
          if (this->photocell_sensor_ != nullptr)
            this->photocell_sensor_->publish_state((io_byte & 0x04) != 0);
          // Use limit switches to confirm position
          this->confirm_position_from_io_(io_byte);
        }
        break;

      case REG_DIAG_BB:
        // Diagnostic blackbox — last stop reason
        if (data_len >= 1 && d.size() > 14) {
          uint8_t reason = d[14];
          ESP_LOGI(TAG, "Last stop reason: %s (0x%02X)", t4_stop_reason_name(reason), reason);
          if (this->last_stop_reason_text_sensor_ != nullptr)
            this->last_stop_reason_text_sensor_->publish_state(t4_stop_reason_name(reason));
          // Set obstacle sensor if reason indicates obstacle
          if (this->obstacle_sensor_ != nullptr) {
            bool obstacle = (reason == STOP_OBSTACLE_ENCODER || reason == STOP_OBSTACLE_FORCE);
            this->obstacle_sensor_->publish_state(obstacle);
          }
        }
        break;

      case REG_KEY_LOCK:
        if (data_len >= 1 && d.size() > 14) {
          this->lock_state_ = (d[14] != 0);
          ESP_LOGI(TAG, "Key lock: %s", this->lock_state_ ? "LOCKED" : "UNLOCKED");
          if (this->locked_sensor_ != nullptr)
            this->locked_sensor_->publish_state(this->lock_state_);
        }
        break;

      case REG_OP_BLOCK:
        if (data_len >= 1 && d.size() > 14) {
          ESP_LOGI(TAG, "Operator block: %s", (d[14] != 0) ? "BLOCKED" : "FREE");
        }
        break;

      default:
        ESP_LOGD(TAG, "Register 0x%02X (%s): %d bytes", register_id, t4_register_name(register_id), data_len);
        // Dispatch to registered numbers
        // data_len=1: value in d[14], data_len=2: d[14]=prefix, value in d[15]
        if (data_len >= 1 && d.size() > 14) {
          uint8_t val = (data_len >= 2 && d.size() > 15) ? d[15] : d[14];
          for (auto *num : this->numbers_) {
            if (num->get_register_id() == register_id) {
              num->publish_value(static_cast<float>(val));
            }
          }
        }
        break;
    }
  } else if (device_type == DEV_ALL) {
    switch (register_id) {
      case REG_WHO: {
        // WHO response: discover device addresses
        // d[4],d[5] = from address (the device responding)
        if (d.size() >= 6) {
          uint8_t from_row = d[4];
          uint8_t from_addr = d[5];

          if (data_len >= 1 && d.size() > 14) {
            uint8_t dev_type = d[14];
            ESP_LOGD(TAG, "WHO response from 0x%02X%02X: type=0x%02X", from_row, from_addr, dev_type);
            if (dev_type == DEV_CONTROLLER) {
              this->device_address_ = {from_row, from_addr};
              this->device_found_ = true;
              this->discovery_attempts_ = 0;
              ESP_LOGI(TAG, "Discovered controller at 0x%02X%02X", from_row, from_addr);
            } else if (dev_type == DEV_RADIO) {
              this->oxi_address_ = {from_row, from_addr};
              ESP_LOGI(TAG, "Discovered receiver (OXI) at 0x%02X%02X", from_row, from_addr);
              // Initialize OXI
              this->init_device_(from_row, from_addr, DEV_RADIO);
            }
          }
        }
        break;
      }

      case REG_MANUFACTURER:
        if (!str_data.empty()) {
          // Determine if this is from controller or OXI based on source address
          uint8_t from_row = d[4];
          uint8_t from_addr = d[5];
          if (this->device_address_.row == from_row && this->device_address_.address == from_addr) {
            this->manufacturer_ = str_data;
            ESP_LOGI(TAG, "Manufacturer: %s", this->manufacturer_.c_str());
            if (this->manufacturer_text_sensor_ != nullptr)
              this->manufacturer_text_sensor_->publish_state(this->manufacturer_);
          } else {
            this->oxi_manufacturer_ = str_data;
            ESP_LOGI(TAG, "OXI Manufacturer: %s", this->oxi_manufacturer_.c_str());
          }
        }
        break;

      case REG_PRODUCT:
        if (!str_data.empty()) {
          uint8_t from_row = d[4];
          uint8_t from_addr = d[5];
          if (this->device_address_.row == from_row && this->device_address_.address == from_addr) {
            this->product_ = str_data;
            ESP_LOGI(TAG, "Product: %s", this->product_.c_str());
            if (this->product_text_sensor_ != nullptr)
              this->product_text_sensor_->publish_state(this->product_);
            this->detect_device_type_();
          } else {
            this->oxi_product_ = str_data;
            ESP_LOGI(TAG, "OXI Product: %s", this->oxi_product_.c_str());
          }
        }
        break;

      case REG_HARDWARE:
        if (!str_data.empty()) {
          uint8_t from_row = d[4];
          uint8_t from_addr = d[5];
          if (this->device_address_.row == from_row && this->device_address_.address == from_addr) {
            this->hardware_ver_ = str_data;
            ESP_LOGI(TAG, "Hardware: %s", this->hardware_ver_.c_str());
            if (this->hardware_version_text_sensor_ != nullptr)
              this->hardware_version_text_sensor_->publish_state(this->hardware_ver_);
          } else {
            this->oxi_hardware_ver_ = str_data;
            ESP_LOGI(TAG, "OXI Hardware: %s", this->oxi_hardware_ver_.c_str());
          }
        }
        break;

      case REG_FIRMWARE:
        if (!str_data.empty()) {
          uint8_t from_row = d[4];
          uint8_t from_addr = d[5];
          if (this->device_address_.row == from_row && this->device_address_.address == from_addr) {
            this->firmware_ver_ = str_data;
            ESP_LOGI(TAG, "Firmware: %s", this->firmware_ver_.c_str());
            if (this->firmware_version_text_sensor_ != nullptr)
              this->firmware_version_text_sensor_->publish_state(this->firmware_ver_);
          } else {
            this->oxi_firmware_ver_ = str_data;
            ESP_LOGI(TAG, "OXI Firmware: %s", this->oxi_firmware_ver_.c_str());
          }
        }
        break;

      case REG_DESCRIPTION:
        if (!str_data.empty()) {
          uint8_t from_row = d[4];
          uint8_t from_addr = d[5];
          if (this->device_address_.row == from_row && this->device_address_.address == from_addr) {
            this->description_ = str_data;
            ESP_LOGI(TAG, "Description: %s", this->description_.c_str());
          } else {
            this->oxi_description_ = str_data;
            ESP_LOGI(TAG, "OXI Description: %s", this->oxi_description_.c_str());
          }
        }
        break;

      default:
        ESP_LOGD(TAG, "DEV_ALL register 0x%02X: %d bytes", register_id, data_len);
        break;
    }
  }
}

void NiceBidiWiFi::parse_rsp_packet_(const std::vector<uint8_t> &d) {
  if (d.size() < 12) {
    return;
  }

  uint8_t cmd_menu = d[9];
  if (cmd_menu != DEV_CONTROLLER) {
    return;
  }

  uint8_t sub_cmd = d[10];
  uint8_t status_byte = d[11];

  if (sub_cmd == MENU_RUN) {
    // Command acknowledgment or status
    if (status_byte >= 0x80) {
      // ACK — command was received
      ESP_LOGD(TAG, "Command ACK received (0x%02X)", status_byte);
    } else {
      // Movement status
      uint8_t old_state = this->operation_state_;
      switch (status_byte) {
        case STA_OPENING:
        case STA_CLOSING:
        case STA_OPENED:
        case STA_CLOSED:
        case STA_STOPPED:
          this->operation_state_ = status_byte;
          ESP_LOGI(TAG, "RSP status: %s", t4_auto_status_name(status_byte));
          break;
        default:
          ESP_LOGD(TAG, "RSP status byte: 0x%02X", status_byte);
          break;
      }
      if (old_state != this->operation_state_) {
        this->handle_movement_transition_(old_state, this->operation_state_);
        this->notify_state_change_();
      }
    }
  } else if (sub_cmd == MENU_STA) {
    // Movement status with position
    uint8_t old_state = this->operation_state_;

    // Road 400 compatibility: 0x83=opening, 0x84=closing
    if (status_byte == 0x83) {
      this->operation_state_ = STA_OPENING;
    } else if (status_byte == 0x84) {
      this->operation_state_ = STA_CLOSING;
    } else {
      switch (status_byte) {
        case STA_OPENING:
        case STA_CLOSING:
        case STA_OPENED:
        case STA_CLOSED:
        case STA_STOPPED:
          this->operation_state_ = status_byte;
          break;
        default:
          break;
      }
    }

    // Position in bytes 12-13
    if (d.size() > 13) {
      uint16_t raw_pos = (d[12] << 8) | d[13];
      this->update_position_(raw_pos);
    }

    if (old_state != this->operation_state_) {
      ESP_LOGI(TAG, "Movement: %s", t4_auto_status_name(this->operation_state_));
      this->notify_state_change_();
    }
  }
}

void NiceBidiWiFi::parse_cmd_status_(const std::vector<uint8_t> &d) {
  if (d.size() < 12) {
    return;
  }

  uint8_t status_byte = d[11];
  uint8_t old_state = this->operation_state_;

  // Simple status switch for RO600-type controllers
  switch (status_byte) {
    case STA_OPENING:
    case STA_CLOSING:
    case STA_OPENED:
    case STA_CLOSED:
    case STA_STOPPED:
      this->operation_state_ = status_byte;
      ESP_LOGI(TAG, "CMD status: %s", t4_auto_status_name(status_byte));
      break;
    default:
      ESP_LOGD(TAG, "CMD status byte: 0x%02X", status_byte);
      break;
  }

  if (old_state != this->operation_state_) {
    this->notify_state_change_();
  }
}

// --- OXI (Remote Control) Sniffing ---

void NiceBidiWiFi::parse_oxi_packet_(const std::vector<uint8_t> &d) {
  if (d.size() < 15) {
    return;
  }

  uint8_t command = d[10];
  uint8_t subcommand = d[11];
  uint8_t sequence = d[12];

  if (command == OXI_REMOTE_LIST && subcommand == 0x01 && sequence == 0x0A && d.size() >= 23) {
    // Remote control info: extract serial, button, mode
    uint32_t remote_serial = (d[19] << 24) | (d[18] << 16) | (d[17] << 8) | d[16];
    uint8_t remote_button = d[19] >> 4;
    uint8_t remote_command = d[22] >> 4;
    uint8_t remote_mode = d[21] + 1;
    uint8_t press_count = d[20];

    ESP_LOGI(TAG, "OXI remote: serial=%08X button=%u cmd=%u mode=%u presses=%u",
             remote_serial, remote_button, remote_command, remote_mode, press_count);

    if (this->oxi_event_text_sensor_ != nullptr) {
      char buf[64];
      snprintf(buf, sizeof(buf), "remote:%08X btn:%u cmd:%u", remote_serial, remote_button, remote_command);
      this->oxi_event_text_sensor_->publish_state(buf);
    }
  } else if (command == OXI_BUTTON_READ && subcommand == 0x41 && sequence == 0x08 && d.size() >= 18) {
    // Direct button press
    uint8_t button = d[14] >> 4;
    uint32_t remote_serial = (d[14] & 0x0F) | (d[15] << 4) | (d[16] << 12) | (d[17] << 20);

    ESP_LOGI(TAG, "OXI button press: serial=%08X button=%u", remote_serial, button);

    if (this->oxi_event_text_sensor_ != nullptr) {
      char buf[48];
      snprintf(buf, sizeof(buf), "button:%08X btn:%u", remote_serial, button);
      this->oxi_event_text_sensor_->publish_state(buf);
    }
  } else {
    ESP_LOGD(TAG, "OXI packet: cmd=0x%02X sub=0x%02X seq=0x%02X", command, subcommand, sequence);
  }
}

// --- Position & Callbacks ---

void NiceBidiWiFi::update_position_(uint16_t raw_pos) {
  this->pos_current_ = raw_pos;
  this->last_encoder_update_time_ = millis();
  ESP_LOGV(TAG, "Position: %u (%.1f%%)", raw_pos, this->get_position_percent() * 100.0f);
  this->notify_state_change_();
}

float NiceBidiWiFi::get_position_percent() const {
  if (this->pos_open_ == this->pos_close_) {
    return 0.0f;  // Avoid division by zero
  }
  float percent = static_cast<float>(this->pos_current_ - this->pos_close_) /
                  static_cast<float>(this->pos_open_ - this->pos_close_);
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 1.0f) percent = 1.0f;
  return percent;
}

void NiceBidiWiFi::notify_state_change_() {
  // Publish sensor values
  if (this->position_sensor_ != nullptr)
    this->position_sensor_->publish_state(this->get_position_percent() * 100.0f);
  if (this->encoder_sensor_ != nullptr)
    this->encoder_sensor_->publish_state(this->pos_current_);

  // Publish gate status text sensor
  if (this->gate_status_text_sensor_ != nullptr)
    this->gate_status_text_sensor_->publish_state(t4_auto_status_name(this->operation_state_));

  for (auto &callback : this->state_callbacks_) {
    callback();
  }
}

// --- Status LEDs ---
// All LEDs are active-low (GPIO LOW = ON, GPIO HIGH = OFF)

void NiceBidiWiFi::set_led_(int pin, bool on) {
  gpio_set_level(static_cast<gpio_num_t>(pin), on ? 0 : 1);
}

void NiceBidiWiFi::setup_leds_() {
  const int pins[] = {led1_red_pin_, led1_green_pin_, led2_red_pin_, led2_green_pin_, led3_pin_};
  for (int pin : pins) {
    gpio_reset_pin(static_cast<gpio_num_t>(pin));
    gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(pin), 1);  // OFF (active-low)
  }
}

void NiceBidiWiFi::update_leds_() {
  uint32_t now = millis();
  bool blink_fast = (now / 150) % 2;   // ~3.3 Hz
  bool blink_slow = (now / 500) % 2;   // 1 Hz

  // --- LED1: Gate Status ---
  // Green=open, Red=closed, Yellow(both)=moving, Blink red=obstacle/error, Off=unknown
  bool led1r = false, led1g = false;

  if (!this->device_found_) {
    // Not discovered — blink red slowly
    led1r = blink_slow;
  } else {
    switch (this->operation_state_) {
      case STA_OPENED:
        led1g = true;
        break;
      case STA_CLOSED:
        led1r = true;
        break;
      case STA_OPENING:
      case STA_CLOSING:
        // Yellow (both) while moving
        led1r = true;
        led1g = true;
        break;
      case STA_STOPPED:
        // Blink red — stopped mid-travel
        led1r = blink_fast;
        break;
      default:
        // Unknown state — dim red blink
        led1r = blink_slow;
        break;
    }
  }

  // Override: obstacle detected — fast blink red only
  if (this->obstacle_sensor_ != nullptr && this->obstacle_sensor_->state) {
    led1r = blink_fast;
    led1g = false;
  }

  this->set_led_(this->led1_red_pin_, led1r);
  this->set_led_(this->led1_green_pin_, led1g);

  // --- LED2: Connectivity ---
  // Green=WiFi+bus OK, Red=no bus device, Yellow=bus OK but searching, Blink green=bus activity
  bool led2r = false, led2g = false;
  bool bus_active = (now - this->last_bus_activity_time_) < 100;

  if (!this->device_found_) {
    // No device — red, blink on bus activity
    led2r = !bus_active;
    led2g = bus_active;
  } else if (!this->init_complete_) {
    // Discovered but initializing — yellow
    led2r = true;
    led2g = true;
  } else {
    // All good — green, brief flash off on bus activity for visual feedback
    led2g = !bus_active;  // flicker off during TX/RX burst
    if (bus_active) {
      led2r = true;  // brief yellow flash on activity
      led2g = true;
    }
  }

  this->set_led_(this->led2_red_pin_, led2r);
  this->set_led_(this->led2_green_pin_, led2g);

  // --- LED3: WiFi / general status ---
  // Solid=connected, Blink slow=connecting, Blink fast=error/no WiFi
  bool led3 = false;
  if (this->device_found_ && this->init_complete_) {
    led3 = true;  // solid on — all good
  } else if (this->device_found_) {
    led3 = blink_slow;  // initializing
  } else {
    led3 = blink_fast;  // searching for device
  }
  this->set_led_(this->led3_pin_, led3);
}

}  // namespace nice_bidiwifi
}  // namespace esphome
