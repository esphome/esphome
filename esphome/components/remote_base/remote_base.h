#include <utility>
#include <vector>

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef USE_ESP32
#include <driver/rmt.h>
#endif

namespace esphome {
namespace remote_base {

using RawTimings = std::vector<int32_t>;

class RemoteTransmitData {
 public:
  void mark(uint32_t length) { this->data_.push_back(length); }
  void space(uint32_t length) { this->data_.push_back(-length); }
  void item(uint32_t mark, uint32_t space) {
    this->mark(mark);
    this->space(space);
  }
  void reserve(uint32_t len) { this->data_.reserve(len); }
  void set_carrier_frequency(uint32_t carrier_frequency) { this->carrier_frequency_ = carrier_frequency; }
  uint32_t get_carrier_frequency() const { return this->carrier_frequency_; }
  const RawTimings &get_data() const { return this->data_; }
  void set_data(const RawTimings &data) { this->data_ = data; }
  void reset() {
    this->data_.clear();
    this->carrier_frequency_ = 0;
  }

 protected:
  RawTimings data_{};
  uint32_t carrier_frequency_{0};
};

class RemoteReceiveData {
 public:
  explicit RemoteReceiveData(const RawTimings &data, uint8_t tolerance)
      : data_(data), index_(0), tolerance_(tolerance) {}

  const RawTimings &get_raw_data() const { return this->data_; }
  uint32_t get_index() const { return index_; }
  int32_t operator[](uint32_t index) const { return this->data_[index]; }
  int32_t size() const { return this->data_.size(); }
  bool is_valid(uint32_t offset) const { return this->index_ + offset < this->data_.size(); }
  int32_t peek(uint32_t offset = 0) const { return this->data_[this->index_ + offset]; }
  bool peek_mark(uint32_t length, uint32_t offset = 0) const;
  bool peek_space(uint32_t length, uint32_t offset = 0) const;
  bool peek_space_at_least(uint32_t length, uint32_t offset = 0) const;
  bool peek_item(uint32_t mark, uint32_t space, uint32_t offset = 0) const {
    return this->peek_space(space, offset + 1) && this->peek_mark(mark, offset);
  }

  bool expect_mark(uint32_t length);
  bool expect_space(uint32_t length);
  bool expect_item(uint32_t mark, uint32_t space);
  bool expect_pulse_with_gap(uint32_t mark, uint32_t space);
  void advance(uint32_t amount = 1) { this->index_ += amount; }
  void reset() { this->index_ = 0; }

 protected:
  int32_t lower_bound_(uint32_t length) const { return int32_t(100 - this->tolerance_) * length / 100U; }
  int32_t upper_bound_(uint32_t length) const { return int32_t(100 + this->tolerance_) * length / 100U; }

  const RawTimings &data_;
  uint32_t index_;
  uint8_t tolerance_;
};

class RemoteComponentBase {
 public:
  explicit RemoteComponentBase(InternalGPIOPin *pin) : pin_(pin){};

 protected:
  InternalGPIOPin *pin_;
};

#ifdef USE_ESP32
class RemoteRMTChannel {
 public:
  explicit RemoteRMTChannel(uint8_t mem_block_num = 1);

  void config_rmt(rmt_config_t &rmt);
  void set_clock_divider(uint8_t clock_divider) { this->clock_divider_ = clock_divider; }

 protected:
  uint32_t from_microseconds_(uint32_t us) {
    const uint32_t ticks_per_ten_us = 80000000u / this->clock_divider_ / 100000u;
    return us * ticks_per_ten_us / 10;
  }
  uint32_t to_microseconds_(uint32_t ticks) {
    const uint32_t ticks_per_ten_us = 80000000u / this->clock_divider_ / 100000u;
    return (ticks * 10) / ticks_per_ten_us;
  }
  RemoteComponentBase *remote_base_;
  rmt_channel_t channel_{RMT_CHANNEL_0};
  uint8_t mem_block_num_;
  uint8_t clock_divider_{80};
};
#endif

class RemoteTransmitterBase : public RemoteComponentBase {
 public:
  RemoteTransmitterBase(InternalGPIOPin *pin) : RemoteComponentBase(pin) {}
  class TransmitCall {
   public:
    explicit TransmitCall(RemoteTransmitterBase *parent) : parent_(parent) {}
    RemoteTransmitData *get_data() { return &this->parent_->temp_; }
    void set_send_times(uint32_t send_times) { send_times_ = send_times; }
    void set_send_wait(uint32_t send_wait) { send_wait_ = send_wait; }
    void perform() { this->parent_->send_(this->send_times_, this->send_wait_); }

   protected:
    RemoteTransmitterBase *parent_;
    uint32_t send_times_{1};
    uint32_t send_wait_{0};
  };

  TransmitCall transmit() {
    this->temp_.reset();
    return TransmitCall(this);
  }

 protected:
  void send_(uint32_t send_times, uint32_t send_wait);
  virtual void send_internal(uint32_t send_times, uint32_t send_wait) = 0;
  void send_single_() { this->send_(1, 0); }

  /// Use same vector for all transmits, avoids many allocations
  RemoteTransmitData temp_;
};

class RemoteReceiverListener {
 public:
  virtual bool on_receive(RemoteReceiveData data) = 0;
};

class RemoteReceiverDumperBase {
 public:
  virtual bool dump(RemoteReceiveData src) = 0;
  virtual bool is_secondary() { return false; }
};

class RemoteReceiverBase : public RemoteComponentBase {
 public:
  RemoteReceiverBase(InternalGPIOPin *pin) : RemoteComponentBase(pin) {}
  void register_listener(RemoteReceiverListener *listener) { this->listeners_.push_back(listener); }
  void register_dumper(RemoteReceiverDumperBase *dumper);
  void set_tolerance(uint8_t tolerance) { tolerance_ = tolerance; }

 protected:
  void call_listeners_();
  void call_dumpers_();
  void call_listeners_dumpers_() {
    this->call_listeners_();
    this->call_dumpers_();
  }

  std::vector<RemoteReceiverListener *> listeners_;
  std::vector<RemoteReceiverDumperBase *> dumpers_;
  std::vector<RemoteReceiverDumperBase *> secondary_dumpers_;
  RawTimings temp_;
  uint8_t tolerance_;
};

class RemoteReceiverBinarySensorBase : public binary_sensor::BinarySensorInitiallyOff,
                                       public Component,
                                       public RemoteReceiverListener {
 public:
  explicit RemoteReceiverBinarySensorBase() {}
  void dump_config() override;
  virtual bool matches(RemoteReceiveData src) = 0;
  bool on_receive(RemoteReceiveData src) override;
};

/* TEMPLATES */

template<typename T> class RemoteProtocol {
 public:
  virtual void encode(RemoteTransmitData *dst, const T &data) = 0;
  virtual optional<T> decode(RemoteReceiveData src) = 0;
  virtual void dump(const T &data) = 0;
};

template<typename T, typename D> class RemoteReceiverBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  RemoteReceiverBinarySensor() : RemoteReceiverBinarySensorBase() {}

 protected:
  bool matches(RemoteReceiveData src) override {
    auto proto = T();
    auto res = proto.decode(src);
    return res.has_value() && *res == this->data_;
  }

 public:
  void set_data(D data) { data_ = data; }

 protected:
  D data_;
};

template<typename T, typename D> class RemoteReceiverTrigger : public Trigger<D>, public RemoteReceiverListener {
 protected:
  bool on_receive(RemoteReceiveData src) override {
    auto proto = T();
    auto res = proto.decode(src);
    if (res.has_value()) {
      this->trigger(*res);
      return true;
    }
    return false;
  }
};

template<typename... Ts> class RemoteTransmitterActionBase : public Action<Ts...> {
 public:
  void set_parent(RemoteTransmitterBase *parent) { this->parent_ = parent; }

  TEMPLATABLE_VALUE(uint32_t, send_times);
  TEMPLATABLE_VALUE(uint32_t, send_wait);

  void play(Ts... x) override {
    auto call = this->parent_->transmit();
    this->encode(call.get_data(), x...);
    call.set_send_times(this->send_times_.value_or(x..., 1));
    call.set_send_wait(this->send_wait_.value_or(x..., 0));
    call.perform();
  }

 protected:
  virtual void encode(RemoteTransmitData *dst, Ts... x) = 0;

  RemoteTransmitterBase *parent_{};
};

template<typename T, typename D> class RemoteReceiverDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override {
    auto proto = T();
    auto decoded = proto.decode(src);
    if (!decoded.has_value())
      return false;
    proto.dump(*decoded);
    return true;
  }
};

#define DECLARE_REMOTE_PROTOCOL_(prefix) \
  using prefix##BinarySensor = RemoteReceiverBinarySensor<prefix##Protocol, prefix##Data>; \
  using prefix##Trigger = RemoteReceiverTrigger<prefix##Protocol, prefix##Data>; \
  using prefix##Dumper = RemoteReceiverDumper<prefix##Protocol, prefix##Data>;
#define DECLARE_REMOTE_PROTOCOL(prefix) DECLARE_REMOTE_PROTOCOL_(prefix)

}  // namespace remote_base
}  // namespace esphome
