#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "nice_bidiwifi.h"

namespace esphome {
namespace nice_bidiwifi {

class NiceNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_hub(NiceBidiWiFi *hub) { this->hub_ = hub; }
  void set_register_id(uint8_t reg) { this->register_id_ = reg; }

  uint8_t get_register_id() const { return this->register_id_; }

  /// Called by hub when a GET response is received for this register.
  void publish_value(float value) { this->publish_state(value); }

 protected:
  void control(float value) override;

  NiceBidiWiFi *hub_{nullptr};
  uint8_t register_id_{0};
};

}  // namespace nice_bidiwifi
}  // namespace esphome
