#pragma once

#include "udp_component.h"
#ifdef USE_NETWORK
#include "esphome/core/automation.h"

#include <vector>

namespace esphome {
namespace udp {

template<typename... Ts> class UDPWriteAction : public Action<Ts...>, public Parented<UDPComponent> {
 public:
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void play(Ts... x) override {
    if (this->static_) {
      this->parent_->send_packet(this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->send_packet(val);
    }
  }

 protected:
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

}  // namespace udp
}  // namespace esphome
#endif
