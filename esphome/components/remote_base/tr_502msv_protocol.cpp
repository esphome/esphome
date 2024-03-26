#include "tr_502msv_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.tr_502msv";

static const uint32_t BIT_ZERO_SPACE_US = 640;
static const uint32_t BIT_ZERO_MARK_US = 1270;
static const uint32_t BIT_ONE_SPACE_US = 1270;
static const uint32_t BIT_ONE_MARK_US = 640;

std::string TR502MSVData::device_string() const {
  switch (this->device) {
    case DEVICE_1:
      return "1";
    case DEVICE_2:
      return "2";
    case DEVICE_3:
      return "3";
    case DEVICE_4:
      return "4";
    case DEVICE_ALL:
      return "all";
    default:
      return "unknown";
  }
}

std::string TR502MSVData::command_string() const {
  switch (this->command) {
    case COMMAND_OFF:
      return "turn_off";
    case COMMAND_ON:
      return "turn_on";
    case COMMAND_INCREASE_BRIGHTNESS:
      return "increase_brightness";
    case COMMAND_DECREASE_BRIGHTNESS:
      return "decrease_brightness";
    default:
      return "unknown";
  }
}

void TR502MSVProtocol::encode(RemoteTransmitData *dst, const TR502MSVData &data) {
  dst->set_carrier_frequency(0);  // 433 MHz
  dst->reserve(41);

  dst->mark(BIT_ONE_MARK_US);

  uint32_t d = data.raw;
  for (uint32_t mask = 1; mask; mask = (mask << 1) & 0x0fffff) {
    if (d & mask) {
      dst->space(BIT_ONE_SPACE_US);
      dst->mark(BIT_ONE_MARK_US);
    } else {
      dst->space(BIT_ZERO_SPACE_US);
      dst->mark(BIT_ZERO_MARK_US);
    }
  }
}

optional<TR502MSVData> TR502MSVProtocol::decode(RemoteReceiveData src) {
  TR502MSVData out;
  uint32_t d = 0;
  if (!src.expect_mark(BIT_ONE_MARK_US))
    return {};
  for (uint32_t mask = 1; mask; mask = (mask << 1) & 0x0fffff) {
    if (src.peek_space(BIT_ONE_SPACE_US) && src.peek_mark(BIT_ONE_MARK_US, 1)) {
      d |= mask;
    } else if (!(src.peek_space(BIT_ZERO_SPACE_US) && src.peek_mark(BIT_ZERO_MARK_US, 1))) {
      return {};
    }
    src.advance(2);
  }
  out.raw = d;
  if (out.calc_cs() != out.checksum) {
    return {};
  }
  return out;
}

void TR502MSVProtocol::dump(const TR502MSVData &data) {
  ESP_LOGD(TAG, "Received TR502MSV: group=0x%03X, device=%s, command=%s", data.group, data.device_string().c_str(),
           data.command_string().c_str());
}

}  // namespace remote_base
}  // namespace esphome
