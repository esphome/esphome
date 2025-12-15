#include "esphome/core/log.h"
#include "power_management.h"

namespace esphome::power_management {

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

#ifndef USE_ESP_IDF
void PowerManagement::setup() {}
void PowerManagement::acquire_lock(PowerManagementLockType lt) {}
void PowerManagement::release_lock(PowerManagementLockType lt) {}
#endif

}  // namespace esphome::power_management
