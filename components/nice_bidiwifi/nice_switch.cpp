#include "nice_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nice_bidiwifi {

static const char *const TAG = "nice_bidiwifi.switch";

void NiceSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Nice Switch (register 0x%02X)...", this->register_id_);
  this->hub_->add_on_state_callback([this]() { this->on_state_change_(); });
}

void NiceSwitch::dump_config() {
  LOG_SWITCH("", "Nice BiDi-WiFi Switch", this);
  ESP_LOGCONFIG(TAG, "  Register: 0x%02X", this->register_id_);
}

void NiceSwitch::write_state(bool state) {
  ESP_LOGD(TAG, "Setting register 0x%02X to %s", this->register_id_, state ? "ON" : "OFF");
  std::vector<uint8_t> data = {state ? (uint8_t) 0x01 : (uint8_t) 0x00};
  this->hub_->send_inf_cmd(DEV_CONTROLLER, this->register_id_, RUN_SET, 0x00, data);
  // Request current value to confirm
  this->hub_->send_get_register(this->register_id_);
}

void NiceSwitch::on_state_change_() {
  bool current_state = false;
  switch (this->register_id_) {
    case REG_AUTOCLS:
      current_state = this->hub_->is_auto_close();
      break;
    case REG_PH_CLS_ON:
      current_state = this->hub_->is_photo_close();
      break;
    case REG_ALW_CLS_ON:
      current_state = this->hub_->is_always_close();
      break;
    case REG_STANDBY_ACT:
      current_state = this->hub_->is_standby();
      break;
    case REG_PEAK_ON:
      current_state = this->hub_->is_peak();
      break;
    case REG_BLINK_ON:
      current_state = this->hub_->is_preflash();
      break;
    case REG_KEY_LOCK:
      current_state = this->hub_->is_locked();
      break;
    default:
      return;
  }
  this->publish_state(current_state);
}

}  // namespace nice_bidiwifi
}  // namespace esphome
