#include "nice_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nice_bidiwifi {

static const char *const TAG = "nice_bidiwifi.cover";

void NiceCover::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Nice Cover...");
  this->hub_->add_on_state_callback([this]() { this->on_state_change_(); });
}

void NiceCover::dump_config() {
  LOG_COVER("", "Nice BiDi-WiFi Cover", this);
  ESP_LOGCONFIG(TAG, "  Invert open/close: %s", this->invert_open_close_ ? "YES" : "NO");
}

cover::CoverTraits NiceCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void NiceCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    ESP_LOGD(TAG, "Stop command");
    this->hub_->send_control_cmd(CMD_STOP);
    return;
  }

  auto cmd_open = this->invert_open_close_ ? CMD_CLOSE : CMD_OPEN;
  auto cmd_close = this->invert_open_close_ ? CMD_OPEN : CMD_CLOSE;

  if (call.get_position().has_value()) {
    float target = *call.get_position();

    if (target >= cover::COVER_OPEN) {
      ESP_LOGD(TAG, "Open command");
      this->hub_->send_control_cmd(cmd_open);
    } else if (target <= cover::COVER_CLOSED) {
      ESP_LOGD(TAG, "Close command");
      this->hub_->send_control_cmd(cmd_close);
    } else {
      // Intermediate position: move toward target
      float current = this->position;
      if (target > current) {
        ESP_LOGD(TAG, "Opening toward %.1f%%", target * 100.0f);
        this->hub_->send_control_cmd(cmd_open);
      } else {
        ESP_LOGD(TAG, "Closing toward %.1f%%", target * 100.0f);
        this->hub_->send_control_cmd(cmd_close);
      }
    }
  }
}

void NiceCover::on_state_change_() {
  uint8_t state = this->hub_->get_operation_state();
  float pos = this->hub_->get_position_percent();
  if (this->invert_open_close_) {
    pos = 1.0f - pos;
    if (state == STA_OPENING) {
      state = STA_CLOSING;
    } else if (state == STA_CLOSING) {
      state = STA_OPENING;
    }
  }

  cover::CoverOperation op;
  switch (state) {
    case STA_OPENING:
      op = cover::COVER_OPERATION_OPENING;
      break;
    case STA_CLOSING:
      op = cover::COVER_OPERATION_CLOSING;
      break;
    case STA_OPENED:
    case STA_CLOSED:
    case STA_STOPPED:
    case STA_PART_OPENED:
    default:
      op = cover::COVER_OPERATION_IDLE;
      break;
  }

  bool changed = false;

  if (op != this->last_published_op_) {
    this->last_published_op_ = op;
    this->current_operation = op;
    changed = true;
  }

  // Clamp near-zero to fully closed
  if (pos < CLOSED_THRESHOLD) {
    pos = cover::COVER_CLOSED;
  }

  if (pos != this->last_published_pos_) {
    this->last_published_pos_ = pos;
    this->position = pos;
    changed = true;
  }

  if (changed) {
    this->publish_state();
  }
}

}  // namespace nice_bidiwifi
}  // namespace esphome
