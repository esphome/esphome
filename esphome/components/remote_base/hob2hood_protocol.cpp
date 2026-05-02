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

static const int8_t LIGHT_OFF_DATA[] = {-1, 2, -1, 1, -1, 1, -1, 3, -1, 1, -1, 2, -1, 2, -1, 1, -1};
static const int8_t LIGHT_ON_DATA[] = {-1, 2, -1, 1, -2, 1, -1, 2, -1, 1, -2, 4, -1, 1, -1, 1, -2};
static const int8_t FAN_OFF_DATA[] = {-1, 2, -1, 2, -3, 2, -1, 2, -2, 3, -1, 2, -1, 1, -1};
static const int8_t FAN_LOW_DATA[] = {-2, 2, -1, 2, -3, 2, -1, 2, -1, 1, -1, 2, -1, 3, -1};
static const int8_t FAN_MEDIUM_DATA[] = {-2, 2, -1, 4, -1, 3, -4, 3, -3};
static const int8_t FAN_HIGH_DATA[] = {-1, 3, -4, 4, -3, 1, -1, 3, -3};
static const int8_t FAN_MAX_DATA[] = {-2, 3, -2, 1, -2, 3, -2, 2, -1, 3, -1, 1, -2};

void Hob2HoodProtocol::encode_data_(RemoteTransmitData *dst, const int8_t *data, size_t length) const {
  dst->reserve(length);
  for (size_t i = 0; i < length; i++) {
    switch (data[i]) {
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
      this->encode_data_(dst, LIGHT_OFF_DATA, sizeof(LIGHT_OFF_DATA));
      break;
    case HOB2HOOD_CMD_LIGHT_ON:
      this->encode_data_(dst, LIGHT_ON_DATA, sizeof(LIGHT_ON_DATA));
      break;
    case HOB2HOOD_CMD_FAN_OFF:
      this->encode_data_(dst, FAN_OFF_DATA, sizeof(FAN_OFF_DATA));
      break;
    case HOB2HOOD_CMD_FAN_LOW:
      this->encode_data_(dst, FAN_LOW_DATA, sizeof(FAN_LOW_DATA));
      break;
    case HOB2HOOD_CMD_FAN_MEDIUM:
      this->encode_data_(dst, FAN_MEDIUM_DATA, sizeof(FAN_MEDIUM_DATA));
      break;
    case HOB2HOOD_CMD_FAN_HIGH:
      this->encode_data_(dst, FAN_HIGH_DATA, sizeof(FAN_HIGH_DATA));
      break;
    case HOB2HOOD_CMD_FAN_MAX:
      this->encode_data_(dst, FAN_MAX_DATA, sizeof(FAN_MAX_DATA));
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

bool Hob2HoodProtocol::expect_data_(RemoteReceiveData &src, const int8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (!this->expect_data_(src, data[i]))
      return false;
  }
  return true;
}

optional<Hob2HoodData> Hob2HoodProtocol::decode(RemoteReceiveData src) {
  if (this->expect_data_(src, LIGHT_OFF_DATA, sizeof(LIGHT_OFF_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_LIGHT_OFF);
  }
  src.reset();
  if (this->expect_data_(src, LIGHT_ON_DATA, sizeof(LIGHT_ON_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_LIGHT_ON);
  }
  src.reset();
  if (this->expect_data_(src, FAN_OFF_DATA, sizeof(FAN_OFF_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_FAN_OFF);
  }
  src.reset();
  if (this->expect_data_(src, FAN_LOW_DATA, sizeof(FAN_LOW_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_FAN_LOW);
  }
  src.reset();
  if (this->expect_data_(src, FAN_MEDIUM_DATA, sizeof(FAN_MEDIUM_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_FAN_MEDIUM);
  }
  src.reset();
  if (this->expect_data_(src, FAN_HIGH_DATA, sizeof(FAN_HIGH_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_FAN_HIGH);
  }
  src.reset();
  if (this->expect_data_(src, FAN_MAX_DATA, sizeof(FAN_MAX_DATA))) {
    return Hob2HoodData(HOB2HOOD_CMD_FAN_MAX);
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
