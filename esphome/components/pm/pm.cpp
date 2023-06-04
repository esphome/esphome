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
namespace pm {

static const char *TAG = "PM";

void PM::setup() {
#ifdef CONFIG_PM_ENABLE
  ESP_LOGI(TAG, "PM Support Enabled");
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

  // Disable powermanagement until startup has time to finish.
  // There may be a better way to do this
  esp_pm_lock_acquire(*this->pm_lock_);
  delay(20);  // NOLINT
  esp_pm_lock_release(*this->pm_lock_);

#else
  ESP_LOGI(TAG, "PM Support Disabled");
#endif  // CONFIG_PM_ENABLE
  App.set_loop_interval(200);
  global_pm = this;
}

void PM::disable() {
#ifdef CONFIG_PM_ENABLE
  esp_pm_config_esp32_t pm_config;
  pm_config.max_freq_mhz = 240;
  pm_config.min_freq_mhz = 240;
  pm_config.light_sleep_enable = false;
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
#endif
}

void PM::set_freq(uint16_t min_freq_mhz, uint16_t max_freq_mhz) {
  this->min_freq_ = min_freq_mhz;
  this->max_freq_ = max_freq_mhz;
}

void PM::set_tickless(bool tickless) { this->tickless_ = tickless; }

std::unique_ptr<pm::PMLock> PM::get_lock() { return make_unique<PMLock>(this->pm_lock_); }

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
  char *dumpchar;
  size_t len;
  FILE *dump = open_memstream(&dumpchar, &len);
  esp_pm_dump_locks(dump);
  fflush(dump);
  std::string tmpstr(dumpchar, len);  // length optional, but needed if there may be zero's in your data
  std::istringstream is(tmpstr);

  std::string line;
  while (std::getline(is, line)) {
    ESP_LOGD(TAG, "%s", line.c_str());
  }

#endif
}

PM *global_pm = nullptr;

}  // namespace pm
}  // namespace esphome
#endif
