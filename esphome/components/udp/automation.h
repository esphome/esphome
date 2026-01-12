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
    this->len_ = -1;  // Sentinel value indicates template mode
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.data = data;
    this->len_ = len;  // Length >= 0 indicates static mode
  }

  void play(const Ts &...x) override {
    if (this->len_ >= 0) {
      // Static mode: pass pointer directly to send_packet(const uint8_t *, size_t)
      this->parent_->send_packet(this->data_.data, static_cast<size_t>(this->len_));
    } else {
      // Template mode: call function and pass vector to send_packet(const std::vector<uint8_t> &)
      auto val = this->data_.func(x...);
      this->parent_->send_packet(val);
    }
  }

 protected:
  ssize_t len_{-1};  // -1 = template mode, >=0 = static mode with length
  union Data {
    std::vector<uint8_t> (*func)(Ts...);  // Function pointer (stateless lambdas)
    const uint8_t *data;                  // Pointer to static data in flash
  } data_;
};

}  // namespace udp
}  // namespace esphome
#endif
