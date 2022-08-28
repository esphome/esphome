#pragma once

#include "dxs238xw.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace dxs238xw {

template<typename... Ts> class sendHexMessageAction : public Action<Ts...> {
 public:
  explicit sendHexMessageAction(dxs238xwComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(const char *, message)
  TEMPLATABLE_VALUE(bool, check_crc)

  void play(Ts... x) override {
    this->parent_->send_hex_message(this->message_.value(x...), this->check_crc_.value_or(x..., true));
  }

 protected:
  dxs238xwComponent *parent_;
};

template<typename... Ts> class setMeterStateToogleAction : public Action<Ts...> {
 public:
  explicit setMeterStateToogleAction(dxs238xwComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->set_meter_state_toogle(); }

 protected:
  dxs238xwComponent *parent_;
};

template<typename... Ts> class setMeterStateOnAction : public Action<Ts...> {
 public:
  explicit setMeterStateOnAction(dxs238xwComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->set_meter_state_on(); }

 protected:
  dxs238xwComponent *parent_;
};

template<typename... Ts> class setMeterStateOffAction : public Action<Ts...> {
 public:
  explicit setMeterStateOffAction(dxs238xwComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->set_meter_state_off(); }

 protected:
  dxs238xwComponent *parent_;
};

}  // namespace dxs238xw
}  // namespace esphome