#pragma once

#include "esphome/components/cstxxx/cstxxx.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <vector>

namespace esphome::cst328 {

static constexpr uint8_t CST328_TOUCH_MAX_POINTS = 5;
static constexpr uint8_t CST328_TOUCH_DATA_SIZE = cstxxx::report_size(CST328_TOUCH_MAX_POINTS);

static constexpr uint16_t CST_REG_TOUCH_INFORMATION = cstxxx::REG_TOUCH_DATA;
static constexpr uint16_t CST_REG_FINGER_COUNT_IDX = cstxxx::FINGER_COUNT_IDX;
static constexpr uint16_t CST_REG_X_Y_RESOLUTION = cstxxx::REG_RESOLUTION;
static constexpr uint16_t CST_REG_FW_CRC_AND_BOOT_TIME = cstxxx::REG_FW_CHECK_CODE;

class CST328ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST328Touchscreen : public cstxxx::CSTTouchscreen {
 public:
  void setup() override;
  void register_button_listener(CST328ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

 protected:
  void update_touches() override;
  void reset_device_();
  void continue_setup_();
  void update_button_state_(bool state);

  bool button_touched_{};
  uint8_t fw_ver_major_{};
  uint8_t fw_ver_minor_{};
  std::vector<CST328ButtonListener *> button_listeners_;
  uint16_t chip_id_{};
  uint16_t project_id_{};
  uint16_t fw_build_{};
};

}  // namespace esphome::cst328
