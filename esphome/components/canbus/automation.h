#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/canbus/canbus.h"

namespace esphome {
namespace canbus {

template<typename... Ts> class CanbusSendAction : public Action<Ts...>, public Parented<Canbus> {
 public:
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void set_can_id(int can_id) {this->can_id_ = can_id;}

  TEMPLATABLE_VALUE(float, data)

//   void play(Ts... x) override { 
//     auto call = this->parent_->make_call(this->can_id_);
//     //unsigned uint const * p = reinterpret_cast<unsigned char const *>(&f);
//     call.set_data(this->data_.optional_value(x...));
// //    call.perform(this->parent_, this->can_id_);
//   }
  void play(Ts... x) override {
    if (this->static_) {
      this->parent_->write_array(this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->write_array(val);
    }
  }
 protected:
  Canbus *parent_;
  int can_id_;

    bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};


}  // namespace canbus
}  // namespace esphome
