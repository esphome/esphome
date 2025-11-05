#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ZEPHYR)
#include "openthread.h"

#include <openthread/instance.h>
#include <openthread/logging.h>
#include <openthread/thread.h>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "openthread";

namespace esphome {
namespace openthread {

// Zephyr OpenThread uses a different approach - instance is managed by Zephyr
extern "C" {
otInstance *openthread_get_default_instance(void);
}

void OpenThreadComponent::setup() {
  ESP_LOGI(TAG, "OpenThread setup for Zephyr");
  // Zephyr manages OpenThread initialization through devicetree/Kconfig
  // No explicit initialization needed here
}

OpenThreadComponent::~OpenThreadComponent() {
  // Cleanup if needed
}

void OpenThreadComponent::ot_main() {
  // Zephyr handles the OpenThread main loop internally
  // This method is called for ESP32 but not needed for Zephyr
}

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;

  auto lock = InstanceLock::try_acquire(100);
  if (!lock) {
    ESP_LOGW(TAG, "Failed to acquire OpenThread lock in get_ip_addresses");
    return addresses;
  }

  otInstance *instance = lock->get_instance();
  if (instance == nullptr) {
    return addresses;
  }

  // Get IPv6 addresses from OpenThread
  const otNetifAddress *unicast_addresses = otIp6GetUnicastAddresses(instance);
  int index = 0;
  for (const otNetifAddress *addr = unicast_addresses; addr && index < 5; addr = addr->mNext) {
    // For now, we only support IPv4 in the IPAddress wrapper for Zephyr
    // Store the first address for compatibility
    if (index == 0) {
      // addresses[0] would be for IPv4, leave it empty for now
    }
    index++;
  }

  return addresses;
}

std::optional<InstanceLock> InstanceLock::try_acquire(int delay) {
  // Zephyr OpenThread doesn't use explicit locking in the same way
  // The instance access is thread-safe via Zephyr's mechanisms
  return InstanceLock();
}

InstanceLock InstanceLock::acquire() {
  // Zephyr OpenThread doesn't use explicit locking in the same way
  return InstanceLock();
}

otInstance *InstanceLock::get_instance() { return openthread_get_default_instance(); }

InstanceLock::~InstanceLock() {
  // No explicit unlock needed for Zephyr
}

}  // namespace openthread
}  // namespace esphome
#endif
