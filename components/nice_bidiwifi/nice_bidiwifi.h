#pragma once

#include <queue>
#include <vector>
#include <string>
#include <functional>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "t4_protocol.h"
#include "t4_bus.h"

namespace esphome {
namespace nice_bidiwifi {

// Forward declarations
class NiceCover;
class NiceNumber;

class NiceBidiWiFi : public Component {
 public:
  // ESPHome Component interface
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Config setters
  void set_uart_rx_pin(int pin) { this->rx_pin_ = pin; }
  void set_uart_tx_pin(int pin) { this->tx_pin_ = pin; }
  void set_led1_red_pin(int pin) { this->led1_red_pin_ = pin; }
  void set_led1_green_pin(int pin) { this->led1_green_pin_ = pin; }
  void set_led2_red_pin(int pin) { this->led2_red_pin_ = pin; }
  void set_led2_green_pin(int pin) { this->led2_green_pin_ = pin; }
  void set_led3_pin(int pin) { this->led3_pin_ = pin; }
  void set_hub_address(uint16_t address) {
    this->hub_address_.row = (address >> 8) & 0xFF;
    this->hub_address_.address = address & 0xFF;
  }

  // Entity registration
  void register_cover(NiceCover *cover) { this->cover_ = cover; }
  void register_number(NiceNumber *num) { this->numbers_.push_back(num); }

  // Sensor setters
  void set_position_sensor(sensor::Sensor *s) { this->position_sensor_ = s; }
  void set_encoder_sensor(sensor::Sensor *s) { this->encoder_sensor_ = s; }
  void set_maneuver_count_sensor(sensor::Sensor *s) { this->maneuver_count_sensor_ = s; }
  void set_maintenance_count_sensor(sensor::Sensor *s) { this->maintenance_count_sensor_ = s; }
  void set_maintenance_threshold_sensor(sensor::Sensor *s) { this->maintenance_threshold_sensor_ = s; }

  // Text sensor setters
  void set_product_text_sensor(text_sensor::TextSensor *s) { this->product_text_sensor_ = s; }
  void set_firmware_version_text_sensor(text_sensor::TextSensor *s) { this->firmware_version_text_sensor_ = s; }
  void set_hardware_version_text_sensor(text_sensor::TextSensor *s) { this->hardware_version_text_sensor_ = s; }
  void set_gate_status_text_sensor(text_sensor::TextSensor *s) { this->gate_status_text_sensor_ = s; }
  void set_manufacturer_text_sensor(text_sensor::TextSensor *s) { this->manufacturer_text_sensor_ = s; }

  // Binary sensor setters
  void set_photocell_sensor(binary_sensor::BinarySensor *s) { this->photocell_sensor_ = s; }
  void set_limit_open_sensor(binary_sensor::BinarySensor *s) { this->limit_open_sensor_ = s; }
  void set_limit_close_sensor(binary_sensor::BinarySensor *s) { this->limit_close_sensor_ = s; }

  // Command API
  void send_control_cmd(uint8_t cmd);
  void send_inf_cmd(uint8_t device_type, uint8_t register_id, uint8_t run_cmd,
                    uint8_t next_data = 0x00, const std::vector<uint8_t> &data = {});
  void send_set_register(uint8_t register_id, const std::vector<uint8_t> &data);
  void send_get_register(uint8_t register_id);
  void send_raw(const std::vector<uint8_t> &data);
  /// Parse hex (ignore spaces/dots) and queue raw frame bytes, e.g. "55.0C..." or "550C00FF".
  void send_raw_cmd(const std::string &hex_input);

  // State accessors
  bool is_device_found() const { return this->device_found_; }
  uint8_t get_motor_type() const { return this->motor_type_; }

  const std::string &get_product() const { return this->product_; }
  const std::string &get_manufacturer() const { return this->manufacturer_; }
  const std::string &get_hardware_version() const { return this->hardware_ver_; }
  const std::string &get_firmware_version() const { return this->firmware_ver_; }
  const std::string &get_description() const { return this->description_; }

  uint16_t get_current_position() const { return this->pos_current_; }
  uint16_t get_position_open() const { return this->pos_open_; }
  uint16_t get_position_close() const { return this->pos_close_; }
  uint16_t get_position_max() const { return this->pos_max_; }
  float get_position_percent() const;

  uint8_t get_operation_state() const { return this->operation_state_; }

  bool is_auto_close() const { return this->auto_close_; }
  bool is_photo_close() const { return this->photo_close_; }
  bool is_always_close() const { return this->always_close_; }
  bool is_standby() const { return this->standby_; }
  bool is_preflash() const { return this->preflash_; }
  bool is_locked() const { return this->lock_state_; }

  // Additional binary sensor setters
  void set_obstacle_sensor(binary_sensor::BinarySensor *s) { this->obstacle_sensor_ = s; }
  void set_locked_sensor(binary_sensor::BinarySensor *s) { this->locked_sensor_ = s; }
  void set_input_1_sensor(binary_sensor::BinarySensor *s) { this->input_1_sensor_ = s; }
  void set_input_2_sensor(binary_sensor::BinarySensor *s) { this->input_2_sensor_ = s; }
  void set_input_3_sensor(binary_sensor::BinarySensor *s) { this->input_3_sensor_ = s; }
  void set_input_4_sensor(binary_sensor::BinarySensor *s) { this->input_4_sensor_ = s; }

  // Additional text sensor setters
  void set_oxi_event_text_sensor(text_sensor::TextSensor *s) { this->oxi_event_text_sensor_ = s; }
  void set_last_stop_reason_text_sensor(text_sensor::TextSensor *s) { this->last_stop_reason_text_sensor_ = s; }
  void set_diag_par_text_sensor(text_sensor::TextSensor *s) { this->diag_par_text_sensor_ = s; }

  // Callback
  void add_on_state_callback(std::function<void()> &&callback) {
    this->state_callbacks_.push_back(std::move(callback));
  }

 protected:
  // Config
  int rx_pin_{18};
  int tx_pin_{21};
  int led1_red_pin_{25};
  int led1_green_pin_{2};
  int led2_red_pin_{27};
  int led2_green_pin_{26};
  int led3_pin_{22};
  T4Address hub_address_{0x50, 0x90};

  // Bus driver
  T4Bus bus_;

  // Device state
  bool device_found_{false};
  T4Address device_address_{0x00, 0x00};
  T4Address oxi_address_{0x00, 0x00};
  uint8_t motor_type_{0};

  // Info strings (controller)
  std::string manufacturer_;
  std::string product_;
  std::string hardware_ver_;
  std::string firmware_ver_;
  std::string description_;

  // Info strings (OXI/receiver)
  std::string oxi_manufacturer_;
  std::string oxi_product_;
  std::string oxi_hardware_ver_;
  std::string oxi_firmware_ver_;
  std::string oxi_description_;

  // Position
  uint16_t pos_max_{0};
  uint16_t pos_open_{2048};
  uint16_t pos_close_{0};
  uint16_t pos_current_{0};

  // Operation state
  uint8_t operation_state_{STA_STOPPED};
  bool auto_close_{false};
  bool photo_close_{false};
  bool always_close_{false};
  bool standby_{false};
  bool preflash_{false};

  // Device type flags (auto-detected from product name)
  bool is_walky_{false};
  bool is_robus_{false};

  // Time-based position estimation
  uint32_t movement_start_time_{0};
  float position_at_movement_start_{0.0f};
  uint32_t last_open_duration_ms_{0};
  uint32_t last_close_duration_ms_{0};
  uint32_t last_encoder_update_time_{0};

  // Status confirmation
  bool awaiting_confirmation_{false};

  // Multi-part response accumulation
  uint8_t pending_register_{0};
  std::string pending_string_data_;
  bool lock_state_{false};

  // Counters
  uint32_t maneuver_count_{0};
  uint32_t maintenance_count_{0};
  /// Set when REG_TOTAL_COUNT (0xB3) returns 0xFD — refresh uses REG_NUM_MOVEMENTS (0xD4) instead.
  bool maneuver_total_count_unsupported_{false};
  /// Set when REG_NUM_MOVEMENTS (0xD4) also returns 0xFD — skip maneuver refresh GETs.
  bool maneuver_num_movements_unsupported_{false};
  /// Logical inputs not exposed on some boards (e.g. CL201): stop polling after first NACK.
  bool input_3_unsupported_{false};
  bool input_4_unsupported_{false};
  // Timing
  uint32_t last_discovery_time_{0};
  uint32_t last_position_poll_time_{0};
  uint32_t last_status_refresh_time_{0};
  uint32_t last_io_poll_time_{0};
  bool init_complete_{false};
  uint8_t discovery_attempts_{0};
  uint32_t last_led_update_time_{0};
  uint32_t last_bus_activity_time_{0};

  // Persistence for learned durations
  ESPPreferenceObject timing_pref_;

  // TX queue
  std::queue<std::vector<uint8_t>> tx_queue_;

  // Entity pointers
  NiceCover *cover_{nullptr};
  std::vector<NiceNumber *> numbers_;

  // Sensor pointers
  sensor::Sensor *position_sensor_{nullptr};
  sensor::Sensor *encoder_sensor_{nullptr};
  sensor::Sensor *maneuver_count_sensor_{nullptr};
  sensor::Sensor *maintenance_count_sensor_{nullptr};
  sensor::Sensor *maintenance_threshold_sensor_{nullptr};

  // Text sensor pointers
  text_sensor::TextSensor *product_text_sensor_{nullptr};
  text_sensor::TextSensor *firmware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *hardware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *gate_status_text_sensor_{nullptr};
  text_sensor::TextSensor *manufacturer_text_sensor_{nullptr};
  text_sensor::TextSensor *oxi_event_text_sensor_{nullptr};

  // Binary sensor pointers
  binary_sensor::BinarySensor *photocell_sensor_{nullptr};
  binary_sensor::BinarySensor *limit_open_sensor_{nullptr};
  binary_sensor::BinarySensor *limit_close_sensor_{nullptr};
  binary_sensor::BinarySensor *obstacle_sensor_{nullptr};
  binary_sensor::BinarySensor *locked_sensor_{nullptr};
  binary_sensor::BinarySensor *input_1_sensor_{nullptr};
  binary_sensor::BinarySensor *input_2_sensor_{nullptr};
  binary_sensor::BinarySensor *input_3_sensor_{nullptr};
  binary_sensor::BinarySensor *input_4_sensor_{nullptr};

  // Additional text sensor pointers
  text_sensor::TextSensor *last_stop_reason_text_sensor_{nullptr};
  text_sensor::TextSensor *diag_par_text_sensor_{nullptr};

  // Callbacks
  std::vector<std::function<void()>> state_callbacks_;

  // Persistence struct
  struct LearnedTimings {
    uint32_t open_duration;
    uint32_t close_duration;
    bool valid;
  };

  // Internal methods
  void discover_devices_();
  uint32_t get_discovery_interval_() const;
  void init_device_(uint8_t addr1, uint8_t addr2, uint8_t device_type);
  void request_position_();
  void request_status_refresh_();
  void request_io_state_();
  void request_logical_inputs_();
  bool any_logical_input_sensor_() const;
  void request_status_confirmation_();
  void detect_device_type_();
  void estimate_time_based_position_();
  void save_learned_timings_();
  void load_learned_timings_();
  void apply_cover_timing_fallbacks_();
  uint32_t effective_position_poll_ms_() const;
  /// When pos_open_ == pos_close_ (NACK / unsupported limits), use 2048/0 so % and time-estimate still work.
  void get_effective_pos_limits_(uint16_t *out_open, uint16_t *out_close) const;
  void handle_movement_transition_(uint8_t old_state, uint8_t new_state);
  void confirm_position_from_io_(uint8_t io_byte);
  void parse_packet_(const T4RxPacket &packet);
  void parse_evt_packet_(const std::vector<uint8_t> &data);
  void parse_rsp_packet_(const std::vector<uint8_t> &data);
  void parse_cmd_status_(const std::vector<uint8_t> &data);
  void parse_oxi_packet_(const std::vector<uint8_t> &data);
  void request_continuation_(uint8_t device_type, uint8_t register_id, uint8_t next_offset);
  void update_position_(uint16_t raw_pos);
  void update_maneuver_count_from_bytes_(uint8_t data_len, size_t offset, const std::vector<uint8_t> &d);
  void notify_state_change_();
  void setup_leds_();
  void update_leds_();
  void set_led_(int pin, bool on);
};

}  // namespace nice_bidiwifi
}  // namespace esphome
