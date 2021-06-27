#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

class MideaData {
 public:
  MideaData() { memset(data_, sizeof(data_), 0); }
  MideaData(const uint8_t *data);
  MideaData(const MideaData &rhs) = default;
  MideaData(const std::vector<uint8_t> &data);
  MideaData(std::initializer_list<uint8_t> data);

  uint8_t *data() { return data_; }
  const uint8_t *data() const { return data_; }
  uint8_t size() const { return sizeof(data_); }
  void finalize() { data_[OFFSET_CS] = calc_cs_(); }
  bool is_valid() const { return data_[OFFSET_CS] == calc_cs_(); }
  bool check_compliment(const MideaData &rhs) const;
  String raw_data() const;
  std::vector<int32_t> encode() const;
  bool operator==(const MideaData &rhs) const { return !memcmp(data_, rhs.data_, sizeof(data_)); }
  bool operator!=(const MideaData &rhs) const { return memcmp(data_, rhs.data_, sizeof(data_)); }
  enum MideaDataType : uint8_t {
    MideaTypeCommand = 0xA1,
    MideaTypeSpecial = 0xA2,
    MideaTypeFollowMe = 0xA4,
  };
  MideaDataType type() const { return static_cast<MideaDataType>(this->data_[0]); }
  void set_type(MideaDataType value) { data_[0] = value; }
  template <typename T> T to() const { return T(*this); }
 protected:
  static const uint8_t OFFSET_CS = 5;
  uint8_t data_[6]; // 48-bits data
  // Calculate checksum
  uint8_t calc_cs_() const;
};

class MideaFollowMe : public MideaData {
 public:
  MideaFollowMe() : MideaData() { this->set_type(MideaTypeFollowMe); }
  MideaFollowMe(const MideaData &data) : MideaData(data) {}
  MideaFollowMe(uint8_t temp) : MideaData() {
    this->set_temp(temp);
    this->finalize();
  }
  uint8_t temp() const { return (this->data_[4] & 0x7F) - 1; }
  void set_temp(uint8_t val) {
    if (val > MAX_TEMP)
      val = MAX_TEMP;
    this->data_[4] = val + 1;
  }
 protected:
  static const uint8_t MAX_TEMP = 37;
};

class MideaProtocol : public RemoteProtocol<MideaData> {
 public:
  void encode(RemoteTransmitData *dst, const MideaData &data) override;
  optional<MideaData> decode(RemoteReceiveData src) override;
  void dump(const MideaData &data) override;
 protected:
  static const int32_t TICK_US = 80;
  static const int32_t HEADER_HIGH_US = 56 * TICK_US;
  static const int32_t HEADER_LOW_US = 56 * TICK_US;
  static const int32_t BIT_HIGH_US = 7 * TICK_US;
  static const int32_t BIT_ONE_LOW_US = 21 * TICK_US;
  static const int32_t BIT_ZERO_LOW_US = 7 * TICK_US;
  static const int32_t MIN_GAP_US = 70 * TICK_US;
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

DECLARE_REMOTE_PROTOCOL(Midea)

template<typename... Ts> class MideaRawAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_data(const uint8_t *data) { data_ = data; }
  void set_template(std::function<std::vector<uint8_t>(Ts...)> func) { this->code_func_ = func; }

  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->code_func_ != nullptr) {
      this->data_ = this->code_func_(x...);
    } else {
      MideaProtocol().encode(dst, data_);
    }
  }

 protected:
  std::function<std::vector<uint8_t>(Ts...)> code_func_{};
  MideaData data_;
};

template<typename... Ts> class MideaFollowMeAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_template(std::function<uint8_t(Ts...)> func) { this->code_func_ = func; }
  void set_temp(uint8_t temp) {
    data_.set_temp(temp);
    data_.finalize();
  }

  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->code_func_ != nullptr) {
      set_temp(this->code_func_(x...));
    } else {
      MideaProtocol().encode(dst, data_);
    }
  }

 protected:
  std::function<uint8_t(Ts...)> code_func_{};
  MideaFollowMe data_;
};

template<typename... Ts> class MideaToggleLightAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void encode(RemoteTransmitData *dst, Ts... x) override {
    MideaProtocol().encode(dst, data_);
  }
 protected:
  MideaData data_ = {0xB5, 0xF5, 0xA5, 0xFF, 0xFF};
};

}  // namespace remote_base
}  // namespace esphome
