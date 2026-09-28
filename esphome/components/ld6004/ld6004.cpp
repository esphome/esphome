#include "ld6004.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdio>

namespace esphome::ld6004 {
static const char *const TAG = "ld6004";
void LD6004Component::setup() {
  if (this->out_pin_ != nullptr)
    this->out_pin_->setup();
}
void LD6004Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD6004: stale timeout %u ms, payload capacity %u", unsigned(this->stale_timeout_),
                unsigned(MAX_PAYLOAD));
  LOG_PIN("  OUT Pin: ", this->out_pin_);
}
void LD6004Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  // Bound work even if a continuous report stream fills the UART.
  for (size_t budget = 0; budget < 256 && this->available(); ++budget) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    if (this->parser_.feed(byte, now)) {
      const Frame &frame = this->parser_.frame();
      this->handle_frame_(frame, now);
    }
  }
  const uint8_t requests = this->startup_.poll(now);
  uint8_t bytes[13];
  if (requests & Startup::TARGETS) {
    const uint8_t enable[]{CMD_ENABLE_TARGETS, 0, 0, 0};
    this->write_array(bytes, encode(1, MSG_CONTROL, enable, sizeof(enable), bytes));
  }
  if (requests & Startup::VERSION)
    this->write_array(bytes, encode(2, MSG_VERSION, nullptr, 0, bytes));
  if (this->tracker_.expire(now, this->stale_timeout_))
    this->publish_targets_();
#ifdef USE_LD6004_BINARY_SENSOR
  if (this->area_valid_ && now - this->area_time_ > this->stale_timeout_) {
    this->area_valid_ = false;
    for (size_t i = 11; i < 15; ++i) {
      if (this->binary_[i])
        this->binary_[i]->invalidate_state();
    }
  }
#endif
  if (this->out_pin_)
    this->publish_binary_(15, this->out_pin_->digital_read());
  if (this->error_counter_.changed(this->parser_.errors()))
    this->publish_sensor_(51, this->parser_.errors());
}
void LD6004Component::publish_sensor_(size_t index, float value) {
#ifdef USE_LD6004_SENSOR
  if (this->sensors_[index])
    this->sensors_[index]->publish_state(value);
#endif
}
void LD6004Component::publish_binary_(size_t index, bool value) {
#ifdef USE_LD6004_BINARY_SENSOR
  if (this->binary_[index])
    this->binary_[index]->publish_state(value);
#endif
}
void LD6004Component::publish_targets_() {
  const bool valid = this->tracker_.valid();
  this->publish_sensor_(0, valid ? this->tracker_.count() : NAN);
  for (size_t i = 0; i < MAX_TARGETS; ++i) {
    const Target &target = this->tracker_.slots()[i];
    const bool present = valid && target.active;
    const float values[]{target.x, target.y, target.z, float(target.doppler), float(target.id)};
    for (size_t j = 0; j < 5; ++j)
      this->publish_sensor_(1 + 5 * i + j, present ? values[j] : NAN);
    if (valid)
      this->publish_binary_(i + 1, present);
  }
#ifdef USE_LD6004_BINARY_SENSOR
  if (valid) {
    this->publish_binary_(0, this->tracker_.count() != 0);
  } else {
    for (size_t i = 0; i < 11; ++i) {
      if (this->binary_[i])
        this->binary_[i]->invalidate_state();
    }
  }
#endif
}
bool LD6004Component::handle_frame_(const Frame &frame, uint32_t now) {
  const uint8_t *data = frame.data.data();
  const size_t length = frame.length;
  ESP_LOGV(TAG, "RX id=%u type=0x%04X length=%u", frame.id, frame.type, unsigned(length));
  if (!valid_report(frame)) {
    ESP_LOGV(TAG, "Unknown or malformed report");
    return false;
  }
  switch (frame.type) {
    case MSG_TARGETS:
      if (!this->tracker_.update(data, length, now))
        return false;
      this->startup_.received(Startup::TARGETS);
      this->publish_targets_();
      return true;
    case MSG_AREA_PRESENCE:
      for (size_t i = 0; i < 4; ++i)
        this->publish_binary_(11 + i, read_u32(data + 4 * i) != 0);
      this->area_time_ = now;
      this->area_valid_ = true;
      return true;
    case MSG_WORK_STATUS:
#ifdef USE_LD6004_TEXT_SENSOR
      if (this->texts_[2])
        this->texts_[2]->publish_state(data[0] ? "Normal" : "Sleeping");
#endif
      return true;
    case MSG_VERSION:
      this->startup_.received(Startup::VERSION);
#ifdef USE_LD6004_TEXT_SENSOR
      char text[12];
      if (this->texts_[0]) {
        snprintf(text, sizeof(text), "%u", data[0]);
        this->texts_[0]->publish_state(text);
      }
      if (this->texts_[1]) {
        snprintf(text, sizeof(text), "%u.%u.%u", data[1], data[2], data[3]);
        this->texts_[1]->publish_state(text);
      }
#endif
      return true;
    default:
      return false;
  }
}
}  // namespace esphome::ld6004
