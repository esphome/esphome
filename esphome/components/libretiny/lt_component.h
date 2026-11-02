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

namespace esphome::libretiny {

class LTComponent final : public Component {
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
#endif

 public:
  float get_setup_priority() const override;
  void dump_config() override;
};

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
