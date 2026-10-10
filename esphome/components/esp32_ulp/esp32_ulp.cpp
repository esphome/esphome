#include "esp32_ulp.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <esp_sleep.h>
#include <esp_system.h>
#include <ulp_riscv.h>

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

namespace esphome::esp32_ulp {

ESPHOME_LOG_TAG(TAG, "esp32_ulp");

void ESP32ULP::setup() {
  if (this->wakeup_period_us_ > 0) {
    esp_err_t err = ulp_set_wakeup_period(0, this->wakeup_period_us_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "ulp_set_wakeup_period failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
  }
  // After a deep sleep the program is still running in RTC memory; reloading
  // it would discard its state.
  const bool from_deep_sleep = esp_reset_reason() == ESP_RST_DEEPSLEEP;
  if (this->run_on_boot_ && !from_deep_sleep && !this->load_and_run()) {
    this->mark_failed();
    return;
  }
  this->loaded_ = this->loaded_ || from_deep_sleep;
  if (this->wakeup_from_sleep_) {
    esp_err_t err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_sleep_enable_ulp_wakeup failed: %s", esp_err_to_name(err));
    }
  }
}

void ESP32ULP::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 ULP RISC-V:\n"
                "  Program size: %u bytes\n"
                "  Wakeup period: %" PRIu32 " us\n"
                "  Run on boot: %s\n"
                "  Wakeup from sleep: %s\n"
                "  Loaded: %s",
                static_cast<unsigned>(ulp_main_bin_end - ulp_main_bin_start), this->wakeup_period_us_,
                YESNO(this->run_on_boot_), YESNO(this->wakeup_from_sleep_), YESNO(this->loaded_));
}

bool ESP32ULP::load_and_run() {
  const size_t size = ulp_main_bin_end - ulp_main_bin_start;
  esp_err_t err = ulp_riscv_load_binary(ulp_main_bin_start, size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ulp_riscv_load_binary failed: %s", esp_err_to_name(err));
    return false;
  }
  err = ulp_riscv_run();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ulp_riscv_run failed: %s", esp_err_to_name(err));
    return false;
  }
  this->loaded_ = true;
  ESP_LOGD(TAG, "Loaded and started %u byte program", static_cast<unsigned>(size));
  return true;
}

void ESP32ULP::timer_stop() { ulp_riscv_timer_stop(); }

void ESP32ULP::timer_resume() { ulp_riscv_timer_resume(); }

}  // namespace esphome::esp32_ulp

#endif  // USE_ESP32
