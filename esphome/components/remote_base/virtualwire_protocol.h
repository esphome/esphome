#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"
#include <array>
#include <utility>
#include <vector>

namespace esphome {
namespace remote_base {

static const uint8_t MAX_LENGTH = 80;
using tx_data_t = uint8_t;  // 6 bit encoded, transmitted LSB first!

class VirtualWireData {
 public:
  // Make default
  VirtualWireData() {
    this->data_.resize(3);
    std::fill(std::begin(this->data_), std::end(this->data_), 0);
  }
  // Make from initializer_list
  VirtualWireData(std::initializer_list<uint8_t> data) {
    size_t size = std::min(MAX_LENGTH, static_cast<uint8_t>(data.size()));
    this->data_.resize(size);
    std::copy_n(data.begin(), size, this->data_.begin());
  }
  // Make from vector
  VirtualWireData(const std::vector<uint8_t> &data) {
    size_t size = std::min(MAX_LENGTH, static_cast<uint8_t>(data.size()));
    this->data_.resize(size);
    std::copy_n(data.begin(), size, this->data_.begin());
  }
  // Default copy constructor
  VirtualWireData(const VirtualWireData &) = default;

  uint8_t *data() { return this->data_.data(); }
  const uint8_t *data() const { return this->data_.data(); }
  std::vector<uint8_t> get_raw_data() const { return this->data_; }
  uint8_t size() const {
    return std::min(static_cast<uint8_t>(3 + this->data_[0]), static_cast<uint8_t>(this->data_.size()));
  }
  bool is_valid() const;
  void set_data(std::vector<uint8_t> data) {
    uint8_t size = std::min(static_cast<uint8_t>(MAX_LENGTH - 3), static_cast<uint8_t>(data.size()));
    this->data_.resize(size + 3);
    this->data_[0] = size + 3;
    std::copy_n(data.begin(), size, this->data_.begin() + 1);
  }
  void set_bit_length(uint32_t bit_length_us) { this->bit_length_us_ = bit_length_us; }
  uint32_t get_bit_length() const { return this->bit_length_us_; }
  std::vector<uint8_t> get_data() const {
    std::vector<uint8_t> data(this->data_.begin() + 1, this->data_.begin() + 1 + get_data_size());
    return data;
  }
  uint8_t get_data_size() const {
    return std::min(static_cast<uint8_t>(MAX_LENGTH - 3), static_cast<uint8_t>(this->data_[0] - 3));
  }
  void finalize() {
    uint16_t crc = this->calc_crc_();
    this->data_[this->size() - 2] = crc & 0xff;
    this->data_[this->size() - 1] = crc >> 8;
  }
  std::string to_string() const {
    return format_hex_pretty(this->data_.data() + 1, this->get_data_size());
    // return format_hex_pretty(this->data_.data(), this->size());
  }
  bool operator==(const VirtualWireData &rhs) const {
    return std::equal(this->data_.begin(), this->data_.begin() + this->size(), rhs.data_.begin());
  }
  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

 protected:
  uint32_t bit_length_us_{500};  // 2000 bps
  std::vector<uint8_t> data_;

  // Calculate crc
  uint16_t calc_crc_() const;
};

class VirtualWireProtocol : public RemoteProtocol<VirtualWireData> {
 public:
  void encode(RemoteTransmitData *dst, const VirtualWireData &src) override;
  optional<VirtualWireData> decode(RemoteReceiveData src) override { return this->decode(src, 0); }
  optional<VirtualWireData> decode(RemoteReceiveData src, uint32_t bit_length_us);
  void dump(const VirtualWireData &data) override;

 protected:
  void encode_final_(RemoteTransmitData *dst, const std::vector<tx_data_t> &data, uint32_t bit_length_us) const;
  int8_t peek_mark_(RemoteReceiveData &src, uint32_t bit_length_us);
  int8_t peek_space_(RemoteReceiveData &src, uint32_t bit_length_us);
  bool decode_symbol_6to4_(uint8_t encoded, uint8_t &data);
  bool decode_4bit_(RemoteReceiveData &src, int8_t &remaining, bool &last_element, uint8_t &data,
                    uint32_t bit_length_us);
  bool decode_byte_(RemoteReceiveData &src, int8_t &remaining, bool &last_element, uint8_t &data,
                    uint32_t bit_length_us);
};

class VirtualWireBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  bool matches(RemoteReceiveData src) override {
    auto data = VirtualWireProtocol().decode(src, this->bit_length_us_);
    return data.has_value() && data.value() == this->data_;
  }
  void set_data(const std::vector<uint8_t> &data) { this->data_.set_data(data); }
  void set_speed(uint16_t speed) {
    if (!speed) {
      this->bit_length_us_ = 0;
    } else {
      this->bit_length_us_ = 1000000 / static_cast<uint32_t>(speed);
    }
  }
  void finalize() { this->data_.finalize(); }

 protected:
  uint16_t bit_length_us_{0};
  VirtualWireData data_;
};

using VirtualWireTrigger = RemoteReceiverTrigger<VirtualWireProtocol>;
using VirtualWireDumper = RemoteReceiverDumper<VirtualWireProtocol>;

template<typename... Ts> class VirtualWireAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, speed)

  void set_data_static(std::vector<uint8_t> data) { data_static_ = std::move(data); }
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    has_data_func_ = true;
  }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    VirtualWireData data;
    if (has_data_func_) {
      data.set_data(this->data_func_(x...));
    } else {
      data.set_data(this->data_static_);
    }
    uint32_t speed_value = static_cast<uint32_t>(this->speed_.value(x...));
    if (!speed_value) {
      speed_value = 2000;
    }
    data.set_bit_length(1000000 / speed_value);
    data.finalize();
    VirtualWireProtocol().encode(dst, data);
  }

 protected:
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
  bool has_data_func_{false};
};

}  // namespace remote_base
}  // namespace esphome
