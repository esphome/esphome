#pragma once

#include "esphome/components/cstxxx/cstxxx.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <vector>

namespace esphome::cst226 {

static const uint8_t CST226_REG_STATUS = 0x00;

class CST226ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST226Touchscreen final : public cstxxx::CSTTouchscreen {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  bool can_proceed() override { return this->setup_complete_ || this->is_failed(); }
  void register_button_listener(CST226ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  void continue_setup_();
  void update_button_state_(bool state);

  bool button_touched_{};
  uint16_t chip_id_{};
  uint16_t project_id_{};
  std::vector<CST226ButtonListener *> button_listeners_;
};

}  // namespace esphome::cst226
