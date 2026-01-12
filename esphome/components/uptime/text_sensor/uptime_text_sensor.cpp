#include "uptime_text_sensor.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uptime {

static const char *const TAG = "uptime.sensor";

// Cap position to prevent buffer overflow from snprintf return value
inline size_t clamp_buffer_pos(size_t pos, size_t buf_size) { return pos < buf_size ? pos : buf_size - 1; }

static void append_unit(char *buf, size_t buf_size, size_t &pos, const char *separator, unsigned value,
                        const char *label) {
  if (pos > 0) {
    pos += snprintf(buf + pos, buf_size - pos, "%s", separator);
    pos = clamp_buffer_pos(pos, buf_size);
  }
  pos += snprintf(buf + pos, buf_size - pos, "%u%s", value, label);
  pos = clamp_buffer_pos(pos, buf_size);
}

void UptimeTextSensor::setup() {
  this->last_ms_ = millis();
  if (this->last_ms_ < 60 * 1000)
    this->last_ms_ = 0;
  this->update();
}

void UptimeTextSensor::update() {
  auto now = millis();
  // get whole seconds since last update. Note that even if the millis count has overflowed between updates,
  // the difference will still be correct due to the way twos-complement arithmetic works.
  uint32_t delta = now - this->last_ms_;
  this->last_ms_ = now - delta % 1000;  // save remainder for next update
  delta /= 1000;
  this->uptime_ += delta;
  uint32_t uptime = this->uptime_;
  unsigned interval = this->get_update_interval() / 1000;

  // Calculate all time units
  unsigned seconds = uptime % 60;
  uptime /= 60;
  unsigned minutes = uptime % 60;
  uptime /= 60;
  unsigned hours = uptime % 24;
  uptime /= 24;
  unsigned days = uptime;

  // Determine which units to display based on interval thresholds
  bool seconds_enabled = interval < 30;
  bool minutes_enabled = interval < 1800;
  bool hours_enabled = interval < 12 * 3600;

  // Determine which units to show
  bool show_days, show_hours, show_minutes, show_seconds;

  if (this->expand_) {
    // Show all enabled units
    show_days = true;
    show_hours = hours_enabled;
    show_minutes = minutes_enabled;
    show_seconds = seconds_enabled;
  } else {
    // Start with only the smallest enabled unit
    show_seconds = seconds_enabled;
    show_minutes = minutes_enabled && !show_seconds;
    show_hours = hours_enabled && !show_minutes && !show_seconds;
    show_days = !show_hours && !show_minutes && !show_seconds;

    // Add larger non-zero units
    if (days > 0)
      show_days = true;
    if (hours > 0 && hours_enabled)
      show_hours = true;
    if (minutes > 0 && minutes_enabled)
      show_minutes = true;

    // Fill in gaps (e.g., show 0h between 1d and 0m)
    if (show_days && hours_enabled)
      show_hours = true;
    if (show_hours && minutes_enabled)
      show_minutes = true;
  }

  // Build output string on stack
  // Home Assistant max state length is 255 chars + null terminator
  char buf[256];
  size_t pos = 0;
  buf[0] = '\0';  // Initialize for empty case

  if (show_days)
    append_unit(buf, sizeof(buf), pos, this->separator_, days, this->days_text_);
  if (show_hours)
    append_unit(buf, sizeof(buf), pos, this->separator_, hours, this->hours_text_);
  if (show_minutes)
    append_unit(buf, sizeof(buf), pos, this->separator_, minutes, this->minutes_text_);
  if (show_seconds)
    append_unit(buf, sizeof(buf), pos, this->separator_, seconds, this->seconds_text_);

  this->publish_state(buf);
}

float UptimeTextSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void UptimeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Uptime Text Sensor", this); }

}  // namespace uptime
}  // namespace esphome
