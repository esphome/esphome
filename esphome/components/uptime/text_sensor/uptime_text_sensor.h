#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uptime {

class UptimeTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  UptimeTextSensor(const char *days_text, const char *hours_text, const char *minutes_text, const char *seconds_text,
                   const char *separator, bool expand)
      : days_text_(days_text),
        hours_text_(hours_text),
        minutes_text_(minutes_text),
        seconds_text_(seconds_text),
        separator_(separator),
        expand_(expand) {}
  void update() override;
  void dump_config() override;
  void setup() override;

  float get_setup_priority() const override;
  void set_days(const char *days_text) { this->days_text_ = days_text; }
  void set_hours(const char *hours_text) { this->hours_text_ = hours_text; }
  void set_minutes(const char *minutes_text) { this->minutes_text_ = minutes_text; }
  void set_seconds(const char *seconds_text) { this->seconds_text_ = seconds_text; }

 protected:
  void insert_buffer_(std::string &buffer, const char *key, unsigned value) const;
  const char *days_text_;
  const char *hours_text_;
  const char *minutes_text_;
  const char *seconds_text_;
  const char *separator_;
  bool expand_{};
  uint32_t uptime_{0};  // uptime in seconds, will overflow after 136 years
  uint32_t last_ms_{0};
};

}  // namespace uptime
}  // namespace esphome
