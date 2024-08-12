#include "debug_component.h"
#ifdef USE_ESP32
#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_chip_info.h>

#if defined(USE_ESP32_VARIANT_ESP32)
#include <esp32/rom/rtc.h>
#elif defined(USE_ESP32_VARIANT_ESP32C3)
#include <esp32c3/rom/rtc.h>
#elif defined(USE_ESP32_VARIANT_ESP32C6)
#include <esp32c6/rom/rtc.h>
#elif defined(USE_ESP32_VARIANT_ESP32S2)
#include <esp32s2/rom/rtc.h>
#elif defined(USE_ESP32_VARIANT_ESP32S3)
#include <esp32s3/rom/rtc.h>
#endif
#ifdef USE_ARDUINO
#include <Esp.h>
#endif

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

std::string DebugComponent::get_reset_reason_() {
  std::string reset_reason;
  switch (rtc_get_reset_reason(0)) {
    case POWERON_RESET:
      reset_reason = "Power On Reset";
      break;
#if defined(USE_ESP32_VARIANT_ESP32)
    case SW_RESET:
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    case RTC_SW_SYS_RESET:
#endif
      reset_reason = "Software Reset Digital Core";
      break;
#if defined(USE_ESP32_VARIANT_ESP32)
    case OWDT_RESET:
      reset_reason = "Watch Dog Reset Digital Core";
      break;
#endif
    case DEEPSLEEP_RESET:
      reset_reason = "Deep Sleep Reset Digital Core";
      break;
#if defined(USE_ESP32_VARIANT_ESP32)
    case SDIO_RESET:
      reset_reason = "SLC Module Reset Digital Core";
      break;
#endif
    case TG0WDT_SYS_RESET:
      reset_reason = "Timer Group 0 Watch Dog Reset Digital Core";
      break;
    case TG1WDT_SYS_RESET:
      reset_reason = "Timer Group 1 Watch Dog Reset Digital Core";
      break;
    case RTCWDT_SYS_RESET:
      reset_reason = "RTC Watch Dog Reset Digital Core";
      break;
#if !defined(USE_ESP32_VARIANT_ESP32C6)
    case INTRUSION_RESET:
      reset_reason = "Intrusion Reset CPU";
      break;
#endif
#if defined(USE_ESP32_VARIANT_ESP32)
    case TGWDT_CPU_RESET:
      reset_reason = "Timer Group Reset CPU";
      break;
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    case TG0WDT_CPU_RESET:
      reset_reason = "Timer Group 0 Reset CPU";
      break;
#endif
#if defined(USE_ESP32_VARIANT_ESP32)
    case SW_CPU_RESET:
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    case RTC_SW_CPU_RESET:
#endif
      reset_reason = "Software Reset CPU";
      break;
    case RTCWDT_CPU_RESET:
      reset_reason = "RTC Watch Dog Reset CPU";
      break;
#if defined(USE_ESP32_VARIANT_ESP32)
    case EXT_CPU_RESET:
      reset_reason = "External CPU Reset";
      break;
#endif
    case RTCWDT_BROWN_OUT_RESET:
      reset_reason = "Voltage Unstable Reset";
      break;
    case RTCWDT_RTC_RESET:
      reset_reason = "RTC Watch Dog Reset Digital Core And RTC Module";
      break;
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    case TG1WDT_CPU_RESET:
      reset_reason = "Timer Group 1 Reset CPU";
      break;
    case SUPER_WDT_RESET:
      reset_reason = "Super Watchdog Reset Digital Core And RTC Module";
      break;
    case GLITCH_RTC_RESET:
      reset_reason = "Glitch Reset Digital Core And RTC Module";
      break;
    case EFUSE_RESET:
      reset_reason = "eFuse Reset Digital Core";
      break;
#endif
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S3)
    case USB_UART_CHIP_RESET:
      reset_reason = "USB UART Reset Digital Core";
      break;
    case USB_JTAG_CHIP_RESET:
      reset_reason = "USB JTAG Reset Digital Core";
      break;
    case POWER_GLITCH_RESET:
      reset_reason = "Power Glitch Reset Digital Core And RTC Module";
      break;
#endif
    default:
      reset_reason = "Unknown Reset Reason";
  }
  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason.c_str());
  return reset_reason;
}

uint32_t DebugComponent::get_free_heap_() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

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
  const char *model;
#if defined(USE_ESP32_VARIANT_ESP32)
  model = "ESP32";
#elif defined(USE_ESP32_VARIANT_ESP32C3)
  model = "ESP32-C3";
#elif defined(USE_ESP32_VARIANT_ESP32C6)
  model = "ESP32-C6";
#elif defined(USE_ESP32_VARIANT_ESP32S2)
  model = "ESP32-S2";
#elif defined(USE_ESP32_VARIANT_ESP32S3)
  model = "ESP32-S3";
#elif defined(USE_ESP32_VARIANT_ESP32H2)
  model = "ESP32-H2";
#else
  model = "UNKNOWN";
#endif
  std::string features;
  if (info.features & CHIP_FEATURE_EMB_FLASH) {
    features += "EMB_FLASH,";
    info.features &= ~CHIP_FEATURE_EMB_FLASH;
  }
  if (info.features & CHIP_FEATURE_WIFI_BGN) {
    features += "WIFI_BGN,";
    info.features &= ~CHIP_FEATURE_WIFI_BGN;
  }
  if (info.features & CHIP_FEATURE_BLE) {
    features += "BLE,";
    info.features &= ~CHIP_FEATURE_BLE;
  }
  if (info.features & CHIP_FEATURE_BT) {
    features += "BT,";
    info.features &= ~CHIP_FEATURE_BT;
  }
  if (info.features & CHIP_FEATURE_EMB_PSRAM) {
    features += "EMB_PSRAM,";
    info.features &= ~CHIP_FEATURE_EMB_PSRAM;
  }
  if (info.features)
    features += "Other:" + format_hex(info.features);
  ESP_LOGD(TAG, "Chip: Model=%s, Features=%s Cores=%u, Revision=%u", model, features.c_str(), info.cores,
           info.revision);
  device_info += "|Chip: ";
  device_info += model;
  device_info += " Features:";
  device_info += features;
  device_info += " Cores:" + to_string(info.cores);
  device_info += " Revision:" + to_string(info.revision);

  ESP_LOGD(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
  device_info += "|ESP-IDF: ";
  device_info += esp_get_idf_version();

  std::string mac = get_mac_address_pretty();
  ESP_LOGD(TAG, "EFuse MAC: %s", mac.c_str());
  device_info += "|EFuse MAC: ";
  device_info += mac;

  device_info += "|Reset: ";
  device_info += get_reset_reason_();

  const char *wakeup_reason;
  switch (rtc_get_wakeup_cause()) {
    case NO_SLEEP:
      wakeup_reason = "No Sleep";
      break;
    case EXT_EVENT0_TRIG:
      wakeup_reason = "External Event 0";
      break;
    case EXT_EVENT1_TRIG:
      wakeup_reason = "External Event 1";
      break;
    case GPIO_TRIG:
      wakeup_reason = "GPIO";
      break;
    case TIMER_EXPIRE:
      wakeup_reason = "Wakeup Timer";
      break;
    case SDIO_TRIG:
      wakeup_reason = "SDIO";
      break;
    case MAC_TRIG:
      wakeup_reason = "MAC";
      break;
    case UART0_TRIG:
      wakeup_reason = "UART0";
      break;
    case UART1_TRIG:
      wakeup_reason = "UART1";
      break;
    case TOUCH_TRIG:
      wakeup_reason = "Touch";
      break;
    case SAR_TRIG:
      wakeup_reason = "SAR";
      break;
    case BT_TRIG:
      wakeup_reason = "BT";
      break;
    default:
      wakeup_reason = "Unknown";
  }
  ESP_LOGD(TAG, "Wakeup Reason: %s", wakeup_reason);
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
#endif
