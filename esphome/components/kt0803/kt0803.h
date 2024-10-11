#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "kt0803defs.h"

namespace esphome {
namespace kt0803 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

class KT0803Component : public PollingComponent, public i2c::I2CDevice {
  ChipId chip_id_;  // no way to detect it
  bool reset_;
  union {
    struct KT0803State state_;
    uint8_t regs_[sizeof(struct KT0803State)];
  };

  bool check_reg_(uint8_t addr);
  void write_reg_(uint8_t addr);
  bool read_reg_(uint8_t addr);

  SUB_BINARY_SENSOR(pw_ok)
  SUB_BINARY_SENSOR(slncid)
  SUB_NUMBER(frequency)

  void publish_pw_ok();
  void publish_slncid();
  void publish_frequency();

  void publish(sensor::Sensor *s, float state);
  void publish(binary_sensor::BinarySensor *s, bool state);
  void publish(text_sensor::TextSensor *s, const std::string &state);
  void publish(number::Number *n, float state);
  void publish(switch_::Switch *s, bool state);
  void publish(select::Select *s, size_t index);
  void publish(text::Text *t, const std::string &state);

 public:
  KT0803Component();

  // float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_chip_id(ChipId value);
  ChipId get_chip_id();
  std::string get_chip_string() const;

  void set_frequency(float value);  // MHz
  float get_frequency();
};

}  // namespace kt0803
}  // namespace esphome
