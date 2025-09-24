#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const char *const TAG = "light.automation";

void addressableset_warn_about_scale(const char *field) {
  ESP_LOGW(TAG, "Lambda for parameter %s of light.addressable_set should return values in range 0-1 instead of 0-255.",
           field);
}

}  // namespace light
}  // namespace esphome
