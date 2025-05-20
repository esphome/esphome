#include "debug_component.h"

#ifdef USE_ESP32
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <esp_sleep.h>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_partition.h>

#include <map>

#ifdef USE_ARDUINO
#include <Esp.h>
#endif

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

// index by values returned by esp_reset_reason

static const char *const RESET_REASONS[] = {
    "unknown source",
    "power-on event",
    "external pin",
    "software via esp_restart",
    "exception/panic",
    "interrupt watchdog",
    "task watchdog",
    "other watchdogs",
    "exiting deep sleep mode",
    "brownout",
    "SDIO",
    "USB peripheral",
    "JTAG",
    "efuse error",
    "power glitch detected",
    "CPU lock up",
};

static const char *const REBOOT_KEY = "reboot_source";
static const size_t REBOOT_MAX_LEN = 24;

// on shutdown, store the source of the reboot request
void DebugComponent::on_shutdown() {
  auto *component = App.get_current_component();
  char buffer[REBOOT_MAX_LEN]{};
  auto pref = global_preferences->make_preference(REBOOT_MAX_LEN, fnv1_hash(REBOOT_KEY + App.get_name()));
  if (component != nullptr) {
    strncpy(buffer, component->get_component_source(), REBOOT_MAX_LEN - 1);
  }
  ESP_LOGD(TAG, "Storing reboot source: %s", buffer);
  pref.save(&buffer);
  global_preferences->sync();
}

std::string DebugComponent::get_reset_reason_() {
  std::string reset_reason;
  unsigned reason = esp_reset_reason();
  if (reason < sizeof(RESET_REASONS) / sizeof(RESET_REASONS[0])) {
    reset_reason = RESET_REASONS[reason];
    if (reason == ESP_RST_SW) {
      auto pref = global_preferences->make_preference(REBOOT_MAX_LEN, fnv1_hash(REBOOT_KEY + App.get_name()));
      char buffer[REBOOT_MAX_LEN]{};
      if (pref.load(&buffer)) {
        reset_reason = "Reboot request from " + std::string(buffer);
      }
    }
  } else {
    reset_reason = "unknown source";
  }
  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason.c_str());
  return reset_reason;
}

static const char *const WAKEUP_CAUSES[] = {
    "undefined",
    "undefined",
    "external signal using RTC_IO",
    "external signal using RTC_CNTL",
    "timer",
    "touchpad",
    "ULP program",
    "GPIO",
    "UART",
    "WIFI",
    "COCPU int",
    "COCPU crash",
    "BT",
};

std::string DebugComponent::get_wakeup_cause_() {
  const char *wake_reason;
  unsigned reason = esp_sleep_get_wakeup_cause();
  if (reason < sizeof(WAKEUP_CAUSES) / sizeof(WAKEUP_CAUSES[0])) {
    wake_reason = WAKEUP_CAUSES[reason];
  } else {
    wake_reason = "unknown source";
  }
  ESP_LOGD(TAG, "Wakeup Reason: %s", wake_reason);
  return wake_reason;
}

void DebugComponent::log_partition_info_() {
  ESP_LOGCONFIG(TAG, "Partition table:");
  ESP_LOGCONFIG(TAG, "  %-12s %-4s %-8s %-10s %-10s", "Name", "Type", "Subtype", "Address", "Size");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it != NULL) {
    const esp_partition_t *partition = esp_partition_get(it);
    ESP_LOGCONFIG(TAG, "  %-12s %-4d %-8d 0x%08" PRIX32 " 0x%08" PRIX32, partition->label, partition->type,
                  partition->subtype, partition->address, partition->size);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

uint32_t DebugComponent::get_free_heap_() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

static const std::map<int, const char *> CHIP_FEATURES = {
    {CHIP_FEATURE_BLE, "BLE"},
    {CHIP_FEATURE_BT, "BT"},
    {CHIP_FEATURE_EMB_FLASH, "EMB Flash"},
    {CHIP_FEATURE_EMB_PSRAM, "EMB PSRAM"},
    {CHIP_FEATURE_WIFI_BGN, "2.4GHz WiFi"},
};

void DebugComponent::get_device_info_(std::string &device_info) {
#if defined(USE_ARDUINO)
  const char *flash_mode;
  switch (ESP.getFlashChipMode()) {  // NOLINT(readability-static-accessed-through-instance)
    case FM_QIO:
      flash_mode = "QIO";
      break;
    case FM_QOUT:
      flash_mode = "QOUT";
      break;
    case FM_DIO:
      flash_mode = "DIO";
      break;
    case FM_DOUT:
      flash_mode = "DOUT";
      break;
    case FM_FAST_READ:
      flash_mode = "FAST_READ";
      break;
    case FM_SLOW_READ:
      flash_mode = "SLOW_READ";
      break;
    default:
      flash_mode = "UNKNOWN";
  }
  ESP_LOGD(TAG, "Flash Chip: Size=%ukB Speed=%uMHz Mode=%s",
           ESP.getFlashChipSize() / 1024,                                                   // NOLINT
           ESP.getFlashChipSpeed() / 1000000, flash_mode);                                  // NOLINT
  device_info += "|Flash: " + to_string(ESP.getFlashChipSize() / 1024) +                    // NOLINT
                 "kB Speed:" + to_string(ESP.getFlashChipSpeed() / 1000000) + "MHz Mode:";  // NOLINT
  device_info += flash_mode;
#endif

  esp_chip_info_t info;
  esp_chip_info(&info);
  const char *model = ESPHOME_VARIANT;
  std::string features;
  for (auto feature : CHIP_FEATURES) {
    if (info.features & feature.first) {
      features += feature.second;
      features += ", ";
      info.features &= ~feature.first;
    }
  }
  if (info.features != 0)
    features += "Other:" + format_hex(info.features);
  ESP_LOGD(TAG, "Chip: Model=%s, Features=%s Cores=%u, Revision=%u", model, features.c_str(), info.cores,
           info.revision);
  device_info += "|Chip: ";
  device_info += model;
  device_info += " Features:";
  device_info += features;
  device_info += " Cores:" + to_string(info.cores);
  device_info += " Revision:" + to_string(info.revision);
  device_info += str_sprintf("|CPU Frequency: %" PRIu32 " MHz", arch_get_cpu_freq_hz() / 1000000);
  ESP_LOGD(TAG, "CPU Frequency: %" PRIu32 " MHz", arch_get_cpu_freq_hz() / 1000000);

  // Framework detection
  device_info += "|Framework: ";
#ifdef USE_ARDUINO
  ESP_LOGD(TAG, "Framework: Arduino");
  device_info += "Arduino";
#elif defined(USE_ESP_IDF)
  ESP_LOGD(TAG, "Framework: ESP-IDF");
  device_info += "ESP-IDF";
#else
  ESP_LOGW(TAG, "Framework: UNKNOWN");
  device_info += "UNKNOWN";
#endif

  ESP_LOGD(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
  device_info += "|ESP-IDF: ";
  device_info += esp_get_idf_version();

  std::string mac = get_mac_address_pretty();
  ESP_LOGD(TAG, "EFuse MAC: %s", mac.c_str());
  device_info += "|EFuse MAC: ";
  device_info += mac;

  device_info += "|Reset: ";
  device_info += get_reset_reason_();

  std::string wakeup_reason = this->get_wakeup_cause_();
  device_info += "|Wakeup: ";
  device_info += wakeup_reason;
}

void DebugComponent::update_platform_() {
#ifdef USE_SENSOR
  if (this->block_sensor_ != nullptr) {
    this->block_sensor_->publish_state(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  }
  if (this->psram_sensor_ != nullptr) {
    this->psram_sensor_->publish_state(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }
#endif
}

}  // namespace debug
}  // namespace esphome
#endif  // USE_ESP32
