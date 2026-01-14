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
    strncpy(buffer, LOG_STR_ARG(component->get_component_log_str()), REBOOT_MAX_LEN - 1);
    buffer[REBOOT_MAX_LEN - 1] = '\0';
  }
  ESP_LOGD(TAG, "Storing reboot source: %s", buffer);
  pref.save(&buffer);
  global_preferences->sync();
}

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  char *buf = buffer.data();
  const size_t size = RESET_REASON_BUFFER_SIZE;

  unsigned reason = esp_reset_reason();
  if (reason < sizeof(RESET_REASONS) / sizeof(RESET_REASONS[0])) {
    if (reason == ESP_RST_SW) {
      auto pref = global_preferences->make_preference(REBOOT_MAX_LEN, fnv1_hash(REBOOT_KEY + App.get_name()));
      char reboot_source[REBOOT_MAX_LEN]{};
      if (pref.load(&reboot_source)) {
        reboot_source[REBOOT_MAX_LEN - 1] = '\0';
        snprintf(buf, size, "Reboot request from %s", reboot_source);
      } else {
        snprintf(buf, size, "%s", RESET_REASONS[reason]);
      }
    } else {
      snprintf(buf, size, "%s", RESET_REASONS[reason]);
    }
  } else {
    snprintf(buf, size, "unknown source");
  }
  ESP_LOGD(TAG, "Reset Reason: %s", buf);
  return buf;
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

const char *DebugComponent::get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  const char *wake_reason;
  unsigned reason = esp_sleep_get_wakeup_cause();
  if (reason < sizeof(WAKEUP_CAUSES) / sizeof(WAKEUP_CAUSES[0])) {
    wake_reason = WAKEUP_CAUSES[reason];
  } else {
    wake_reason = "unknown source";
  }
  ESP_LOGD(TAG, "Wakeup Reason: %s", wake_reason);
  // Return the static string directly - no need to copy to buffer
  return wake_reason;
}

void DebugComponent::log_partition_info_() {
  ESP_LOGCONFIG(TAG,
                "Partition table:\n"
                "  %-12s %-4s %-8s %-10s %-10s",
                "Name", "Type", "Subtype", "Address", "Size");
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

struct ChipFeature {
  int bit;
  const char *name;
};

static constexpr ChipFeature CHIP_FEATURES[] = {
    {CHIP_FEATURE_BLE, "BLE"},
    {CHIP_FEATURE_BT, "BT"},
    {CHIP_FEATURE_EMB_FLASH, "EMB Flash"},
    {CHIP_FEATURE_EMB_PSRAM, "EMB PSRAM"},
    {CHIP_FEATURE_WIFI_BGN, "2.4GHz WiFi"},
};

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

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
  uint32_t flash_size = ESP.getFlashChipSize() / 1024;       // NOLINT
  uint32_t flash_speed = ESP.getFlashChipSpeed() / 1000000;  // NOLINT
  ESP_LOGD(TAG, "Flash Chip: Size=%" PRIu32 "kB Speed=%" PRIu32 "MHz Mode=%s", flash_size, flash_speed, flash_mode);
  pos = buf_append(buf, size, pos, "|Flash: %" PRIu32 "kB Speed:%" PRIu32 "MHz Mode:%s", flash_size, flash_speed,
                   flash_mode);
#endif

  esp_chip_info_t info;
  esp_chip_info(&info);
  const char *model = ESPHOME_VARIANT;

  // Build features string
  pos = buf_append(buf, size, pos, "|Chip: %s Features:", model);
  bool first_feature = true;
  for (const auto &feature : CHIP_FEATURES) {
    if (info.features & feature.bit) {
      pos = buf_append(buf, size, pos, "%s%s", first_feature ? "" : ", ", feature.name);
      first_feature = false;
      info.features &= ~feature.bit;
    }
  }
  if (info.features != 0) {
    pos = buf_append(buf, size, pos, "%sOther:0x%" PRIx32, first_feature ? "" : ", ", info.features);
  }
  ESP_LOGD(TAG, "Chip: Model=%s, Cores=%u, Revision=%u", model, info.cores, info.revision);
  pos = buf_append(buf, size, pos, " Cores:%u Revision:%u", info.cores, info.revision);

  uint32_t cpu_freq_mhz = arch_get_cpu_freq_hz() / 1000000;
  ESP_LOGD(TAG, "CPU Frequency: %" PRIu32 " MHz", cpu_freq_mhz);
  pos = buf_append(buf, size, pos, "|CPU Frequency: %" PRIu32 " MHz", cpu_freq_mhz);

  // Framework detection
#ifdef USE_ARDUINO
  ESP_LOGD(TAG, "Framework: Arduino");
  pos = buf_append(buf, size, pos, "|Framework: Arduino");
#elif defined(USE_ESP32)
  ESP_LOGD(TAG, "Framework: ESP-IDF");
  pos = buf_append(buf, size, pos, "|Framework: ESP-IDF");
#else
  ESP_LOGW(TAG, "Framework: UNKNOWN");
  pos = buf_append(buf, size, pos, "|Framework: UNKNOWN");
#endif

  ESP_LOGD(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
  pos = buf_append(buf, size, pos, "|ESP-IDF: %s", esp_get_idf_version());

  uint8_t mac[6];
  get_mac_address_raw(mac);
  ESP_LOGD(TAG, "EFuse MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  pos = buf_append(buf, size, pos, "|EFuse MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
                   mac[5]);

  char reason_buffer[RESET_REASON_BUFFER_SIZE];
  const char *reset_reason = get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE>(reason_buffer));
  pos = buf_append(buf, size, pos, "|Reset: %s", reset_reason);

  const char *wakeup_cause = get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE>(reason_buffer));
  pos = buf_append(buf, size, pos, "|Wakeup: %s", wakeup_cause);

  return pos;
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
