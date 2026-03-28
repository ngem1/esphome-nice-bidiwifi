#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "nice_bidiwifi.h"

namespace esphome {
namespace nice_bidiwifi {

class NiceCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_hub(NiceBidiWiFi *hub) { this->hub_ = hub; }
  void set_invert_open_close(bool invert) { this->invert_open_close_ = invert; }
  void set_supports_position(bool sp) { this->supports_position_ = sp; }

  cover::CoverTraits get_traits() override;
  void control(const cover::CoverCall &call) override;

 protected:
  void on_state_change_();

  NiceBidiWiFi *hub_{nullptr};
  bool invert_open_close_{false};
  bool supports_position_{true};
  float last_published_pos_{-1.0f};
  cover::CoverOperation last_published_op_{cover::COVER_OPERATION_IDLE};

  static constexpr float CLOSED_THRESHOLD = 0.007f;
};

}  // namespace nice_bidiwifi
}  // namespace esphome
