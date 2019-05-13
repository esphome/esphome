#include "climate_mode.h"

namespace esphome {
namespace climate {

const char *climate_mode_to_string(ClimateMode mode) {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return "off";
    case CLIMATE_MODE_AUTO:
      return "auto";
    case CLIMATE_MODE_COOL:
      return "cool";
    case CLIMATE_MODE_HEAT:
      return "heat";
    default:
      return "UNKNOWN";
  }
}

}  // namespace climate
}  // namespace esphome
