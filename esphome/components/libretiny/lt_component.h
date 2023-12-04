#pragma once

#ifdef USE_LIBRETINY

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace libretiny {

class LTComponent : public Component {
 public:
  float get_setup_priority() const override;
  void dump_config() override;

#ifdef USE_TEXT_SENSOR
  void set_version_sensor(text_sensor::TextSensor *version) { version_ = version; }
#endif  // USE_TEXT_SENSOR

 protected:
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *version_{nullptr};
#endif  // USE_TEXT_SENSOR
};

}  // namespace libretiny
}  // namespace esphome

#endif  // USE_LIBRETINY
