#pragma once

#include "esphome/components/cstxxx/cstxxx.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::cst9220 {

ESPHOME_LOG_TAG(TAG, "cst9220.touchscreen");

static constexpr uint8_t TOUCH_EVENT_DOWN = 0x06;

static constexpr uint16_t CST9220_CHIP_ID = 0x9220;
static constexpr uint16_t CST9217_CHIP_ID = 0x9217;

static constexpr uint8_t CST9220_MAX_TOUCHES = 5;
// Report layout: 5 bytes per touch point plus 5 bytes of status/ack overhead.
static constexpr size_t CST9220_DATA_LENGTH = CST9220_MAX_TOUCHES * 5 + 5;

class CST9220Touchscreen : public cstxxx::CSTTouchscreen {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void update_touches() override;
  void continue_setup_();

  uint16_t chip_id_{};
  uint16_t project_id_{};
};

}  // namespace esphome::cst9220
