#include "nice_cover.h"
#include "t4_protocol.h"
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
  ESP_LOGCONFIG(TAG, "  Supports position: %s", this->supports_position_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Auto-learn timing: %s", this->auto_learn_timing_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Default open duration: %u ms", this->open_duration_ms_);
  ESP_LOGCONFIG(TAG, "  Default close duration: %u ms", this->close_duration_ms_);
  ESP_LOGCONFIG(TAG, "  Position poll interval: %u ms", this->position_poll_interval_ms_);
}

cover::CoverTraits NiceCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(this->supports_position_);
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
  uint8_t raw_state = this->hub_->get_operation_state();
  float pos = this->hub_->get_position_percent();

  // Encoder often stays at 0% on swing gates while the leaf is physically open. Home Assistant
  // disables "Close" when position is 0, so align reported % with motor limit states.
  if (raw_state == STA_OPENED) {
    pos = this->invert_open_close_ ? cover::COVER_CLOSED : cover::COVER_OPEN;
  } else if (raw_state == STA_CLOSED) {
    pos = this->invert_open_close_ ? cover::COVER_OPEN : cover::COVER_CLOSED;
  } else if (this->invert_open_close_) {
    pos = 1.0f - pos;
  }

  uint8_t state = raw_state;
  if (this->invert_open_close_) {
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
