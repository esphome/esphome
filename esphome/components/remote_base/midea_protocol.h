#pragma once

#include "esphome/core/component.h"
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
  // Make 40-bit copy from array
  MideaData(const uint8_t *data) { memcpy(this->data(), data, OFFSET_CS); }
  // Default copy constructor
  MideaData(const MideaData &) = default;

  uint8_t *data() { return this->data_; }
  const uint8_t *data() const { return this->data_; }
  uint8_t size() const { return sizeof(this->data_); }
  bool is_valid() const { return this->data_[OFFSET_CS] == this->calc_cs_(); }
  void finalize() { this->data_[OFFSET_CS] = this->calc_cs_(); }
  bool check_compliment(const MideaData &rhs) const;
  String raw_data() const;
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

class MideaFollowMe : public MideaData {
 public:
  // Default constructor (temp: 30C, beeper: off)
  MideaFollowMe() : MideaData({MIDEA_TYPE_FOLLOW_ME, 0x82, 0x48, 0x7F, 0x1F}) {}
  // Copy from Base
  MideaFollowMe(const MideaData &data) : MideaData(data) {}
  // Direct from temperature and beeper values
  MideaFollowMe(uint8_t temp, bool beeper = false) : MideaFollowMe() {
    this->set_temp(temp);
    this->set_beeper(beeper);
  }

  /* TEMPERATURE */
  uint8_t temp() const { return this->data_[4] - 1; }
  void set_temp(uint8_t val) { this->data_[4] = std::min(MAX_TEMP, val) + 1; }

  /* BEEPER */
  bool beeper() const { return this->data_[3] & 128; }
  void set_beeper(bool val) { this->set_value_(3, 1, 7, val); }

 protected:
  static const uint8_t MAX_TEMP = 37;
};

class MideaCommand : public MideaData {
 public:
  // Default constructor: power: on, mode: auto, fan: auto, temp: 25C, all timers: off
  MideaCommand() : MideaData({MIDEA_TYPE_COMMAND, 0x82, 0x48, 0xFF, 0xFF}) {}
  // Copy from Base
  MideaCommand(const MideaData &data) : MideaData(data) {}
  // Midea modes enum
  enum MideaMode : uint8_t {
    MIDEA_MODE_COOL,
    MIDEA_MODE_DRY,
    MIDEA_MODE_AUTO,
    MIDEA_MODE_HEAT,
    MIDEA_MODE_FAN,
  };
  // Midea fan enum
  enum MideaFan : uint8_t {
    MIDEA_FAN_AUTO,
    MIDEA_FAN_LOW,
    MIDEA_FAN_MEDIUM,
    MIDEA_FAN_HIGH,
  };
  // Set temperature setpoint
  void set_temp(uint8_t val) { this->data_[2] = std::min(MAX_TEMP, std::max(MIN_TEMP, val)) - MIN_TEMP; }
  // Set mode
  void set_mode(MideaMode mode) { set_value_(1, 0b111, 0, mode); }
  // Set fan speed
  void set_fan_speed(MideaFan fan) { set_value_(1, 0b11, 3, fan); }
  // Set sleep
  void set_sleep(bool val) { set_value_(1, 0b1, 6, val); }
  // Set power
  void set_power(bool val) { set_value_(1, 0b1, 7, val); }
  // Set ON timer (0: disable)
  void set_on_timer(uint16_t minutes);
  // Set OFF timer (0: disable)
  void set_off_timer(uint16_t minutes);

 protected:
  static const uint8_t MIN_TEMP = 17;
  static const uint8_t MAX_TEMP = 30;
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
  void set_code(const uint8_t *code) { this->data_ = code; }

 protected:
  MideaData data_;
};

using MideaTrigger = RemoteReceiverTrigger<MideaProtocol, MideaData>;
using MideaDumper = RemoteReceiverDumper<MideaProtocol, MideaData>;

template<typename... Ts> class MideaRawAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(const uint8_t *, code)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    MideaData data = this->code_.value(x...);
    data.finalize();
    MideaProtocol().encode(dst, data);
  }
};

template<typename... Ts> class MideaFollowMeAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(uint8_t, temperature)
  TEMPLATABLE_VALUE(bool, beeper)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    MideaFollowMe data;
    data.set_temp(this->temperature_.value(x...));
    data.set_beeper(this->beeper_.value(x...));
    data.finalize();
    MideaProtocol().encode(dst, data);
  }
};

template<typename... Ts> class MideaDisplayToggleAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  MideaDisplayToggleAction() { this->data_.finalize(); }
  void encode(RemoteTransmitData *dst, Ts... x) override { MideaProtocol().encode(dst, this->data_); }

 protected:
  MideaData data_ = {MideaData::MIDEA_TYPE_SPECIAL, 0x08, 0xFF, 0xFF, 0xFF};
};

template<typename... Ts> class MideaSwingStepAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  MideaSwingStepAction() { this->data_.finalize(); }
  void encode(RemoteTransmitData *dst, Ts... x) override { MideaProtocol().encode(dst, this->data_); }

 protected:
  MideaData data_ = {MideaData::MIDEA_TYPE_SPECIAL, 0x01, 0xFF, 0xFF, 0xFF};
};

}  // namespace remote_base
}  // namespace esphome
