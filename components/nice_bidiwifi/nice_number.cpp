#include "nice_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nice_bidiwifi {

static const char *const TAG = "nice_bidiwifi.number";

void NiceNumber::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Nice Number (register 0x%02X)...", this->register_id_);
  this->hub_->register_number(this);
  // Request current value from the device
  this->hub_->send_get_register(this->register_id_);
}

void NiceNumber::dump_config() {
  LOG_NUMBER("", "Nice BiDi-WiFi Number", this);
  ESP_LOGCONFIG(TAG, "  Register: 0x%02X", this->register_id_);
}

void NiceNumber::control(float value) {
  ESP_LOGD(TAG, "Setting register 0x%02X to %.0f", this->register_id_, value);
  uint8_t byte_val = static_cast<uint8_t>(value);
  // Send as 2-byte data: [prefix=0x01, value] — matches Nice app format
  std::vector<uint8_t> data = {0x01, byte_val};
  this->hub_->send_inf_cmd(DEV_CONTROLLER, this->register_id_, RUN_SET, 0x00, data);
  // Request current value to confirm
  this->hub_->send_get_register(this->register_id_);
}

}  // namespace nice_bidiwifi
}  // namespace esphome
