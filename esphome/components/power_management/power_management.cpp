#include "esphome/core/log.h"
#include "power_management.h"

namespace esphome {
namespace power_management {

static const char *TAG = "power_management";

PowerManagement *global_pm = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char *power_manager_type_to_string(PowerManagementLockType type) {
  switch (type) {
    case TMR:
      return "esphome_timer";
    case CPU:
      return "esphome_cpu";
    case APB:
      return "esphome_apb";
    case SLP:
      return "esphome_slp";
    default:
      return "UNKNOWN";
  }
}

const char *power_manager_user_to_string(PowerManagementLockUser user) {
  switch (user) {
    case PM:
      return "pm";
    case ACTION:
      return "action";
    case API:
      return "api";
    case OT:
      return "openthread";
    case UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

#ifndef USE_ESP_IDF
void PowerManagement::setup() {}
void PowerManagement::acquire_lock(PowerManagementLockUser user, PowerManagementLockType lt) {}
void PowerManagement::release_lock(PowerManagementLockUser user, PowerManagementLockType lt) {}
#endif

}  // namespace power_management
}  // namespace esphome
