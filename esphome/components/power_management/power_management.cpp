#include "esphome/core/log.h"
#include "power_management.h"

namespace esphome::power_management {

#ifdef USE_POWER_MANAGEMENT
const char *power_manager_type_to_string(PowerManagementLockType type) {
  switch (type) {
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
#endif

#ifndef USE_ESP32
void PowerManagement::setup() {}
void PowerManagement::loop() {}
int8_t PowerManagement::count_pm_locks_() { return -1; }
#ifdef USE_POWER_MANAGEMENT
void PowerManagement::acquire_lock(PowerManagementLockType lt) {}
void PowerManagement::release_lock(PowerManagementLockType lt) {}
#endif
#endif

}  // namespace esphome::power_management
