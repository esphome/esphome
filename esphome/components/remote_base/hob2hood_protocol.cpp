#include "hob2hood_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.hob2hood";

static const int32_t MARK_1_US = 950;
static const int32_t MARK_2_US = 1700;
static const int32_t MARK_3_US = 2450;
static const int32_t MARK_4_US = 3400;
static const int32_t SPACE_1_US = 550;
static const int32_t SPACE_2_US = 1200;
static const int32_t SPACE_3_US = 1900;
static const int32_t SPACE_4_US = 2600;
static const int32_t MARK_THRESHOLD_0_US = 600;
static const int32_t MARK_THRESHOLD_1_US = 1415;
static const int32_t MARK_THRESHOLD_2_US = 2085;
static const int32_t MARK_THRESHOLD_3_US = 2815;
static const int32_t MARK_THRESHOLD_4_US = 4100;
static const int32_t SPACE_THRESHOLD_0_US = 30;
static const int32_t SPACE_THRESHOLD_1_US = 785;
static const int32_t SPACE_THRESHOLD_2_US = 1535;
static const int32_t SPACE_THRESHOLD_3_US = 2300;
static const int32_t SPACE_THRESHOLD_4_US = 3100;

static const std::vector<int8_t> L0_DATA = {-1, 2, -1, 1, -1, 1, -1, 3, -1, 1, -1, 2, -1, 2, -1, 1, -1};
static const std::vector<int8_t> L1_DATA = {-1, 2, -1, 1, -2, 1, -1, 2, -1, 1, -2, 4, -1, 1, -1, 1, -2};
static const std::vector<int8_t> F0_DATA = {-1, 2, -1, 2, -3, 2, -1, 2, -2, 3, -1, 2, -1, 1, -1};
static const std::vector<int8_t> F1_DATA = {-2, 2, -1, 2, -3, 2, -1, 2, -1, 1, -1, 2, -1, 3, -1};
static const std::vector<int8_t> F2_DATA = {-2, 2, -1, 4, -1, 3, -4, 3, -3};
static const std::vector<int8_t> F3_DATA = {-1, 3, -4, 4, -3, 1, -1, 3, -3};
static const std::vector<int8_t> F4_DATA = {-2, 3, -2, 1, -2, 3, -2, 2, -1, 3, -1, 1, -2};

void Hob2HoodProtocol::encode_data_(RemoteTransmitData *dst, const std::vector<int8_t> &data) const {
  dst->reserve(data.size());
  for (const int8_t &d : data) {
    switch (d) {
      case -1:
        dst->mark(MARK_1_US);
        break;
      case -2:
        dst->mark(MARK_2_US);
        break;
      case -3:
        dst->mark(MARK_3_US);
        break;
      case -4:
        dst->mark(MARK_4_US);
        break;
      case 1:
        dst->space(SPACE_1_US);
        break;
      case 2:
        dst->space(SPACE_2_US);
        break;
      case 3:
        dst->space(SPACE_3_US);
        break;
      case 4:
        dst->space(SPACE_4_US);
        break;
    }
  }
}

void Hob2HoodProtocol::encode(RemoteTransmitData *dst, const Hob2HoodData &data) {
  dst->set_carrier_frequency(38000);
  switch (data.command) {
    case HOB2HOOD_CMD_LIGHT_OFF:
      encode_data_(dst, L0_DATA);
      break;
    case HOB2HOOD_CMD_LIGHT_ON:
      encode_data_(dst, L1_DATA);
      break;
    case HOB2HOOD_CMD_FAN_OFF:
      encode_data_(dst, F0_DATA);
      break;
    case HOB2HOOD_CMD_FAN_LOW:
      encode_data_(dst, F1_DATA);
      break;
    case HOB2HOOD_CMD_FAN_MEDIUM:
      encode_data_(dst, F2_DATA);
      break;
    case HOB2HOOD_CMD_FAN_HIGH:
      encode_data_(dst, F3_DATA);
      break;
    case HOB2HOOD_CMD_FAN_MAX:
      encode_data_(dst, F4_DATA);
      break;
    default:
      break;
  }
}

bool Hob2HoodProtocol::expect_data_(RemoteReceiveData &src, int8_t data) {
  switch (data) {
    case -1:
      return src.expect_mark(MARK_THRESHOLD_0_US, MARK_THRESHOLD_1_US);
    case -2:
      return src.expect_mark(MARK_THRESHOLD_1_US, MARK_THRESHOLD_2_US);
    case -3:
      return src.expect_mark(MARK_THRESHOLD_2_US, MARK_THRESHOLD_3_US);
    case -4:
      return src.expect_mark(MARK_THRESHOLD_3_US, MARK_THRESHOLD_4_US);
    case 1:
      return src.expect_space(SPACE_THRESHOLD_0_US, SPACE_THRESHOLD_1_US);
    case 2:
      return src.expect_space(SPACE_THRESHOLD_1_US, SPACE_THRESHOLD_2_US);
    case 3:
      return src.expect_space(SPACE_THRESHOLD_2_US, SPACE_THRESHOLD_3_US);
    case 4:
      return src.expect_space(SPACE_THRESHOLD_3_US, SPACE_THRESHOLD_4_US);
    default:
      return false;
  }
}

bool Hob2HoodProtocol::expect_data_(RemoteReceiveData &src, const std::vector<int8_t> &data) {
  for (const int8_t &d : data) {
    if (!expect_data_(src, d))
      return false;
  }
  return true;
}

optional<Hob2HoodData> Hob2HoodProtocol::decode(RemoteReceiveData src) {
  if (this->expect_data_(src, -1)) {
    if (this->expect_data_(src, 2) && this->expect_data_(src, -1)) {
      if (this->expect_data_(src, 1)) {
        if (this->expect_data_(src, std::vector<int8_t>(L0_DATA.begin() + 4, L0_DATA.end()))) {
          // L0: -1 2 -1 1 -1 ...
          return Hob2HoodData(HOB2HOOD_CMD_LIGHT_OFF);
        } else if (this->expect_data_(src, std::vector<int8_t>(L1_DATA.begin() + 4, L1_DATA.end()))) {
          // L1: -1 2 -1 1 -2 ...
          return Hob2HoodData(HOB2HOOD_CMD_LIGHT_ON);
        }
      } else if (this->expect_data_(src, std::vector<int8_t>(F0_DATA.begin() + 3, F0_DATA.end()))) {
        // F0: -1 2 -1 2 ...
        return Hob2HoodData(HOB2HOOD_CMD_FAN_OFF);
      }
    } else if (this->expect_data_(src, std::vector<int8_t>(F3_DATA.begin() + 1, F3_DATA.end()))) {
      // F3: -1 3 ...
      return Hob2HoodData(HOB2HOOD_CMD_FAN_HIGH);
    }
  } else if (this->expect_data_(src, -2)) {
    if (this->expect_data_(src, 2) && this->expect_data_(src, -1)) {
      if (this->expect_data_(src, std::vector<int8_t>(F1_DATA.begin() + 3, F1_DATA.end()))) {
        // F1: -2 2 -1 2 ...
        return Hob2HoodData(HOB2HOOD_CMD_FAN_LOW);
      } else if (this->expect_data_(src, std::vector<int8_t>(F2_DATA.begin() + 3, F2_DATA.end()))) {
        // F2: -2 2 -1 4 ...
        return Hob2HoodData(HOB2HOOD_CMD_FAN_MEDIUM);
      }
    } else if (this->expect_data_(src, std::vector<int8_t>(F4_DATA.begin() + 1, F4_DATA.end()))) {
      // F4: -2 3 ...
      return Hob2HoodData(HOB2HOOD_CMD_FAN_MAX);
    }
  }
  return {};
}

void Hob2HoodProtocol::dump(const Hob2HoodData &data) {
  ESP_LOGD(TAG, "Received Hob2Hood: %s", data.to_string().c_str());
}

}  // namespace remote_base
}  // namespace esphome
