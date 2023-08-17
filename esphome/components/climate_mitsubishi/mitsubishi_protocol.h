#include "climate_mitsubishi.h"

namespace esphome {
namespace climate_mitsubishi {
namespace mitsubishi_protocol {

const size_t packet_len = 22;

const size_t connect_len = 8;
const uint8_t connect_packet[connect_len] = {0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8};

const size_t set_request_header_len = 6;
const uint8_t set_request_header[] = {0xfc, 0x41, 0x01, 0x30, 0x10, 0x01};

const size_t temperature_inject_header_len = 6;
const uint8_t temperature_inject_header[] = {0xfc, 0x41, 0x01, 0x30, 0x10, 0x07};

const size_t info_header_len = 5;
const uint8_t info_header[] = {0xfc, 0x42, 0x01, 0x30, 0x10};

enum class InfoType : uint8_t {
  SETTINGS = 2,
  ROOM_TEMP = 3,
  STATUS = 6,
  SENSORS = 9
};

enum class Offset : int {
  INFO_TYPE = 5,
  DATA_LENGTH = 4,

  SETTING_MASK_1 = 6,
  POWER = 8,
  MODE = 9,
  TARGET_TEMP = 10,
  TARGET_TEMP_SET_05 = 19,
  TARGET_TEMP_GET_05 = 16,
  FAN = 11,
  VERTICAL_VANE = 12,

  ROOM_TEMP_05 = 11,
  ROOM_TEMP = 8,

  TEMPERATURE_INJECT_ENABLE = 6,
  TEMPERATURE_INJECT_TEMP_1 = 7,
  TEMPERATURE_INJECT_TEMP_2 = 8,

  COMPRESSOR_FREQUENCY = 8,
  OPERATING = 9,

  LOOP_FLAGS = 8,
  FAN_VELOCITY = 9,
};

enum class SettingsMask1 : uint8_t {
  POWER = 0x01,
  MODE = 0x02,
  TARGET_TEMP = 0x04,
  FAN = 0x08,
  VERTICAL_VANE = 0x10
};

enum class Power : uint8_t {
  ON = 1,
  OFF = 0
};

enum class Mode : uint8_t {
  HEAT = 1,
  DRY = 2,
  COOL = 3,
  FAN = 7,
  AUTO = 8
};

enum class FanMode : uint8_t {
  AUTO = 0,
  FAN_QUIET = 1,
  FAN_1 = 2,
  FAN_2 = 3,
  FAN_3 = 5,
  FAN_4 = 6
};

enum class VerticalVaneMode : uint8_t {
  VANE_AUTO = 0,
  VANE_1 = 1,
  VANE_2 = 2,
  VANE_3 = 3,
  VANE_4 = 4,
  VANE_5 = 5,
  VANE_EXPERIMENTAL_6 = 6,
  VANE_SWING = 7,
  VANE_EXPERIMENTAL_8 = 8,
};

enum class LoopFlagsMask : uint8_t {
  PREHEAT = 0x04,
  CONFLICT = 0x08
};

}
}
}