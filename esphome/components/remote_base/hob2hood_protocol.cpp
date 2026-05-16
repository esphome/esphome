#include "hob2hood_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.hob2hood";

static constexpr uint32_t BIT_TIME_US = 700;
static constexpr int32_t SPACE_US = -200;
static constexpr int32_t MARK_US = 300;

bool Hob2HoodProtocol::get_timings_(const Hob2HoodCommand data, RemoteReceiveData *src, RemoteTransmitData *dst) {
  // Generate the timings for receiving or transmitting Hob2Hood data. src or dst may be set to nullptr.
  static constexpr uint32_t BIT_MASK = (1U << 24U);
  // Transmitted data is 25 bits, first bit is always zero, followed by (data) (data + 1) (data + 2)
  uint32_t transmitted_data = ((uint8_t) data << 16U) | ((uint8_t) (data + 1) << 8U) | (uint8_t) (data + 2);
  int8_t bit_counter = 0;
  bool current_bit = (transmitted_data & BIT_MASK) != 0;
  bool result = true;
  for (uint8_t bits_remaining = 25; bits_remaining > 0; bits_remaining--) {
    if (current_bit) {
      bit_counter++;
    } else {
      bit_counter--;
    }
    transmitted_data <<= 1U;
    current_bit = (transmitted_data & BIT_MASK) != 0;
    // Emit a mark or space if the bit value changes compared to the previous bit or if the message is finished
    if (bits_remaining == 1 || (bit_counter > 0) != current_bit) {
      uint32_t total_time = ((bit_counter > 0) ? SPACE_US : MARK_US) + BIT_TIME_US * std::abs(bit_counter);
      if (src != nullptr && result) {  // Receiving
        if (bit_counter < 0) {
          result = src->expect_mark(total_time);
        } else if (bits_remaining != 1) {  // Ignore the last space if the message ends with a space
          result = src->expect_space(total_time);
        }
      }
      if (dst != nullptr) {  // Transmitting
        if (bit_counter < 0) {
          dst->mark(total_time);
        } else {
          dst->space(total_time);
        }
      } else if (!result) {  // If we're only receiving, return early
        return result;
      }
      bit_counter = 0;
    }
  }
  return result;
}

void Hob2HoodProtocol::encode(RemoteTransmitData *dst, const Hob2HoodData &data) {
  static constexpr uint32_t CARRIER_FREQUENCY = 38000;
  static constexpr uint32_t RESERVE_LENGTH = 17;
  dst->set_carrier_frequency(CARRIER_FREQUENCY);
  dst->reserve(RESERVE_LENGTH);
  this->get_timings_(data.command, nullptr, dst);
}

optional<Hob2HoodData> Hob2HoodProtocol::decode(RemoteReceiveData src) {
  static constexpr std::array<Hob2HoodCommand, 7> COMMANDS = {
    HOB2HOOD_CMD_LIGHT_OFF, HOB2HOOD_CMD_LIGHT_ON, HOB2HOOD_CMD_FAN_OFF, HOB2HOOD_CMD_FAN_LOW,
    HOB2HOOD_CMD_FAN_MEDIUM, HOB2HOOD_CMD_FAN_HIGH, HOB2HOOD_CMD_FAN_MAX,
  };
  for (auto cmd : COMMANDS) {
    src.reset();
    if (this->get_timings_(cmd, &src, nullptr)) {
      return Hob2HoodData{cmd};
    }
  }
  return {};
}

void Hob2HoodProtocol::dump(const Hob2HoodData &data) {
  const char *command_str;
  switch (data.command) {
    case HOB2HOOD_CMD_LIGHT_OFF: command_str = "light_off"; break;
    case HOB2HOOD_CMD_LIGHT_ON: command_str = "light_on"; break;
    case HOB2HOOD_CMD_FAN_OFF: command_str = "fan_off"; break;
    case HOB2HOOD_CMD_FAN_LOW: command_str = "fan_low"; break;
    case HOB2HOOD_CMD_FAN_MEDIUM: command_str = "fan_medium"; break;
    case HOB2HOOD_CMD_FAN_HIGH: command_str = "fan_high"; break;
    case HOB2HOOD_CMD_FAN_MAX: command_str = "fan_max"; break;
    default: command_str = "unknown"; break;
  }
  ESP_LOGD(TAG, "Received Hob2Hood: %s", command_str);
}

}  // namespace esphome::remote_base
