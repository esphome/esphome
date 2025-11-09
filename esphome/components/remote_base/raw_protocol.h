#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>
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

class RawTrigger : public Trigger<RawTimings>, public Component, public RemoteReceiverListener {
 protected:
  bool on_receive(RemoteReceiveData src) override {
    this->trigger(src.get_raw_data());
    return false;
  }
};

template<typename... Ts> class RawAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_code_template(RawTimings (*func)(Ts...)) {
    this->code_.func = func;
    this->static_ = false;
  }
  void set_code_static(const int32_t *code, size_t len) {
    this->code_.static_code.data = code;
    this->code_.static_code.len = len;
    this->static_ = true;
  }
  TEMPLATABLE_VALUE(uint32_t, carrier_frequency);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->static_) {
      for (size_t i = 0; i < this->code_.static_code.len; i++) {
        auto val = this->code_.static_code.data[i];
        if (val < 0) {
          dst->space(static_cast<uint32_t>(-val));
        } else {
          dst->mark(static_cast<uint32_t>(val));
        }
      }
    } else {
      dst->set_data(this->code_.func(x...));
    }
    dst->set_carrier_frequency(this->carrier_frequency_.value(x...));
  }

 protected:
  bool static_{true};
  union Code {
    RawTimings (*func)(Ts...);
    struct {
      const int32_t *data;
      size_t len;
    } static_code;
  } code_;
};

class RawDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
  bool is_secondary() override { return true; }
};

}  // namespace remote_base
}  // namespace esphome
