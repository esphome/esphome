#include "hob2hood_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.hob2hood";

static constexpr int32_t MARK_1_US = 950;
static constexpr int32_t MARK_2_US = 1700;
static constexpr int32_t MARK_3_US = 2450;
static constexpr int32_t MARK_4_US = 3400;
static constexpr int32_t SPACE_1_US = 550;
static constexpr int32_t SPACE_2_US = 1200;
static constexpr int32_t SPACE_3_US = 1900;
static constexpr int32_t SPACE_4_US = 2600;

static const std::vector<int8_t> LIGHT_OFF_DATA = {-1, 2, -1, 1, -1, 1, -1, 3, -1, 1, -1, 2, -1, 2, -1, 1, -1};
static const std::vector<int8_t> LIGHT_ON_DATA = {-1, 2, -1, 1, -2, 1, -1, 2, -1, 1, -2, 4, -1, 1, -1, 1, -2};
static const std::vector<int8_t> FAN_OFF_DATA = {-1, 2, -1, 2, -3, 2, -1, 2, -2, 3, -1, 2, -1, 1, -1};
static const std::vector<int8_t> FAN_1_DATA = {-2, 2, -1, 2, -3, 2, -1, 2, -1, 1, -1, 2, -1, 3, -1};
static const std::vector<int8_t> FAN_2_DATA = {-2, 2, -1, 4, -1, 3, -4, 3, -3};
static const std::vector<int8_t> FAN_3_DATA = {-1, 3, -4, 4, -3, 1, -1, 3, -3};
static const std::vector<int8_t> FAN_4_DATA = {-2, 3, -2, 1, -2, 3, -2, 2, -1, 3, -1, 1, -2};

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
      encode_data_(dst, LIGHT_OFF_DATA);
      break;
    case HOB2HOOD_CMD_LIGHT_ON:
      encode_data_(dst, LIGHT_ON_DATA);
      break;
    case HOB2HOOD_CMD_FAN_OFF:
      encode_data_(dst, FAN_OFF_DATA);
      break;
    case HOB2HOOD_CMD_FAN_LOW:
      encode_data_(dst, FAN_1_DATA);
      break;
    case HOB2HOOD_CMD_FAN_MEDIUM:
      encode_data_(dst, FAN_2_DATA);
      break;
    case HOB2HOOD_CMD_FAN_HIGH:
      encode_data_(dst, FAN_3_DATA);
      break;
    case HOB2HOOD_CMD_FAN_MAX:
      encode_data_(dst, FAN_4_DATA);
      break;
    default:
      break;
  }
}

bool Hob2HoodProtocol::expect_data_(RemoteReceiveData &src, int8_t data) {
  switch (data) {
    case -1:
      return src.expect_mark(MARK_1_US);
    case -2:
      return src.expect_mark(MARK_2_US);
    case -3:
      return src.expect_mark(MARK_3_US);
    case -4:
      return src.expect_mark(MARK_4_US);
    case 1:
      return src.expect_space(SPACE_1_US);
    case 2:
      return src.expect_space(SPACE_2_US);
    case 3:
      return src.expect_space(SPACE_3_US);
    case 4:
      return src.expect_space(SPACE_4_US);
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
        if (this->expect_data_(src, std::vector<int8_t>(LIGHT_OFF_DATA.begin() + 4, LIGHT_OFF_DATA.end()))) {
          // LIGHT_OFF: -1 2 -1 1 -1 ...
          return Hob2HoodData(HOB2HOOD_CMD_LIGHT_OFF);
        } else if (this->expect_data_(src, std::vector<int8_t>(LIGHT_ON_DATA.begin() + 4, LIGHT_ON_DATA.end()))) {
          // LIGHT_ON: -1 2 -1 1 -2 ...
          return Hob2HoodData(HOB2HOOD_CMD_LIGHT_ON);
        }
      } else if (this->expect_data_(src, std::vector<int8_t>(FAN_OFF_DATA.begin() + 3, FAN_OFF_DATA.end()))) {
        // FAN_OFF: -1 2 -1 2 ...
        return Hob2HoodData(HOB2HOOD_CMD_FAN_OFF);
      }
    } else if (this->expect_data_(src, std::vector<int8_t>(FAN_3_DATA.begin() + 1, FAN_3_DATA.end()))) {
      // FAN_3: -1 3 ...
      return Hob2HoodData(HOB2HOOD_CMD_FAN_HIGH);
    }
  } else if (this->expect_data_(src, -2)) {
    if (this->expect_data_(src, 2) && this->expect_data_(src, -1)) {
      if (this->expect_data_(src, std::vector<int8_t>(FAN_1_DATA.begin() + 3, FAN_1_DATA.end()))) {
        // FAN_1: -2 2 -1 2 ...
        return Hob2HoodData(HOB2HOOD_CMD_FAN_LOW);
      } else if (this->expect_data_(src, std::vector<int8_t>(FAN_2_DATA.begin() + 3, FAN_2_DATA.end()))) {
        // FAN_2: -2 2 -1 4 ...
        return Hob2HoodData(HOB2HOOD_CMD_FAN_MEDIUM);
      }
    } else if (this->expect_data_(src, std::vector<int8_t>(FAN_4_DATA.begin() + 1, FAN_4_DATA.end()))) {
      // FAN_4: -2 3 ...
      return Hob2HoodData(HOB2HOOD_CMD_FAN_MAX);
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
