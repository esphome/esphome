#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome::uptime {

/// Emitted by codegen in PROGMEM once per distinct format. On ESP8266 the labels are PROGMEM strings
/// and expand is read with progmem_read_byte().
struct UptimeFormat {
  const char *days;
  const char *hours;
  const char *minutes;
  const char *seconds;
  const char *separator;
  uint8_t expand;
};

class UptimeTextSensor final : public text_sensor::TextSensor, public PollingComponent {
 public:
  explicit UptimeTextSensor(const UptimeFormat *format) : format_(format) {}
  void update() override;
  /// Publish uptime (in seconds) as text, keeping only the units the update interval can resolve.
  /// For tests; firmware only calls update().
  void publish_uptime(uint32_t uptime);
  void dump_config() override;
  void setup() override;

  float get_setup_priority() const override;

 protected:
  void publish_uptime_(uint32_t uptime);

  const UptimeFormat *format_;
};

}  // namespace esphome::uptime
