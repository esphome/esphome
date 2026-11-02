#include "uptime_text_sensor.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::uptime {

ESPHOME_LOG_TAG(TAG, "uptime.sensor");

static size_t append_flash_str(char *buf, size_t buf_size, size_t pos, const char *str) {
#ifdef USE_ESP8266
  return buf_append_str_p(buf, buf_size, pos, str);
#else
  return buf_append_str(buf, buf_size, pos, str);
#endif
}

static void append_unit(char *buf, size_t buf_size, size_t &pos, const char *separator, unsigned value,
                        const char *label) {
  if (pos > 0) {
    pos = append_flash_str(buf, buf_size, pos, separator);
  }
  pos = buf_append_printf(buf, buf_size, pos, "%u", value);
  pos = append_flash_str(buf, buf_size, pos, label);
}

void UptimeTextSensor::setup() { this->update(); }

// Inlined into update(), so firmware has no extra function
ESPHOME_ALWAYS_INLINE inline void UptimeTextSensor::publish_uptime_(uint32_t uptime) {
  const UptimeFormat *format = this->format_;
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

  // Show from highest non-zero unit (or all in expand mode) down to smallest enabled
  bool show_days = progmem_read_byte(&format->expand) || days > 0;
  bool show_hours = hours_enabled && (show_days || hours > 0);
  bool show_minutes = minutes_enabled && (show_hours || minutes > 0);
  bool show_seconds = seconds_enabled && (show_minutes || seconds > 0);

  // If nothing shown, show smallest enabled unit
  if (!show_days && !show_hours && !show_minutes && !show_seconds) {
    if (seconds_enabled) {
      show_seconds = true;
    } else if (minutes_enabled) {
      show_minutes = true;
    } else if (hours_enabled) {
      show_hours = true;
    } else {
      show_days = true;
    }
  }

  // Build output string on stack
  // Home Assistant max state length is 255 chars + null terminator
  char buf[256];
  size_t pos = 0;
  buf[0] = '\0';  // Initialize for empty case

  if (show_days)
    append_unit(buf, sizeof(buf), pos, format->separator, days, format->days);
  if (show_hours)
    append_unit(buf, sizeof(buf), pos, format->separator, hours, format->hours);
  if (show_minutes)
    append_unit(buf, sizeof(buf), pos, format->separator, minutes, format->minutes);
  if (show_seconds)
    append_unit(buf, sizeof(buf), pos, format->separator, seconds, format->seconds);

  this->publish_state(buf, pos);
}

void UptimeTextSensor::update() { this->publish_uptime_(static_cast<uint32_t>(millis_64() / 1000)); }
void UptimeTextSensor::publish_uptime(uint32_t uptime) { this->publish_uptime_(uptime); }

float UptimeTextSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void UptimeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Uptime Text Sensor", this); }

}  // namespace esphome::uptime
