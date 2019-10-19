#include "climate_mode.h"

namespace esphome {
namespace climate {

const char *climate_mode_to_string(ClimateMode mode) {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return "OFF";
    case CLIMATE_MODE_AUTO:
      return "AUTO";
    case CLIMATE_MODE_COOL:
      return "COOL";
    case CLIMATE_MODE_HEAT:
      return "HEAT";
    default:
      return "UNKNOWN";
  }
}
const char *climate_action_to_string(ClimateAction action) {
  switch (action) {
    case CLIMATE_ACTION_OFF:
      return "OFF";
    case CLIMATE_ACTION_COOLING:
      return "COOLING";
    case CLIMATE_ACTION_HEATING:
      return "HEATING";
    default:
      return "UNKNOWN";
  }
}

}  // namespace climate
}  // namespace esphome
