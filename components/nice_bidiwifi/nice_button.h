#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "nice_bidiwifi.h"

namespace esphome {
namespace nice_bidiwifi {

class NiceButton : public button::Button, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_hub(NiceBidiWiFi *hub) { this->hub_ = hub; }
  void set_command(uint8_t cmd) { this->command_ = cmd; }

 protected:
  void press_action() override;

  NiceBidiWiFi *hub_{nullptr};
  uint8_t command_{0};
};

}  // namespace nice_bidiwifi
}  // namespace esphome
