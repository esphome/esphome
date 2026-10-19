#include "esp32_ulp.h"

#if defined(USE_ESP32) && (SOC_RISCV_COPROC_SUPPORTED || SOC_LP_CORE_SUPPORTED)

#include "esphome/core/log.h"

#include <esp_attr.h>
#include <esp_sleep.h>
#include <esp_system.h>
#if SOC_RISCV_COPROC_SUPPORTED
#include <ulp_riscv.h>
#endif

extern const uint8_t ULP_MAIN_BIN_START[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ULP_MAIN_BIN_END[] asm("_binary_ulp_main_bin_end");

namespace esphome::esp32_ulp {

ESPHOME_LOG_TAG(TAG, "esp32_ulp");

// Survives deep sleep (RTC memory) so a program loaded before sleeping is known
// to still be running on wake, however it was started.
static RTC_DATA_ATTR bool s_program_loaded = false;

#if SOC_RISCV_COPROC_SUPPORTED
static constexpr const char *BACKEND = "ULP RISC-V";
#else
static constexpr const char *BACKEND = "LP core";
#endif

void ESP32ULP::setup() {
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP) {
    ESP_LOGI(TAG, "Woken from deep sleep by the %s", BACKEND);
  }
#if SOC_RISCV_COPROC_SUPPORTED
  if (this->wakeup_period_us_ > 0) {
    esp_err_t err = ulp_set_wakeup_period(0, this->wakeup_period_us_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "ulp_set_wakeup_period failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
  }
#endif
  // After a deep sleep the program is still running in RTC memory; reloading
  // it would discard its state.
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP && s_program_loaded) {
    this->loaded_ = true;
  } else if (this->run_on_boot_ && !this->load_and_run()) {
    this->mark_failed();
    return;
  }
  if (this->wakeup_from_sleep_) {
    esp_err_t err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_sleep_enable_ulp_wakeup failed: %s", esp_err_to_name(err));
    }
  }
}

void ESP32ULP::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 ULP:\n"
                "  Coprocessor: %s\n"
                "  Program size: %u bytes\n"
                "  Wakeup period: %" PRIu32 " us\n"
                "  Run on boot: %s\n"
                "  Wakeup from sleep: %s\n"
                "  Loaded: %s",
                BACKEND, static_cast<unsigned>(ULP_MAIN_BIN_END - ULP_MAIN_BIN_START), this->wakeup_period_us_,
                YESNO(this->run_on_boot_), YESNO(this->wakeup_from_sleep_), YESNO(this->loaded_));
}

bool ESP32ULP::load_and_run() {
  const size_t size = ULP_MAIN_BIN_END - ULP_MAIN_BIN_START;
#if SOC_RISCV_COPROC_SUPPORTED
  // ulp_riscv_load_binary() overwrites RTC memory without stopping a running
  // program; the LP core variant stops the core itself.
  ulp_riscv_halt();
  esp_err_t err = ulp_riscv_load_binary(ULP_MAIN_BIN_START, size);
#else
  esp_err_t err = ulp_lp_core_load_binary(ULP_MAIN_BIN_START, size);
#endif
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Loading the program failed: %s", esp_err_to_name(err));
    return false;
  }
  if (!this->run_()) {
    return false;
  }
  this->loaded_ = true;
  s_program_loaded = true;
  ESP_LOGD(TAG, "Loaded and started %u byte program", static_cast<unsigned>(size));
  return true;
}

bool ESP32ULP::run_() {
#if SOC_RISCV_COPROC_SUPPORTED
  esp_err_t err = ulp_riscv_run();
#else
  ulp_lp_core_cfg_t cfg = {};
  cfg.wakeup_source = this->wakeup_sources_;
  cfg.lp_timer_sleep_duration_us = this->wakeup_period_us_;
  esp_err_t err = ulp_lp_core_run(&cfg);
#endif
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Starting the program failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void ESP32ULP::timer_stop() {
#if SOC_RISCV_COPROC_SUPPORTED
  ulp_riscv_timer_stop();
#else
  ulp_lp_core_stop();
#endif
}

void ESP32ULP::timer_resume() {
  if (!this->loaded_) {
    return;
  }
#if SOC_RISCV_COPROC_SUPPORTED
  ulp_riscv_timer_resume();
#else
  this->run_();
#endif
}

}  // namespace esphome::esp32_ulp

#endif  // USE_ESP32 && (SOC_RISCV_COPROC_SUPPORTED || SOC_LP_CORE_SUPPORTED)
