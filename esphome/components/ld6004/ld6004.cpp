#include "ld6004.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdio>

namespace esphome::ld6004 {
static const char *const TAG = "ld6004";
static constexpr uint8_t QUERIES[]{
    CMD_QUERY_HOLD_DELAY,      CMD_QUERY_Z_RANGE,     CMD_QUERY_SLEEP_TIME,    CMD_QUERY_DWELL_LIFETIME,
    CMD_QUERY_OUTPUT_INTERVAL, CMD_QUERY_SENSITIVITY, CMD_QUERY_TRIGGER_SPEED, CMD_QUERY_INSTALLATION_MODE,
    CMD_QUERY_WORK_MODE,       CMD_QUERY_P20_MODE,    CMD_QUERY_ZONES};
static constexpr uint16_t REPORTS[]{MSG_HOLD_DELAY,      MSG_Z_RANGE,     MSG_SLEEP_TIME,        MSG_DWELL_LIFETIME,
                                    MSG_OUTPUT_INTERVAL, MSG_SENSITIVITY, MSG_TRIGGER_SPEED,     MSG_INSTALLATION_MODE,
                                    MSG_WORK_MODE,       MSG_P20_MODE,    MSG_INTERFERENCE_ZONES};
void LD6004Component::setup() {
  if (this->out_pin_ != nullptr)
    this->out_pin_->setup();
  this->control(CMD_ENABLE_TARGETS);
  this->refresh();
}
void LD6004Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD6004: stale timeout %u ms, payload capacity %u", unsigned(this->stale_timeout_),
                unsigned(MAX_PAYLOAD));
  LOG_PIN("  OUT Pin: ", this->out_pin_);
}
bool LD6004Component::enqueue_(const Command &command) {
  if (this->queue_.push(command))
    return true;
  ESP_LOGW(TAG, "Command queue full or blocked. Recovery requires a radar restart");
  this->status_set_warning();
  return false;
}
bool LD6004Component::control(uint8_t value, uint16_t report, bool retry) {
  if (value < CMD_GENERATE_INTERFERENCE || value > CMD_QUERY_OUTPUT_INTERVAL)
    return false;
  Command command;
  command.type = MSG_CONTROL;
  command.length = 4;
  command.report = report;
  command.retry = retry;
  write_u32(command.data.data(), value);
  return this->enqueue_(command);
}
void LD6004Component::refresh() { this->refresh_index_ = 0; }
void LD6004Component::recover_commands() {
  this->queue_.recover();
  this->parser_.reset();
  this->refresh();
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
      if (this->handle_frame_(frame, now)) {
        if (this->queue_.reply(frame.id, frame.type, frame.length, now)) {
          this->status_clear_warning();
#ifdef USE_LD6004_SWITCH
          bool value;
          if (this->queue_.consume_output_ack(value) && this->target_output_switch_)
            this->target_output_switch_->publish_state(value);
#endif
        }
      }
    }
  }
  if (this->queue_.failures() != this->last_failures_) {
    this->last_failures_ = this->queue_.failures();
    if (this->queue_.blocked()) {
      ESP_LOGW(TAG, "Replies became ambiguous. Restart radar before recovering commands");
    } else {
      ESP_LOGW(TAG, "Command rejected because its bounds are unknown or invalid");
    }
    this->status_set_warning();
  }
  if (!this->queue_.busy() && !this->queue_.blocked()) {
    if (this->refresh_index_ < sizeof(QUERIES)) {
      const uint8_t i = this->refresh_index_++;
      this->control(QUERIES[i], REPORTS[i]);
    } else if (this->refresh_index_ == sizeof(QUERIES)) {
      Command command;
      command.type = MSG_VERSION;
      command.report = MSG_VERSION;
      this->enqueue_(command);
      this->refresh_index_ = 255;
    }
  }
  if (const Command *command = this->queue_.poll(now)) {
    uint8_t bytes[37];
    this->write_array(bytes, encode(command->id, command->type, command->data.data(), command->length, bytes));
  }
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
#ifdef USE_LD6004_TEXT_SENSOR
  const uint8_t command_status = this->queue_.status();
  if (this->command_status_counter_.changed(command_status) && this->texts_[3]) {
    const char *const labels[]{"Ready", "Busy", "Blocked"};
    this->texts_[3]->publish_state(labels[command_status]);
  }
#endif
  if (this->error_counter_.changed(this->parser_.errors()))
    this->publish_sensor_(123, this->parser_.errors());
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
void LD6004Component::publish_number_(size_t index, float value) {
#ifdef USE_LD6004_NUMBER
  if (this->numbers_[index])
    this->numbers_[index]->publish_state(value);
#endif
}
void LD6004Component::publish_select_(size_t index, uint8_t value) {
#ifdef USE_LD6004_SELECT
  if (this->selects_[index])
    this->selects_[index]->publish_state(size_t(value));
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
  if (length == 0)
    return true;  // Acknowledgements never carry setting values.
  switch (frame.type) {
    case MSG_TARGETS:
      if (!this->tracker_.update(data, length, now))
        return false;
      this->publish_targets_();
      return true;
    case MSG_AREA_PRESENCE:
      for (size_t i = 0; i < 4; ++i)
        this->publish_binary_(11 + i, read_u32(data + 4 * i) != 0);
      this->area_time_ = now;
      this->area_valid_ = true;
      return true;
    case MSG_INTERFERENCE_ZONES:
    case MSG_DETECTION_ZONES:
    case MSG_DWELL_ZONES: {
      const size_t offset = frame.type == MSG_INTERFERENCE_ZONES ? 0 : (frame.type == MSG_DETECTION_ZONES ? 24 : 48);
      for (size_t i = 0; i < 24; ++i)
        this->publish_sensor_(51 + offset + i, read_float(data + 4 * i));
      return true;
    }
    case MSG_HOLD_DELAY:
    case MSG_SLEEP_TIME:
    case MSG_DWELL_LIFETIME:
    case MSG_OUTPUT_INTERVAL: {
      const size_t index = frame.type == MSG_HOLD_DELAY
                               ? 0
                               : (frame.type == MSG_SLEEP_TIME ? 3 : (frame.type == MSG_DWELL_LIFETIME ? 4 : 5));
      const float value = read_u32(data);
      this->publish_number_(index, value);
      return true;
    }
    case MSG_Z_RANGE:
      this->queue_.update_z_range(read_float(data), read_float(data + 4));
      this->publish_number_(1, read_float(data));
      this->publish_number_(2, read_float(data + 4));
      return true;
    case MSG_SENSITIVITY:
    case MSG_TRIGGER_SPEED:
    case MSG_INSTALLATION_MODE:
    case MSG_WORK_MODE:
    case MSG_P20_MODE: {
      const size_t index =
          frame.type == MSG_SENSITIVITY
              ? 0
              : (frame.type == MSG_TRIGGER_SPEED
                     ? 1
                     : (frame.type == MSG_INSTALLATION_MODE ? 2 : (frame.type == MSG_WORK_MODE ? 3 : 4)));
      this->publish_select_(index, data[0]);
      return true;
    }
    case MSG_WORK_STATUS:
#ifdef USE_LD6004_TEXT_SENSOR
      if (this->texts_[2])
        this->texts_[2]->publish_state(data[0] ? "Normal" : "Sleeping");
#endif
      return true;
    case MSG_VERSION:
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
bool LD6004Component::set_number(uint8_t index, float value) {
  if (!std::isfinite(value) || index > 5)
    return false;
  Command command;
  if (index == 1 || index == 2) {
    command.type = MSG_SET_Z_RANGE;
    command.length = 8;
    command.query = CMD_QUERY_Z_RANGE;
    command.report = MSG_Z_RANGE;
    command.z_bound = index;
    command.z_value = value;
    return this->enqueue_(command);
  }
  return make_number_command(index, value, command) && this->enqueue_(command);
}
bool LD6004Component::set_select(uint8_t index, size_t value) {
  Command command;
  return make_select_command(index, value, command) && this->enqueue_(command);
}
bool LD6004Component::set_output(bool value) {
  // Publish receipt acknowledgement only. This protocol has no output-state query.
  return this->control(value ? CMD_ENABLE_TARGETS : CMD_DISABLE_TARGETS);
}
bool LD6004Component::set_zone(uint32_t id, float x_min, float x_max, float y_min, float y_max, float z_min,
                               float z_max) {
  const float values[]{x_min, x_max, y_min, y_max, z_min, z_max};
  if (id > 11)
    return false;
  for (size_t i = 0; i < 6; i += 2) {
    if (!valid_bounds(values[i], values[i + 1]))
      return false;
  }
  Command command;
  command.type = MSG_SET_ZONE;
  command.length = 28;
  command.query = CMD_QUERY_ZONES;
  command.report = id < 4 ? MSG_INTERFERENCE_ZONES : (id < 8 ? MSG_DETECTION_ZONES : MSG_DWELL_ZONES);
  write_u32(command.data.data(), id);
  for (size_t i = 0; i < 6; ++i)
    write_float(command.data.data() + 4 + 4 * i, values[i]);
  return this->enqueue_(command);
}
bool LD6004Component::set_z_range(float minimum, float maximum) {
  if (!valid_bounds(minimum, maximum))
    return false;
  Command command;
  command.type = MSG_SET_Z_RANGE;
  command.length = 8;
  command.query = CMD_QUERY_Z_RANGE;
  command.report = MSG_Z_RANGE;
  write_float(command.data.data(), minimum);
  write_float(command.data.data() + 4, maximum);
  return this->enqueue_(command);
}
}  // namespace esphome::ld6004
