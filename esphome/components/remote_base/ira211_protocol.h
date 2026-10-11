#pragma once

#include "remote_base.h"
#include "esphome/core/helpers.h"

#include <cmath>

namespace esphome::remote_base {

/* Siemens IRA211 thermostat remote: 38 kHz carrier, NRZ runs of 800 us bits after a fixed preamble.
   A frame is 4, 5 or 7 ten-bit packets (start bit 1, 8 data bits, end bit 0): device ID, command,
   the command's fields, then a checksum. Field bytes go over the wire as reverse_bits(~value). */

enum class IRA211Command : uint8_t {
  IRA211_COMMAND_TEMP_UP = 1,
  IRA211_COMMAND_TEMP_DOWN = 2,
  IRA211_COMMAND_MODE = 4,
  IRA211_COMMAND_FAN = 5,
  IRA211_COMMAND_POWER = 6,
  IRA211_COMMAND_SYNC = 7,
};

enum class IRA211Mode : uint8_t {
  IRA211_MODE_PROTECTION = 0,
  IRA211_MODE_TIMER = 85,
  IRA211_MODE_COMFORT = 170,
};

enum class IRA211Fan : uint8_t {
  IRA211_FAN_AUTO = 3,
  IRA211_FAN_LOW = 12,
  IRA211_FAN_MEDIUM = 48,
  IRA211_FAN_HIGH = 192,
};

static constexpr float IRA211_MIN_TEMPERATURE = 5.0f;
static constexpr float IRA211_MAX_TEMPERATURE = 35.0f;

struct IRA211Data {
  IRA211Command command{IRA211Command::IRA211_COMMAND_SYNC};
  uint8_t half_degrees{40};  // target temperature in 0.5 C steps
  IRA211Mode mode{IRA211Mode::IRA211_MODE_COMFORT};
  IRA211Fan fan{IRA211Fan::IRA211_FAN_AUTO};

  float get_temperature() const { return this->half_degrees * 0.5f; }
  /// Rounds to the nearest half degree and clamps to the 5 to 35 C range the thermostat accepts.
  void set_temperature(float temperature) {
    this->half_degrees =
        static_cast<uint8_t>(lroundf(clamp(temperature, IRA211_MIN_TEMPERATURE, IRA211_MAX_TEMPERATURE) * 2.0f));
  }

  /// Compares the command and only the fields that command carries.
  bool operator==(const IRA211Data &rhs) const;
};

class IRA211Protocol : public RemoteProtocol<IRA211Data> {
 public:
  void encode(RemoteTransmitData *dst, const IRA211Data &data);
  optional<IRA211Data> decode(RemoteReceiveData src);
  void dump(const IRA211Data &data);
};

DECLARE_REMOTE_PROTOCOL(IRA211)

template<typename... Ts> class IRA211Action : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(IRA211Command, command)
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(IRA211Mode, mode)
  TEMPLATABLE_VALUE(IRA211Fan, fan)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    IRA211Data data{};
    data.command = this->command_.value(x...);
    data.set_temperature(this->temperature_.value(x...));
    data.mode = this->mode_.value(x...);
    data.fan = this->fan_.value(x...);
    IRA211Protocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
