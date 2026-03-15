#include "nice_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nice_bidiwifi {

static const char *const TAG = "nice_bidiwifi.button";

void NiceButton::dump_config() {
  LOG_BUTTON("", "Nice BiDi-WiFi Button", this);
  ESP_LOGCONFIG(TAG, "  Command: 0x%02X (%s)", this->command_,
                t4_control_cmd_name(this->command_));
}

void NiceButton::press_action() {
  ESP_LOGD(TAG, "Button pressed, sending command 0x%02X (%s)",
           this->command_, t4_control_cmd_name(this->command_));
  this->hub_->send_control_cmd(this->command_);
}

}  // namespace nice_bidiwifi
}  // namespace esphome
