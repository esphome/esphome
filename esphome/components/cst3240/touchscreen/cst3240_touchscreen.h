#pragma once

#include "esphome/components/cstxxx/cstxxx.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::cst3240 {

static constexpr uint16_t CST3240_CHIP_ID = 0x3240;
static constexpr uint8_t CST3240_MAX_TOUCHES = 5;
static constexpr uint8_t CST3240_REPORT_SIZE = cstxxx::report_size(CST3240_MAX_TOUCHES);
static constexpr uint8_t TOUCH_EVENT_PRESSED = 0x06;

class CST3240Touchscreen : public cstxxx::CSTTouchscreen {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void update_touches() override;
  void continue_setup_();
  /// Zeroes the touch report block, which polling relies on to tell new reports from old.
  void clear_report_();

  uint16_t chip_id_{};
  uint16_t project_id_{};
  uint16_t fw_version_{};
  uint16_t fw_build_{};
};

}  // namespace esphome::cst3240
