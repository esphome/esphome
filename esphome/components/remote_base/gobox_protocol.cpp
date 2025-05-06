#include "gobox_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.gobox";

constexpr uint32_t BIT_MARK_US = 580;  // 70us seems like a safe time delta for the receiver...
constexpr uint32_t BIT_ONE_SPACE_US = 1640;
constexpr uint32_t BIT_ZERO_SPACE_US = 545;
constexpr uint64_t HEADER = 0b011001001100010uL;  // 15 bits
constexpr uint64_t HEADER_SIZE = 15;
constexpr uint64_t CODE_SIZE = 17;

void GoboxProtocol::dump_timings_(const RawTimings &timings) const {
  ESP_LOGD(TAG, "Gobox: size=%u", timings.size());
  for (int32_t timing : timings) {
    ESP_LOGD(TAG, "Gobox: timing=%ld", (long) timing);
  }
}

void GoboxProtocol::encode(RemoteTransmitData *dst, const GoboxData &data) {
  ESP_LOGI(TAG, "Send Gobox: code=0x%x", data.code);
  dst->set_carrier_frequency(38000);
  dst->reserve((HEADER_SIZE + CODE_SIZE + 1) * 2);
  uint64_t code = (HEADER << CODE_SIZE) | (data.code & ((1UL << CODE_SIZE) - 1));
  ESP_LOGI(TAG, "Send Gobox: code=0x%Lx", code);
  for (int16_t i = (HEADER_SIZE + CODE_SIZE - 1); i >= 0; i--) {
    if (code & ((uint64_t) 1 << i)) {
      dst->item(BIT_MARK_US, BIT_ONE_SPACE_US);
    } else {
      dst->item(BIT_MARK_US, BIT_ZERO_SPACE_US);
    }
  }
  dst->item(BIT_MARK_US, 2000);

  dump_timings_(dst->get_data());
}

optional<GoboxData> GoboxProtocol::decode(RemoteReceiveData src) {
  if (src.size() < ((HEADER_SIZE + CODE_SIZE) * 2 + 1)) {
    return {};
  }

  // First check for the header
  uint64_t code = HEADER;
  for (int16_t i = HEADER_SIZE - 1; i >= 0; i--) {
    if (code & ((uint64_t) 1 << i)) {
      if (!src.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US)) {
        return {};
      }
    } else {
      if (!src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US)) {
        return {};
      }
    }
  }

  // Next, build up the code
  code = 0UL;
  for (int16_t i = CODE_SIZE - 1; i >= 0; i--) {
    if (!src.expect_mark(BIT_MARK_US)) {
      return {};
    }
    if (src.expect_space(BIT_ONE_SPACE_US)) {
      code |= (1UL << i);
    } else if (!src.expect_space(BIT_ZERO_SPACE_US)) {
      return {};
    }
  }

  if (!src.expect_mark(BIT_MARK_US)) {
    return {};
  }

  dump_timings_(src.get_raw_data());

  GoboxData out;
  out.code = code;

  return out;
}

void GoboxProtocol::dump(const GoboxData &data) {
  ESP_LOGI(TAG, "Received Gobox: code=0x%x", data.code);
  switch (data.code) {
    case GOBOX_MENU:
      ESP_LOGI(TAG, "Received Gobox: key=MENU");
      break;
    case GOBOX_RETURN:
      ESP_LOGI(TAG, "Received Gobox: key=RETURN");
      break;
    case GOBOX_UP:
      ESP_LOGI(TAG, "Received Gobox: key=UP");
      break;
    case GOBOX_LEFT:
      ESP_LOGI(TAG, "Received Gobox: key=LEFT");
      break;
    case GOBOX_RIGHT:
      ESP_LOGI(TAG, "Received Gobox: key=RIGHT");
      break;
    case GOBOX_DOWN:
      ESP_LOGI(TAG, "Received Gobox: key=DOWN");
      break;
    case GOBOX_OK:
      ESP_LOGI(TAG, "Received Gobox: key=OK");
      break;
    case GOBOX_TOGGLE:
      ESP_LOGI(TAG, "Received Gobox: key=TOGGLE");
      break;
    case GOBOX_PROFILE:
      ESP_LOGI(TAG, "Received Gobox: key=PROFILE");
      break;
    case GOBOX_FASTER:
      ESP_LOGI(TAG, "Received Gobox: key=FASTER");
      break;
    case GOBOX_SLOWER:
      ESP_LOGI(TAG, "Received Gobox: key=SLOWER");
      break;
    case GOBOX_LOUDER:
      ESP_LOGI(TAG, "Received Gobox: key=LOUDER");
      break;
    case GOBOX_SOFTER:
      ESP_LOGI(TAG, "Received Gobox: key=SOFTER");
      break;
  }
}

}  // namespace remote_base
}  // namespace esphome
