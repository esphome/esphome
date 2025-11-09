#pragma once

#include "udp_component.h"
#ifdef USE_NETWORK
#include "esphome/core/automation.h"

#include <vector>

namespace esphome {
namespace udp {

template<typename... Ts> class UDPWriteAction : public Action<Ts...>, public Parented<UDPComponent> {
 public:
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->data_.func = func;
    this->static_ = false;
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.static_data.ptr = data;
    this->data_.static_data.len = len;
    this->static_ = true;
  }

  void play(const Ts &...x) override {
    if (this->static_) {
      this->parent_->send_packet(this->data_.static_data.ptr, this->data_.static_data.len);
    } else {
      auto val = this->data_.func(x...);
      this->parent_->send_packet(val);
    }
  }

 protected:
  bool static_{true};
  union Data {
    std::vector<uint8_t> (*func)(Ts...);
    struct {
      const uint8_t *ptr;
      size_t len;
    } static_data;
  } data_;
};

}  // namespace udp
}  // namespace esphome
#endif
