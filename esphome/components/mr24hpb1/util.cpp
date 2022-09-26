#include "mr24hpb1.h"

namespace esphome {
namespace mr24hpb1 {

const char *scene_setting_to_string(SceneSetting setting) {
  switch (setting) {
    case SCENE_DEFAULT:
      return "DEFAULT";
    case AREA:
      return "AREA";
    case BATHROOM:
      return "BATHROOM";
    case BEDROOM:
      return "BEDROOM";
    case LIVING_ROOM:
      return "LIVING_ROOM";
    case OFFICE:
      return "OFFICE";
    case HOTEL:
      return "HOTEL";
    default:
      return "UNDEFINED";
  }
}

const char *environment_status_to_string(EnvironmentStatus status) {
  switch (status) {
    case UNOCCUPIED:
      return "UNOCCUPIED";
    case STATIONARY:
      return "STATIONARY";
    case MOVING:
      return "MOVING";
    default:
      return "UNDEFINED";
  }
}

const char *forced_unoccupied_to_string(ForcedUnoccupied value) {
  switch (value) {
    case ForcedUnoccupied::NONE:
      return "None";
    case ForcedUnoccupied::SEC_10:
      return "10s";
    case ForcedUnoccupied::SEC_30:
      return "30s";
    case ForcedUnoccupied::MIN_1:
      return "1min";
    case ForcedUnoccupied::MIN_2:
      return "2min";
    case ForcedUnoccupied::MIN_5:
      return "5min";
    case ForcedUnoccupied::MIN_10:
      return "10min";
    case ForcedUnoccupied::MIN_30:
      return "30min";
    case ForcedUnoccupied::MIN_60:
      return "60min";
    default:
      return "UNDEFINED";
  }
}

const char *movement_type_to_string(MovementType type) {
  switch (type) {
    case MovementType::NONE:
      return "NONE";
    case MovementType::APPROACHING:
      return "APPROACHING";
    case MovementType::FAR_AWAY:
      return "FAR_AWAY";
    case MovementType::U1:
      return "U1";
    case MovementType::U2:
      return "U2";
    default:
      return "UNDEFINED";
  }
}
}  // namespace mr24hpb1
}  // namespace esphome
