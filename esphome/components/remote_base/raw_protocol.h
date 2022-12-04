#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <vector>

namespace esphome {
namespace remote_base {

class RawBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  bool matches(RemoteReceiveData src) override {
    for (size_t i = 0; i < this->len_; i++) {
      auto val = this->data_[i];
      if (val < 0) {
        if (!src.expect_space(static_cast<uint32_t>(-val)))
          return false;
      } else {
        if (!src.expect_mark(static_cast<uint32_t>(val)))
          return false;
      }
    }
    return true;
  }
  void set_data(const int32_t *data) { data_ = data; }
  void set_len(size_t len) { len_ = len; }

 protected:
  const int32_t *data_;
  size_t len_;
};

class RawTrigger : public Trigger<std::vector<int32_t>>, public Component, public RemoteReceiverListener {
 protected:
  bool on_receive(RemoteReceiveData src) override {
    this->trigger(*src.get_raw_data());
    return false;
  }
};

template<typename... Ts> class RawAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_code_template(std::function<std::vector<int32_t>(Ts...)> func) { this->code_func_ = func; }
  void set_code_static(const int32_t *code, size_t len) {
    this->code_static_ = code;
    this->code_static_len_ = len;
  }
  TEMPLATABLE_VALUE(uint32_t, carrier_frequency);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->code_static_ != nullptr) {
      for (size_t i = 0; i < this->code_static_len_; i++) {
        auto val = this->code_static_[i];
        if (val < 0) {
          dst->space(static_cast<uint32_t>(-val));
        } else {
          dst->mark(static_cast<uint32_t>(val));
        }
      }
    } else {
      dst->set_data(this->code_func_(x...));
    }
    dst->set_carrier_frequency(this->carrier_frequency_.value(x...));
  }

 protected:
  std::function<std::vector<int32_t>(Ts...)> code_func_{};
  const int32_t *code_static_{nullptr};
  int32_t code_static_len_{0};
};

class RawDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
  bool is_secondary() override { return true; }
};

}  // namespace remote_base
}  // namespace esphome
