#pragma once

#include "dxs238xw.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace dxs238xw {

template<typename... Ts> class HexMessageAction : public Action<Ts...> {
 public:
  explicit HexMessageAction(Dxs238xwComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, message)
  TEMPLATABLE_VALUE(bool, check_crc)

  void play(Ts... x) override {
    this->parent_->hex_message(this->message_.value(x...), this->check_crc_.value_or(x..., true));
  }

 protected:
  Dxs238xwComponent *parent_;
};

template<typename... Ts> class MeterStateToggleAction : public Action<Ts...> {
 public:
  explicit MeterStateToggleAction(Dxs238xwComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->meter_state_toggle(); }

 protected:
  Dxs238xwComponent *parent_;
};

template<typename... Ts> class MeterStateOnAction : public Action<Ts...> {
 public:
  explicit MeterStateOnAction(Dxs238xwComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->meter_state_on(); }

 protected:
  Dxs238xwComponent *parent_;
};

template<typename... Ts> class MeterStateOffAction : public Action<Ts...> {
 public:
  explicit MeterStateOffAction(Dxs238xwComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->meter_state_off(); }

 protected:
  Dxs238xwComponent *parent_;
};

}  // namespace dxs238xw
}  // namespace esphome
