#pragma once

#include "esphome/core/defines.h"
#ifdef USE_VBUS_WRITE_RESULT
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::vbus {

class VBusWriteResultTextSensor final : public text_sensor::TextSensor {
 public:
  explicit VBusWriteResultTextSensor([[maybe_unused]] bool has_filters = false) {
    this->state.reserve(144);
#ifdef USE_TEXT_SENSOR_FILTER
    if (has_filters)
      this->raw_state_.reserve(144);
#endif
  }
};

}  // namespace esphome::vbus
#endif
