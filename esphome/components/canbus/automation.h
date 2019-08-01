#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace canbus {

template<typename... Ts> class CanbusSendAction : public Action<Ts...>, public Parented<Canbus> {
 public:
  void set_data_template(const std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void set_can_id(uint32_t can_id) { this->can_id_ = can_id; }

  void play(Ts... x) override {
    if (this->static_) {
      this->parent_->send_data(this->can_id_, this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->send_data(this->can_id_, val);
    }
  }

 protected:
  uint32_t can_id_;
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

}  // namespace canbus
}  // namespace esphome
