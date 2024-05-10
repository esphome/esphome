#include "debug_component.h"
#ifdef USE_ZEPHYR
#include <climits>
#include "esphome/core/log.h"
#include <zephyr/drivers/hwinfo.h>
#include <hal/nrf_power.h>

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

static void set_reset_reason(std::string &reset_reason, bool set, const char *reason) {
  if (!set) {
    return;
  }
  if (reset_reason.size()) {
    reset_reason += ", ";
  }
  reset_reason += reason;
}

std::string DebugComponent::get_reset_reason_() {
  uint32_t cause;
  auto ret = hwinfo_get_reset_cause(&cause);
  if (ret) {
    ESP_LOGE(TAG, "Unable to get reset cause: %d", ret);
    return "";
  }
  std::string reset_reason;

  set_reset_reason(reset_reason, cause & RESET_PIN, "External pin");
  set_reset_reason(reset_reason, cause & RESET_SOFTWARE, "Software reset");
  set_reset_reason(reset_reason, cause & RESET_BROWNOUT, "Brownout (drop in voltage)");
  set_reset_reason(reset_reason, cause & RESET_POR, "Power-on reset (POR)");
  set_reset_reason(reset_reason, cause & RESET_WATCHDOG, "Watchdog timer expiration");
  set_reset_reason(reset_reason, cause & RESET_DEBUG, "Debug event");
  set_reset_reason(reset_reason, cause & RESET_SECURITY, "Security violation");
  set_reset_reason(reset_reason, cause & RESET_LOW_POWER_WAKE, "Waking up from low power mode");
  set_reset_reason(reset_reason, cause & RESET_CPU_LOCKUP, "CPU lock-up detected");
  set_reset_reason(reset_reason, cause & RESET_PARITY, "Parity error");
  set_reset_reason(reset_reason, cause & RESET_PLL, "PLL error");
  set_reset_reason(reset_reason, cause & RESET_CLOCK, "Clock error");
  set_reset_reason(reset_reason, cause & RESET_HARDWARE, "Hardware reset");
  set_reset_reason(reset_reason, cause & RESET_USER, "User reset");
  set_reset_reason(reset_reason, cause & RESET_TEMPERATURE, "Temperature reset");

  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason.c_str());
  return reset_reason;
}

uint32_t DebugComponent::get_free_heap_() { return INT_MAX; }

void DebugComponent::get_device_info_(std::string &device_info) {
  std::string supply = "Main supply status: ";
  if (nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_NORMAL) {
    supply += "Normal voltage.";
  } else {
    supply += "High voltage.";
  }
  ESP_LOGD(TAG, "%s", supply.c_str());
  device_info += supply;
}

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif

//   ESP_LOGD(TAG, "bootloader version %lu.%lu.%lu", (bootloaderVersion >> 16) & 0xFF, (bootloaderVersion >> 8) & 0xFF,
//            bootloaderVersion & 0xFF);
//   ESP_LOGD(TAG, "MBR bootloader addr 0x%08lx, UICR bootloader addr 0x%08lx", (*((uint32_t *) MBR_BOOTLOADER_ADDR)),
//            NRF_UICR->NRFFW[0]);
//   ESP_LOGD(TAG, "MBR param page addr 0x%08lx, UICR param page addr 0x%08lx", (*((uint32_t *) MBR_PARAM_PAGE_ADDR)),
//            NRF_UICR->NRFFW[1]);
