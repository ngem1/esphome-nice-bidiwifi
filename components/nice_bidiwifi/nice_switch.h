#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "nice_bidiwifi.h"

namespace esphome {
namespace nice_bidiwifi {

class NiceSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_hub(NiceBidiWiFi *hub) { this->hub_ = hub; }
  void set_register_id(uint8_t reg) { this->register_id_ = reg; }

 protected:
  void write_state(bool state) override;
  void on_state_change_();

  NiceBidiWiFi *hub_{nullptr};
  uint8_t register_id_{0};
};

}  // namespace nice_bidiwifi
}  // namespace esphome
