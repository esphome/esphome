#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace sun_gtil2 {

class SunGTIL2 : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;

#ifdef USE_SENSOR
  void set_ac_voltage(sensor::Sensor *sensor) { ac_voltage_ = sensor; }
  void set_dc_voltage(sensor::Sensor *sensor) { dc_voltage_ = sensor; }
  void set_ac_power(sensor::Sensor *sensor) { ac_power_ = sensor; }
  void set_dc_power(sensor::Sensor *sensor) { dc_power_ = sensor; }
  void set_limiter_power(sensor::Sensor *sensor) { limiter_power_ = sensor; }
  void set_temperature(sensor::Sensor *sensor) { temperature_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_state(text_sensor::TextSensor *text_sensor) { state_ = text_sensor; }
  void set_serial_number(text_sensor::TextSensor *text_sensor) { serial_number_ = text_sensor; }
#endif

 protected:
  std::string state_to_string_(uint8_t state);
#ifdef USE_SENSOR
  sensor::Sensor *ac_voltage_{nullptr};
  sensor::Sensor *dc_voltage_{nullptr};
  sensor::Sensor *ac_power_{nullptr};
  sensor::Sensor *dc_power_{nullptr};
  sensor::Sensor *limiter_power_{nullptr};
  sensor::Sensor *temperature_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *state_{nullptr};
  text_sensor::TextSensor *serial_number_{nullptr};
#endif

  float calculate_temperature_(uint16_t adc_value);
  void handle_char_(uint8_t c);
  std::vector<uint8_t> rx_message_;
};

}  // namespace sun_gtil2
}  // namespace esphome
