#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>
#include <type_traits>
#include <variant>
#include <vector>

namespace esphome::remote_base {

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

/// x is a view of the receiver's buffer (RawTimingsView) when the automation runs synchronously, and a vector
/// (RawTimings) when it defers, since a deferred action keeps its own copy of x.
template<typename T> class RawTrigger final : public Trigger<T>, public RemoteReceiverListener {
 protected:
  bool on_receive(RemoteReceiveData src) override {
    if constexpr (std::is_same_v<T, RawTimingsView>) {
      this->trigger(src.get_raw_data());
    } else if (const RawTimings *timings = src.get_raw_vector()) {
      this->trigger(*timings);
    } else {
      RawTimingsView data = src.get_raw_data();
      this->timings_.assign(data.begin(), data.end());
      this->trigger(this->timings_);
    }
    return false;
  }

  // Only filled for a deferring automation fed from a receiver's own buffer
  [[no_unique_address]] std::conditional_t<std::is_same_v<T, RawTimings>, RawTimings, std::monostate> timings_;
};

template<typename... Ts> class RawAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_code_template(RawTimings (*func)(Ts...)) {
    this->code_.func = func;
    this->len_ = -1;  // Sentinel value indicates template mode
  }
  void set_code_static(const int32_t *code, size_t len) {
    this->code_.data = code;
    this->len_ = len;  // Length >= 0 indicates static mode
  }
  TEMPLATABLE_VALUE(uint32_t, carrier_frequency);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->len_ >= 0) {
      for (size_t i = 0; i < static_cast<size_t>(this->len_); i++) {
        auto val = this->code_.data[i];
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
  ssize_t len_{-1};  // -1 = template mode, >=0 = static mode with length
  union Code {
    RawTimings (*func)(Ts...);
    const int32_t *data;
  } code_;
};

class RawDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
  bool is_secondary() override { return true; }
};

}  // namespace esphome::remote_base
