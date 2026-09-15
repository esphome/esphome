#pragma once

#include <array>
#include <vector>

#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome::remote_base {

static const uint8_t WYT_REMOTE_COMMAND_SIZE = 14;

enum PioneerWytMode : uint8_t {
  PIONEER_WYT_MODE_HEAT = 0x01,
  PIONEER_WYT_MODE_DRY = 0x02,
  PIONEER_WYT_MODE_COOL = 0x03,
  PIONEER_WYT_MODE_FAN = 0x07,
  PIONEER_WYT_MODE_AUTO = 0x08,
};

enum PioneerWytFanSpeed : uint8_t {
  PIONEER_WYT_FAN_AUTO = 0x01,
  PIONEER_WYT_FAN_LOW = 0x02,
  PIONEER_WYT_FAN_MEDIUM_LOW = 0x03,
  PIONEER_WYT_FAN_MEDIUM = 0x04,
  PIONEER_WYT_FAN_MEDIUM_HIGH = 0x05,
  PIONEER_WYT_FAN_HIGH = 0x06,
};

enum PioneerWytDataType : uint8_t {
  PIONEER_WYT_TYPE_GENERAL = 0x01,
  PIONEER_WYT_TYPE_FAN = 0x02,
  PIONEER_WYT_TYPE_BOTH = 0x03,
};

class PioneerWytData {
 public:
  PioneerWytData() {}
  explicit PioneerWytData(const std::vector<uint8_t> &data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }

  uint8_t size() const { return this->data_.size(); }
  void finalize() {
    this->data_[WYT_REMOTE_COMMAND_SIZE - 1] = this->calc_cs_(this->type() == PIONEER_WYT_TYPE_FAN ? 0x0F : 0x00);
  }

  PioneerWytDataType type() const { return static_cast<PioneerWytDataType>(this->data_[3]); }
  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

  static PioneerWytData make_general(bool power, uint8_t mode, float target_temperature, bool beeper, bool display,
                                     bool eco, bool turbo, bool sleep, bool follow_me, uint8_t remote_temp,
                                     bool up_down_swing, bool left_right_swing);

  static PioneerWytData make_fan(uint8_t fan_speed, bool mute, bool vertical_swing, bool horizontal_swing);

 protected:
  std::array<uint8_t, WYT_REMOTE_COMMAND_SIZE> data_{0};

  // Calculate checksum
  uint8_t calc_cs_(uint8_t checksum_offset = 0x00) const;
};

class PioneerWytProtocol {
 public:
  void encode(RemoteTransmitData *dst, const PioneerWytData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

template<typename... Ts> class PioneerWytAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, code)
  TEMPLATABLE_VALUE(PioneerWytDataType, type)
  TEMPLATABLE_VALUE(bool, power)
  TEMPLATABLE_VALUE(PioneerWytMode, mode)
  TEMPLATABLE_VALUE(float, target_temperature)
  TEMPLATABLE_VALUE(bool, beeper)
  TEMPLATABLE_VALUE(bool, display)
  TEMPLATABLE_VALUE(bool, eco)
  TEMPLATABLE_VALUE(bool, turbo)
  TEMPLATABLE_VALUE(bool, sleep)
  TEMPLATABLE_VALUE(bool, follow_me)
  TEMPLATABLE_VALUE(uint8_t, remote_temp)
  TEMPLATABLE_VALUE(PioneerWytFanSpeed, fan_speed)
  TEMPLATABLE_VALUE(bool, mute)
  TEMPLATABLE_VALUE(bool, vertical_swing)
  TEMPLATABLE_VALUE(bool, horizontal_swing)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    if (this->code_.has_value()) {
      PioneerWytData data(this->code_.value(x...));
      data.finalize();
      PioneerWytProtocol().encode(dst, data);
    } else {
      auto type = this->type_.value(x...);
      if (type == PIONEER_WYT_TYPE_GENERAL) {
        PioneerWytData data = PioneerWytData::make_general(
            this->power_.value(x...), static_cast<uint8_t>(this->mode_.value(x...)),
            this->target_temperature_.value(x...), this->beeper_.value(x...), this->display_.value(x...),
            this->eco_.value(x...), this->turbo_.value(x...), this->sleep_.value(x...), this->follow_me_.value(x...),
            this->remote_temp_.value(x...), this->vertical_swing_.value(x...), this->horizontal_swing_.value(x...));
        PioneerWytProtocol().encode(dst, data);
      } else if (type == PIONEER_WYT_TYPE_FAN) {
        PioneerWytData data =
            PioneerWytData::make_fan(static_cast<uint8_t>(this->fan_speed_.value(x...)), this->mute_.value(x...),
                                     this->vertical_swing_.value(x...), this->horizontal_swing_.value(x...));
        PioneerWytProtocol().encode(dst, data);
      } else if (type == PIONEER_WYT_TYPE_BOTH) {
        PioneerWytData fan_data =
            PioneerWytData::make_fan(static_cast<uint8_t>(this->fan_speed_.value(x...)), this->mute_.value(x...),
                                     this->vertical_swing_.value(x...), this->horizontal_swing_.value(x...));
        PioneerWytProtocol().encode(dst, fan_data);

        // ~70ms delay between fan and general packets
        dst->space(70000);

        PioneerWytData general_data = PioneerWytData::make_general(
            this->power_.value(x...), static_cast<uint8_t>(this->mode_.value(x...)),
            this->target_temperature_.value(x...), this->beeper_.value(x...), this->display_.value(x...),
            this->eco_.value(x...), this->turbo_.value(x...), this->sleep_.value(x...), this->follow_me_.value(x...),
            this->remote_temp_.value(x...), this->vertical_swing_.value(x...), this->horizontal_swing_.value(x...));
        PioneerWytProtocol().encode(dst, general_data);
      }
    }
  }
};

}  // namespace esphome::remote_base
