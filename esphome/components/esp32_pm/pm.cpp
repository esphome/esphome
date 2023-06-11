#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#ifdef USE_ESP_IDF
#include "esp_err.h"
#include "esp_pm.h"
#include "soc/rtc.h"
#include "freertos/task.h"

#include "esp_pm.h"
#include "esp_sleep.h"

#include "pm.h"

#include <sstream>

namespace esphome {
namespace esp32_pm {

static const char *const TAG = "PM";

void dump_locks() {
  char *dumpchar;
  size_t len;
  FILE *dump = open_memstream(&dumpchar, &len);
  esp_pm_dump_locks(dump);
  fflush(dump);
  std::string tmpstr(dumpchar, len);  // length optional, but needed if there may be zero's in your data
  std::istringstream is(tmpstr);

  std::string line;
  while (std::getline(is, line)) {
    ESP_LOGV(TAG, "%s", line.c_str());
  }
}

void ESP32PowerManagement::setup() {
#ifdef CONFIG_PM_ENABLE
  ESP_LOGI(TAG, "ESP32_PM Support Enabled");
  esp_pm_config_esp32_t pm_config;
  pm_config.max_freq_mhz = max_freq_;
  pm_config.min_freq_mhz = min_freq_;
  ESP_LOGI(TAG, "Setting Minimum frequency to %dMHz, Maximum to %dMHz", min_freq_, max_freq_);
#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
  pm_config.light_sleep_enable = this->tickless_;
  if (this->tickless_) {
    ESP_LOGI(TAG, "Tickless Idle Enabled");
  } else {
    ESP_LOGI(TAG, "Tickless Idle Disabled");
  }
#else
  ESP_LOGI(TAG, "Tickless Idle Support Disabled");
#endif
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

  esp_sleep_enable_gpio_wakeup();

  this->pm_lock_ = std::make_shared<esp_pm_lock_handle_t>();

  esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "Output Lock", this->pm_lock_.get());
  esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "Startup Lock", &this->startup_lock_);

  // Disable powermanagement until startup has time to finish.
  // There may be a better way to do this
  esp_pm_lock_acquire(this->startup_lock_);
  this->setup_done_ = true;

#else
  ESP_LOGI(TAG, "PM Support Disabled");
#endif  // CONFIG_PM_ENABLE
  App.set_loop_interval(200);
  global_pm = this;
}

void ESP32PowerManagement::loop() {
  // Disable the startup lock once we have looped once
  if (this->setup_done_) {
    esp_pm_lock_release(this->startup_lock_);
    this->setup_done_ = false;
  }
}

void ESP32PowerManagement::dump_config() {
#ifdef CONFIG_PM_ENABLE
  ESP_LOGCONFIG(TAG, "PM Support Enabled");
  ESP_LOGCONFIG(TAG, "Setting Minimum frequency to %dMHz, Maximum to %dMHz", min_freq_, max_freq_);
#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
  if (this->tickless_) {
    ESP_LOGCONFIG(TAG, "Tickless Idle Enabled");
  } else {
    ESP_LOGCONFIG(TAG, "Tickless Idle Disabled");
  }
#else
  ESP_LOGCONFIG(TAG, "Tickless Idle Support Disabled");
#endif
  dump_locks();
#else
  ESP_LOGCONFIG(TAG, "PM Support Disabled");
#endif  // CONFIG_PM_ENABLE
}

void ESP32PowerManagement::set_freq(uint16_t min_freq_mhz, uint16_t max_freq_mhz) {
  this->min_freq_ = min_freq_mhz;
  this->max_freq_ = max_freq_mhz;
}

void ESP32PowerManagement::set_tickless(bool tickless) { this->tickless_ = tickless; }

std::unique_ptr<esp32_pm::PMLock> ESP32PowerManagement::get_lock() { return make_unique<PMLock>(this->pm_lock_); }

PMLock::PMLock(std::shared_ptr<esp_pm_lock_handle_t> pm_lock) {
#ifdef CONFIG_PM_ENABLE
  pm_lock_ = std::move(pm_lock);
  esp_pm_lock_acquire(*this->pm_lock_);
  App.set_loop_interval(16);
  ESP_LOGD(TAG, "PM Lock Aquired");
#endif
}

PMLock::~PMLock() {
#ifdef CONFIG_PM_ENABLE
  esp_pm_lock_release(*this->pm_lock_);
  App.set_loop_interval(200);
  ESP_LOGD(TAG, "PM Lock Released");
  dump_locks();
#endif
}

ESP32PowerManagement *global_pm = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esp32_pm
}  // namespace esphome
#endif
