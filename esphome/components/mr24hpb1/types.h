#pragma once
#include <cstdint>

namespace esphome {
namespace mr24hpb1 {

using FunctionCode = uint8_t;
using AddressCode1 = uint8_t;
using AddressCode2 = uint8_t;

enum SceneSetting {
  SCENE_DEFAULT = 0x00,
  AREA = 0x01,
  BATHROOM = 0x02,
  BEDROOM = 0x03,
  LIVING_ROOM = 0x04,
  OFFICE = 0x05,
  HOTEL = 0x06
};

const char *scene_setting_to_string(SceneSetting setting);

enum EnvironmentStatus { UNOCCUPIED = 0x00FFFF, STATIONARY = 0x0100FF, MOVING = 0x010101 };

const char *environment_status_to_string(EnvironmentStatus status);

enum class MovementType { NONE = 0x01, APPROACHING = 0x02, FAR_AWAY = 0x03, U1 = 0x04, U2 = 0x05 };

const char *movement_type_to_string(MovementType type);

enum class ForcedUnoccupied {
  NONE = 0x00,
  SEC_10 = 0x01,
  SEC_30 = 0x02,
  MIN_1 = 0x03,
  MIN_2 = 0x04,
  MIN_5 = 0x05,
  MIN_10 = 0x06,
  MIN_30 = 0x07,
  MIN_60 = 0x08
};

enum class BreathingSigns {
  NORMAL = 0x00,
  BREATHING_ABNORMALLY = 0x01,
  NO_SIGNAL = 0x02,
  MOVEMENT_ANOMALY = 0x04,
  SHORTNESS_OF_BREATH = 0x05
};
enum class BedOccupation { OUT_OF_BED = 0x00, IN_BED = 0x01, NA = 0x02 };
enum class SleepState { AWAKE = 0x00, LIGHT_SLEEP = 0x01, DEEP_SLEEP = 0x02, NA = 0x03 };
}  // namespace mr24hpb1
}  // namespace esphome
