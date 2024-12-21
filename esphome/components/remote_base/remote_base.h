#include <utility>
#include <vector>

#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#if defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR < 5
#include <driver/rmt.h>
#endif

namespace esphome {
namespace remote_base {

enum ToleranceMode : uint8_t {
  TOLERANCE_MODE_PERCENTAGE = 0,
  TOLERANCE_MODE_TIME = 1,
};

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
  explicit RemoteReceiveData(const RawTimings &data, uint32_t tolerance, ToleranceMode tolerance_mode)
      : data_(data), index_(0), tolerance_(tolerance), tolerance_mode_(tolerance_mode) {}

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

  void set_tolerance(uint32_t tolerance, ToleranceMode tolerance_mode) {
    this->tolerance_ = tolerance;
    this->tolerance_mode_ = tolerance_mode;
  }
  uint32_t get_tolerance() { return tolerance_; }
  ToleranceMode get_tolerance_mode() { return this->tolerance_mode_; }

 protected:
  int32_t lower_bound_(uint32_t length) const {
    if (this->tolerance_mode_ == TOLERANCE_MODE_TIME) {
      return int32_t(length - this->tolerance_);
    } else if (this->tolerance_mode_ == TOLERANCE_MODE_PERCENTAGE) {
      return int32_t(100 - this->tolerance_) * length / 100U;
    }
    return 0;
  }
  int32_t upper_bound_(uint32_t length) const {
    if (this->tolerance_mode_ == TOLERANCE_MODE_TIME) {
      return int32_t(length + this->tolerance_);
    } else if (this->tolerance_mode_ == TOLERANCE_MODE_PERCENTAGE) {
      return int32_t(100 + this->tolerance_) * length / 100U;
    }
    return 0;
  }

  const RawTimings &data_;
  uint32_t index_;
  uint32_t tolerance_;
  ToleranceMode tolerance_mode_;
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
#if ESP_IDF_VERSION_MAJOR >= 5
  void set_clock_resolution(uint32_t clock_resolution) { this->clock_resolution_ = clock_resolution; }
  void set_rmt_symbols(uint32_t rmt_symbols) { this->rmt_symbols_ = rmt_symbols; }
#else
  explicit RemoteRMTChannel(uint8_t mem_block_num = 1);
  explicit RemoteRMTChannel(rmt_channel_t channel, uint8_t mem_block_num = 1);

  void config_rmt(rmt_config_t &rmt);
  void set_clock_divider(uint8_t clock_divider) { this->clock_divider_ = clock_divider; }
#endif

 protected:
  uint32_t from_microseconds_(uint32_t us) {
#if ESP_IDF_VERSION_MAJOR >= 5
    const uint32_t ticks_per_ten_us = this->clock_resolution_ / 100000u;
#else
    const uint32_t ticks_per_ten_us = 80000000u / this->clock_divider_ / 100000u;
#endif
    return us * ticks_per_ten_us / 10;
  }
  uint32_t to_microseconds_(uint32_t ticks) {
#if ESP_IDF_VERSION_MAJOR >= 5
    const uint32_t ticks_per_ten_us = this->clock_resolution_ / 100000u;
#else
    const uint32_t ticks_per_ten_us = 80000000u / this->clock_divider_ / 100000u;
#endif
    return (ticks * 10) / ticks_per_ten_us;
  }
  RemoteComponentBase *remote_base_;
#if ESP_IDF_VERSION_MAJOR >= 5
  uint32_t clock_resolution_{1000000};
  uint32_t rmt_symbols_;
#else
  rmt_channel_t channel_{RMT_CHANNEL_0};
  uint8_t mem_block_num_;
  uint8_t clock_divider_{80};
#endif
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
  template<typename Protocol>
  void transmit(const typename Protocol::ProtocolData &data, uint32_t send_times = 1, uint32_t send_wait = 0) {
    auto call = this->transmit();
    Protocol().encode(call.get_data(), data);
    call.set_send_times(send_times);
    call.set_send_wait(send_wait);
    call.perform();
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
  void set_tolerance(uint32_t tolerance, ToleranceMode tolerance_mode) {
    this->tolerance_ = tolerance;
    this->tolerance_mode_ = tolerance_mode;
  }

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
  uint32_t tolerance_{25};
  ToleranceMode tolerance_mode_{TOLERANCE_MODE_PERCENTAGE};
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
  using ProtocolData = T;
  virtual void encode(RemoteTransmitData *dst, const ProtocolData &data) = 0;
  virtual optional<ProtocolData> decode(RemoteReceiveData src) = 0;
  virtual void dump(const ProtocolData &data) = 0;
};

template<typename T> class RemoteReceiverBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  RemoteReceiverBinarySensor() : RemoteReceiverBinarySensorBase() {}

 protected:
  bool matches(RemoteReceiveData src) override {
    auto proto = T();
    auto res = proto.decode(src);
    return res.has_value() && *res == this->data_;
  }

 public:
  void set_data(typename T::ProtocolData data) { data_ = data; }

 protected:
  typename T::ProtocolData data_;
};

template<typename T>
class RemoteReceiverTrigger : public Trigger<typename T::ProtocolData>, public RemoteReceiverListener {
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

class RemoteTransmittable {
 public:
  RemoteTransmittable() {}
  RemoteTransmittable(RemoteTransmitterBase *transmitter) : transmitter_(transmitter) {}
  void set_transmitter(RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }

 protected:
  template<typename Protocol>
  void transmit_(const typename Protocol::ProtocolData &data, uint32_t send_times = 1, uint32_t send_wait = 0) {
    this->transmitter_->transmit<Protocol>(data, send_times, send_wait);
  }
  RemoteTransmitterBase *transmitter_;
};

template<typename... Ts> class RemoteTransmitterActionBase : public RemoteTransmittable, public Action<Ts...> {
  TEMPLATABLE_VALUE(uint32_t, send_times)
  TEMPLATABLE_VALUE(uint32_t, send_wait)

 protected:
  void play(Ts... x) override {
    auto call = this->transmitter_->transmit();
    this->encode(call.get_data(), x...);
    call.set_send_times(this->send_times_.value_or(x..., 1));
    call.set_send_wait(this->send_wait_.value_or(x..., 0));
    call.perform();
  }
  virtual void encode(RemoteTransmitData *dst, Ts... x) = 0;
};

template<typename T> class RemoteReceiverDumper : public RemoteReceiverDumperBase {
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
  using prefix##BinarySensor = RemoteReceiverBinarySensor<prefix##Protocol>; \
  using prefix##Trigger = RemoteReceiverTrigger<prefix##Protocol>; \
  using prefix##Dumper = RemoteReceiverDumper<prefix##Protocol>;
#define DECLARE_REMOTE_PROTOCOL(prefix) DECLARE_REMOTE_PROTOCOL_(prefix)

}  // namespace remote_base
}  // namespace esphome
