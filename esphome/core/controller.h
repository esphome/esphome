#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan_state.h"
#endif
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome {

class Controller {
 public:
  void setup_controller(bool include_internal = false);
#ifdef USE_BINARY_SENSOR
  virtual void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state){};
#endif
#ifdef USE_FAN
  virtual void on_fan_update(fan::FanState *obj){};
#endif
#ifdef USE_LIGHT
  virtual void on_light_update(light::LightState *obj){};
#endif
#ifdef USE_SENSOR
  virtual void on_sensor_update(sensor::Sensor *obj, float state){};
#endif
#ifdef USE_SWITCH
  virtual void on_switch_update(switch_::Switch *obj, bool state){};
#endif
#ifdef USE_COVER
  virtual void on_cover_update(cover::Cover *obj){};
#endif
#ifdef USE_TEXT_SENSOR
  virtual void on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state){};
#endif
#ifdef USE_CLIMATE
  virtual void on_climate_update(climate::Climate *obj){};
#endif
#ifdef USE_NUMBER
  virtual void on_number_update(number::Number *obj, float state){};
#endif
#ifdef USE_SELECT
  virtual void on_select_update(select::Select *obj, const std::string &state){};
#endif
};

}  // namespace esphome
