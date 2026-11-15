#pragma once

#include "esphome/components/cstxxx/cstxxx.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef CST226_BUTTON_COUNT
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"
#endif

namespace esphome::cst226 {

static const uint8_t CST226_REG_STATUS = 0x00;

class CST226Touchscreen final : public cstxxx::CSTTouchscreen {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  bool can_proceed() override { return this->setup_complete_ || this->is_failed(); }
#ifdef CST226_BUTTON_COUNT
  void add_button_binary_sensor(binary_sensor::BinarySensor *sensor) { this->buttons_.push_back(sensor); }
#endif

 protected:
  void continue_setup_();
  void update_button_state_(bool state);

#ifdef CST226_BUTTON_COUNT
  bool button_touched_{};
#endif
  uint16_t chip_id_{};
  uint16_t project_id_{};
#ifdef CST226_BUTTON_COUNT
  StaticVector<binary_sensor::BinarySensor *, CST226_BUTTON_COUNT> buttons_;
#endif
};

}  // namespace esphome::cst226
