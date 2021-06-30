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
  bool operator==(const MideaData &rhs) const {
    return !memcmp(this->data_, rhs.data_, sizeof(this->data_));
  }
  enum MideaDataType : uint8_t {
    MideaTypeCommand = 0xA1,
    MideaTypeSpecial = 0xA2,
    MideaTypeFollowMe = 0xA4,
  };
  MideaDataType type() const { return static_cast<MideaDataType>(this->data_[0]); }
  template <typename T> T to() const { return T(*this); }
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
  MideaFollowMe() : MideaData({ MideaTypeFollowMe, 0x82, 0x48, 0x7F, 0x1F }) {}
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
  MideaCommand() : MideaData({ MideaTypeCommand, 0x82, 0x48, 0xFF, 0xFF }) {}
  // Copy from Base
  MideaCommand(const MideaData &data) : MideaData(data) {}
  // Midea modes enum
  enum MideaMode : uint8_t {
    MideaModeCool,
    MideaModeDry,
    MideaModeAuto,
    MideaModeHeat,
    MideaModeFan,
  };
  // Midea fan enum
  enum MideaFan : uint8_t {
    MideaFanAuto,
    MideaFanLow,
    MideaFanMedium,
    MideaFanHigh,
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

DECLARE_REMOTE_PROTOCOL(Midea)

template<typename... Ts> class MideaRawAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_data(const uint8_t *data) {
    this->data_ = data;
    this->data_.finalize();
  }
  void set_template(std::function<std::vector<uint8_t>(Ts...)> func) { this->code_func_ = func; }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->code_func_ != nullptr) {
      this->data_ = this->code_func_(x...);
      this->data_.finalize();
    }
    MideaProtocol().encode(dst, this->data_);
  }
 protected:
  std::function<std::vector<uint8_t>(Ts...)> code_func_{};
  MideaData data_;
};

template<typename... Ts> class MideaFollowMeAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_template(std::function<uint8_t(Ts...)> func) { this->code_func_ = func; }
  void set_beeper(bool value) { this->data_.set_beeper(value); }
  void set_temp(uint8_t temp) {
    data_.set_temp(temp);
    data_.finalize();
  }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->code_func_ != nullptr)
      set_temp(this->code_func_(x...));
    MideaProtocol().encode(dst, this->data_);
  }
 protected:
  std::function<uint8_t(Ts...)> code_func_{};
  MideaFollowMe data_;
};

template<typename... Ts> class MideaToggleLightAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  MideaToggleLightAction() { this->data_.finalize(); }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    MideaProtocol().encode(dst, this->data_);
  }
 protected:
  MideaData data_ = { MideaData::MideaTypeSpecial, 0x08, 0xFF, 0xFF, 0xFF };
};

template<typename... Ts> class MideaSwingStepAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  MideaSwingStepAction() { this->data_.finalize(); }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    MideaProtocol().encode(dst, this->data_);
  }
 protected:
  MideaData data_ = { MideaData::MideaTypeSpecial, 0x01, 0xFF, 0xFF, 0xFF };
};

}  // namespace remote_base
}  // namespace esphome
