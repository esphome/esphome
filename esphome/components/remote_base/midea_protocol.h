#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

class MideaData {
 public:
  // Make zero-filled
  MideaData() { memset(this->data_, 0, sizeof(this->data_)); }
  // Make from initializer_list
  MideaData(std::initializer_list<uint8_t> data) { std::copy(data.begin(), data.end(), this->data()); }
  // Make from vector
  MideaData(const std::vector<uint8_t> &data) {
    memcpy(this->data_, data.data(), std::min<size_t>(data.size(), sizeof(this->data_)));
  }
  // Default copy constructor
  MideaData(const MideaData &) = default;

  uint8_t *data() { return this->data_; }
  const uint8_t *data() const { return this->data_; }
  uint8_t size() const { return sizeof(this->data_); }
  bool is_valid() const { return this->data_[OFFSET_CS] == this->calc_cs_(); }
  void finalize() { this->data_[OFFSET_CS] = this->calc_cs_(); }
  bool check_compliment(const MideaData &rhs) const;
  std::string to_string() const { return hexencode(*this); }
  // compare only 40-bits
  bool operator==(const MideaData &rhs) const { return !memcmp(this->data_, rhs.data_, OFFSET_CS); }
  enum MideaDataType : uint8_t {
    MIDEA_TYPE_COMMAND = 0xA1,
    MIDEA_TYPE_SPECIAL = 0xA2,
    MIDEA_TYPE_FOLLOW_ME = 0xA4,
  };
  MideaDataType type() const { return static_cast<MideaDataType>(this->data_[0]); }
  template<typename T> T to() const { return T(*this); }

 protected:
  void set_value_(uint8_t offset, uint8_t val_mask, uint8_t shift, uint8_t val) {
    data_[offset] &= ~(val_mask << shift);
    data_[offset] |= (val << shift);
  }
  static const uint8_t OFFSET_CS = 5;
  // 48-bits data
  uint8_t data_[6];
  // Calculate checksum
  uint8_t calc_cs_() const;
};

class MideaProtocol : public RemoteProtocol<MideaData> {
 public:
  void encode(RemoteTransmitData *dst, const MideaData &data) override;
  optional<MideaData> decode(RemoteReceiveData src) override;
  void dump(const MideaData &data) override;

 protected:
  static const int32_t TICK_US = 560;
  static const int32_t HEADER_HIGH_US = 8 * TICK_US;
  static const int32_t HEADER_LOW_US = 8 * TICK_US;
  static const int32_t BIT_HIGH_US = 1 * TICK_US;
  static const int32_t BIT_ONE_LOW_US = 3 * TICK_US;
  static const int32_t BIT_ZERO_LOW_US = 1 * TICK_US;
  static const int32_t MIN_GAP_US = 10 * TICK_US;
  static void one(RemoteTransmitData *dst) { dst->item(BIT_HIGH_US, BIT_ONE_LOW_US); }
  static void zero(RemoteTransmitData *dst) { dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US); }
  static void header(RemoteTransmitData *dst) { dst->item(HEADER_HIGH_US, HEADER_LOW_US); }
  static void footer(RemoteTransmitData *dst) { dst->item(BIT_HIGH_US, MIN_GAP_US); }
  static void data(RemoteTransmitData *dst, const MideaData &src, bool compliment = false);
  static bool expect_one(RemoteReceiveData &src);
  static bool expect_zero(RemoteReceiveData &src);
  static bool expect_header(RemoteReceiveData &src);
  static bool expect_footer(RemoteReceiveData &src);
  static bool expect_data(RemoteReceiveData &src, MideaData &out);
};

class MideaBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  bool matches(RemoteReceiveData src) override {
    auto data = MideaProtocol().decode(src);
    return data.has_value() && data.value() == this->data_;
  }
  void set_code(const std::vector<uint8_t> &code) { this->data_ = code; }

 protected:
  MideaData data_;
};

using MideaTrigger = RemoteReceiverTrigger<MideaProtocol, MideaData>;
using MideaDumper = RemoteReceiverDumper<MideaProtocol, MideaData>;

template<typename... Ts> class MideaAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(std::vector<uint8_t>, code)
  void set_code(const std::vector<uint8_t> &code) { code_ = code; }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    MideaData data = this->code_.value(x...);
    data.finalize();
    MideaProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
