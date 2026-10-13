#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BUTTON

#include "bq25186.h"
#include "esphome/components/button/button.h"

namespace esphome::bq25186 {

/// Writes one SHIP_RST field when pressed
class BQ25186Button final : public button::Button, public Parented<BQ25186Component> {
 public:
  BQ25186Button(uint8_t mask, uint8_t value) : mask_(mask), value_(value) {}

 protected:
  void press_action() override { this->parent_->write_ship_rst(this->mask_, this->value_); }

  const uint8_t mask_;
  const uint8_t value_;
};

}  // namespace esphome::bq25186

#endif  // USE_BUTTON
