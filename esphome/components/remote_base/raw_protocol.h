#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>
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

class RawTrigger final : public Trigger<RawTimings>, public RemoteReceiverListener {
 protected:
  bool on_receive(RemoteReceiveData src) override {
    this->trigger(src.get_raw_data());
    return false;
  }
};

/// Appends constant timings, marks positive and spaces negative; the table may be in flash
inline void encode_raw_timings(RemoteTransmitData *dst, const int32_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    auto val = data[i];
    if (val < 0) {
      dst->space(static_cast<uint32_t>(-val));
    } else {
      dst->mark(static_cast<uint32_t>(val));
    }
  }
}

class RawDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
  bool is_secondary() override { return true; }
};

}  // namespace esphome::remote_base
