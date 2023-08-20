#include "climate_mitsubishi.h"

#pragma once

namespace esphome {
namespace climate_mitsubishi {
namespace mitsubishi_protocol {

const size_t PACKET_LEN = 22;

const size_t CONNECT_LEN = 8;
const uint8_t CONNECT_PACKET[CONNECT_LEN] = {0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8};

const size_t SET_REQUEST_HEADER_LEN = 6;
const uint8_t SET_REQUEST_HEADER[] = {0xfc, 0x41, 0x01, 0x30, 0x10, 0x01};

const size_t TEMPERATURE_INJECT_HEADER_LEN = 6;
const uint8_t TEMPERATURE_INJECT_HEADER[] = {0xfc, 0x41, 0x01, 0x30, 0x10, 0x07};

const size_t INFO_HEADER_LEN = 5;
const uint8_t INFO_HEADER[] = {0xfc, 0x42, 0x01, 0x30, 0x10};

enum class InfoType : uint8_t { SETTINGS = 2, ROOM_TEMP = 3, STATUS = 6, SENSORS = 9 };

enum class Offset : int {
  INFO_TYPE = 5,
  DATA_LENGTH = 4,

  SETTING_MASK_1 = 6,
  SETTING_MASK_2 = 7,
  POWER = 8,
  MODE = 9,
  TARGET_TEMP = 10,
  TARGET_TEMP_SET_05 = 19,
  TARGET_TEMP_GET_05 = 16,
  FAN = 11,
  VERTICAL_VANE = 12,
  HORIZONTAL_VANE_GET = 15,
  HORIZONTAL_VANE_SET = 18,

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

enum class SettingsMask1 : uint8_t { POWER = 0x01, MODE = 0x02, TARGET_TEMP = 0x04, FAN = 0x08, VERTICAL_VANE = 0x10 };

enum class SettingsMask2 : uint8_t { HORIZONTAL_VANE = 0x01 };

enum class Power : uint8_t { ON = 1, OFF = 0 };

enum class Mode : uint8_t { HEAT = 1, DRY = 2, COOL = 3, FAN = 7, AUTO = 8 };

enum class FanMode : uint8_t { AUTO = 0, FAN_QUIET = 1, FAN_1 = 2, FAN_2 = 3, FAN_3 = 5, FAN_4 = 6 };

enum class VerticalVaneMode : uint8_t {
  VANE_AUTO = 0,
  VANE_1 = 1,
  VANE_2 = 2,
  VANE_3 = 3,
  VANE_4 = 4,
  VANE_5 = 5,
  VANE_SWING = 7,
};

enum class HorizontalVaneMode : uint8_t {
  VANE_LEFT_2 = 0x01,
  VANE_LEFT_1 = 0x02,
  VANE_CENTER = 0x03,
  VANE_RIGHT_1 = 0x04,
  VANE_RIGHT_2 = 0x05,
  VANE_SPLIT = 0x08,
  VANE_SWING = 0x0c,
};

enum class LoopFlagsMask : uint8_t { PREHEAT = 0x04, CONFLICT = 0x08 };

}  // namespace mitsubishi_protocol
}  // namespace climate_mitsubishi
}  // namespace esphome
