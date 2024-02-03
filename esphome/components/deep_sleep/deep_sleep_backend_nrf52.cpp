#if defined(USE_NRF52) && defined(USE_ARDUINO)
#include "deep_sleep_backend_nrf52.h"
#include "nrf_power.h"
#include <cassert>
#include "Adafruit_TinyUSB.h"
#include "esphome/core/log.h"

namespace esphome {
namespace deep_sleep {

#define DFU_MAGIC_SKIP 0x6d

static const char *const TAG = "deep_sleep.nrf52";

void Nrf52DeepSleepBackend::begin_sleep(const optional<uint64_t> &sleep_duration) {
  // RTC works only during System On
  if (sleep_duration.has_value()) {
    // TinyUSBDevice.detach();
    // TODO deinit USB
    // TOOD and the rest of peripherals
    uint32_t start_time = millis();
    portSUPPRESS_TICKS_AND_SLEEP(ms2tick(*sleep_duration / 1000));
    last_sleep_duration_ = millis() - start_time;
    // TinyUSBDevice.attach();
  } else {
    NRF_POWER->GPREGRET = DFU_MAGIC_SKIP;
    // Enter System OFF.
#ifdef SOFTDEVICE_PRESENT
    uint8_t sd_en = 0;
    (void) sd_softdevice_is_enabled(&sd_en);
    if (sd_en) {
      uint32_t ret_code = sd_power_system_off();
      assert((ret_code == NRF_SUCCESS) || (ret_code == NRF_ERROR_SOFTDEVICE_NOT_ENABLED));
    }
#endif  // SOFTDEVICE_PRESENT
    nrf_power_system_off(NRF_POWER);
    // it should never reach here...
  }
}

void Nrf52DeepSleepBackend::dump_config() {
  if (last_sleep_duration_.has_value()) {
    ESP_LOGD(TAG, "Last sleep duration: %lu ms", *last_sleep_duration_);
  } else {
    ESP_LOGD(TAG, "Last sleep duration: unknown");
  }
}

}  // namespace deep_sleep
}  // namespace esphome

#endif
