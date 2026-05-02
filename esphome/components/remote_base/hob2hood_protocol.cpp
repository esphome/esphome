#include "hob2hood_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.hob2hood";

static constexpr uint32_t BIT_TIME_US = 700;
static constexpr int32_t SPACE_US = -200;
static constexpr int32_t MARK_US = 300;

bool Hob2HoodProtocol::get_timings_(const Hob2HoodCommand data, RemoteReceiveData *src, RemoteTransmitData *dst) {
  static constexpr uint32_t BIT_MASK = (1U << 24U);
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
    if (bits_remaining == 1 || (bit_counter > 0) != current_bit) {
      uint32_t total_time = ((bit_counter > 0) ? SPACE_US : MARK_US) + BIT_TIME_US * std::abs(bit_counter);
      if (src != nullptr && result) {
        if (bit_counter < 0) {
          result = src->expect_mark(total_time);
        } else if (bits_remaining != 1) {
          result = src->expect_space(total_time);
        }
      }
      if (dst != nullptr) {
        if (bit_counter < 0) {
          dst->mark(total_time);
        } else {
          dst->space(total_time);
        }
      } else if (!result) {
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
  const Hob2HoodCommand commands[] = {
      HOB2HOOD_CMD_LIGHT_OFF,  HOB2HOOD_CMD_LIGHT_ON, HOB2HOOD_CMD_FAN_OFF, HOB2HOOD_CMD_FAN_LOW,
      HOB2HOOD_CMD_FAN_MEDIUM, HOB2HOOD_CMD_FAN_HIGH, HOB2HOOD_CMD_FAN_MAX,
  };
  for (auto cmd : commands) {
    src.reset();
    if (this->get_timings_(cmd, &src, nullptr)) {
      return cmd;
    }
  }
  return {};
}

void Hob2HoodProtocol::dump(const Hob2HoodData &data) {
  char command_str[11] = {0};
  switch (data.command) {
    case HOB2HOOD_CMD_LIGHT_OFF:
      snprintf(command_str, sizeof(command_str), "Light Off");
      break;
    case HOB2HOOD_CMD_LIGHT_ON:
      snprintf(command_str, sizeof(command_str), "Light On");
      break;
    case HOB2HOOD_CMD_FAN_OFF:
      snprintf(command_str, sizeof(command_str), "Fan Off");
      break;
    case HOB2HOOD_CMD_FAN_LOW:
      snprintf(command_str, sizeof(command_str), "Fan Low");
      break;
    case HOB2HOOD_CMD_FAN_MEDIUM:
      snprintf(command_str, sizeof(command_str), "Fan Medium");
      break;
    case HOB2HOOD_CMD_FAN_HIGH:
      snprintf(command_str, sizeof(command_str), "Fan High");
      break;
    case HOB2HOOD_CMD_FAN_MAX:
      snprintf(command_str, sizeof(command_str), "Fan Max");
      break;
    default:
      snprintf(command_str, sizeof(command_str), "Unknown");
      break;
  }
  ESP_LOGD(TAG, "Received Hob2Hood: %s", command_str);
}

}  // namespace esphome::remote_base
