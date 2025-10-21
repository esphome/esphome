#include "mcp4461_output.h"
#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp4461 {

static const char *const TAG = "mcp4461.output";

// public set_level function
void Mcp4461Wiper::set_level(float state) {
  if (!std::isfinite(state)) {
    ESP_LOGW(TAG, "Finite state state value is required.");
    return;
  }
  state = clamp(state, 0.0f, 1.0f);
  if (this->is_inverted()) {
    state = 1.0f - state;
  }
  this->write_state(state);
}

// floats from other components (like light etc.) are passed as "percentage floats"
// this function converts them to the 0 - 256 range used by the MCP4461
void Mcp4461Wiper::write_state(float state) {
  if (this->parent_->set_wiper_level_(this->wiper_, static_cast<uint16_t>(std::roundf(state * 256)))) {
    this->state_ = state;
  }
}

float Mcp4461Wiper::read_state() { return (static_cast<float>(this->parent_->get_wiper_level_(this->wiper_)) / 256.0); }

float Mcp4461Wiper::update_state() {
  this->state_ = this->read_state();
  return this->state_;
}

void Mcp4461Wiper::set_state(bool state) {
  if (state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void Mcp4461Wiper::turn_on() { this->parent_->enable_wiper_(this->wiper_); }

void Mcp4461Wiper::turn_off() { this->parent_->disable_wiper_(this->wiper_); }

void Mcp4461Wiper::increase_wiper() {
  if (this->parent_->increase_wiper_(this->wiper_)) {
    this->state_ = this->update_state();
    ESP_LOGV(TAG, "Increased wiper %u to %u", static_cast<uint8_t>(this->wiper_),
             static_cast<uint16_t>(std::roundf(this->state_ * 256)));
  }
}

void Mcp4461Wiper::decrease_wiper() {
  if (this->parent_->decrease_wiper_(this->wiper_)) {
    this->state_ = this->update_state();
    ESP_LOGV(TAG, "Decreased wiper %u to %u", static_cast<uint8_t>(this->wiper_),
             static_cast<uint16_t>(std::roundf(this->state_ * 256)));
  }
}

void Mcp4461Wiper::enable_terminal(char terminal) { this->parent_->enable_terminal_(this->wiper_, terminal); }

void Mcp4461Wiper::disable_terminal(char terminal) { this->parent_->disable_terminal_(this->wiper_, terminal); }

}  // namespace mcp4461
}  // namespace esphome
