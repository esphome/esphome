#pragma once

#include <optional>
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_log.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esp_task_wdt.h"

#include "esp_openthread_cli.h"
#include "esp_openthread_netif_glue.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_vfs_eventfd.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace esphome {
namespace openthread {

class EspOpenThreadLockGuard {
 public:
   static std::optional<EspOpenThreadLockGuard> TryAcquire(TickType_t delay) {
     if (esp_openthread_lock_acquire(delay)) {
        return EspOpenThreadLockGuard();
     }
     return {};
   }
   static std::optional<EspOpenThreadLockGuard> Acquire() {
     while (!esp_openthread_lock_acquire(100)) {
       esp_task_wdt_reset();
     }
     return EspOpenThreadLockGuard();
   }
  ~EspOpenThreadLockGuard() { esp_openthread_lock_release(); }
  private:
  // Use a private constructor in order to force thehandling
  // of acquisition failure
  EspOpenThreadLockGuard() {}
};
}
}
