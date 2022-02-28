#include "debug_component.h"

#include <algorithm>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/version.h"

#ifdef USE_ESP32

#include <esp_heap_caps.h>
#include <esp_system.h>

#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp32/rom/rtc.h>
#else
#include <rom/rtc.h>
#endif

#endif  // USE_ESP32

#ifdef USE_ARDUINO
#include <Esp.h>
#endif

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

static uint32_t get_free_heap() {
#if defined(USE_ESP8266)
  return ESP.getFreeHeap();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP32)
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#endif
}

void DebugComponent::dump_config() {
  std::string device_info;
  device_info.reserve(256);

#ifndef ESPHOME_LOG_HAS_DEBUG
  ESP_LOGE(TAG, "Debug Component requires debug log level!");
  this->status_set_error();
  return;
#endif

  ESP_LOGCONFIG(TAG, "Debug component:");
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Device info", this->device_info_);
#endif  // USE_TEXT_SENSOR
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Free space on heap", this->free_sensor_);
  LOG_SENSOR("  ", "Largest free heap block", this->block_sensor_);
#if defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
  LOG_SENSOR("  ", "Heap fragmentation", this->fragmentation_sensor_);
#endif  // defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
#endif  // USE_SENSOR

  ESP_LOGD(TAG, "ESPHome version %s", ESPHOME_VERSION);
  device_info += ESPHOME_VERSION;

  this->free_heap_ = get_free_heap();
  ESP_LOGD(TAG, "Free Heap Size: %u bytes", this->free_heap_);

#ifdef USE_ARDUINO
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
#ifdef USE_ESP32
    case FM_FAST_READ:
      flash_mode = "FAST_READ";
      break;
    case FM_SLOW_READ:
      flash_mode = "SLOW_READ";
      break;
#endif
    default:
      flash_mode = "UNKNOWN";
  }
  ESP_LOGD(TAG, "Flash Chip: Size=%ukB Speed=%uMHz Mode=%s",
           ESP.getFlashChipSize() / 1024,                                                   // NOLINT
           ESP.getFlashChipSpeed() / 1000000, flash_mode);                                  // NOLINT
  device_info += "|Flash: " + to_string(ESP.getFlashChipSize() / 1024) +                    // NOLINT
                 "kB Speed:" + to_string(ESP.getFlashChipSpeed() / 1000000) + "MHz Mode:";  // NOLINT
  device_info += flash_mode;
#endif  // USE_ARDUINO

#ifdef USE_ESP32
  esp_chip_info_t info;
  esp_chip_info(&info);
  const char *model;
  switch (info.model) {
    case CHIP_ESP32:
      model = "ESP32";
      break;
    default:
      model = "UNKNOWN";
  }
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

  const char *reset_reason;
  switch (rtc_get_reset_reason(0)) {
    case POWERON_RESET:
      reset_reason = "Power On Reset";
      break;
    case SW_RESET:
      reset_reason = "Software Reset Digital Core";
      break;
    case OWDT_RESET:
      reset_reason = "Watch Dog Reset Digital Core";
      break;
    case DEEPSLEEP_RESET:
      reset_reason = "Deep Sleep Reset Digital Core";
      break;
    case SDIO_RESET:
      reset_reason = "SLC Module Reset Digital Core";
      break;
    case TG0WDT_SYS_RESET:
      reset_reason = "Timer Group 0 Watch Dog Reset Digital Core";
      break;
    case TG1WDT_SYS_RESET:
      reset_reason = "Timer Group 1 Watch Dog Reset Digital Core";
      break;
    case RTCWDT_SYS_RESET:
      reset_reason = "RTC Watch Dog Reset Digital Core";
      break;
    case INTRUSION_RESET:
      reset_reason = "Intrusion Reset CPU";
      break;
    case TGWDT_CPU_RESET:
      reset_reason = "Timer Group Reset CPU";
      break;
    case SW_CPU_RESET:
      reset_reason = "Software Reset CPU";
      break;
    case RTCWDT_CPU_RESET:
      reset_reason = "RTC Watch Dog Reset CPU";
      break;
    case EXT_CPU_RESET:
      reset_reason = "External CPU Reset";
      break;
    case RTCWDT_BROWN_OUT_RESET:
      reset_reason = "Voltage Unstable Reset";
      break;
    case RTCWDT_RTC_RESET:
      reset_reason = "RTC Watch Dog Reset Digital Core And RTC Module";
      break;
    default:
      reset_reason = "Unknown Reset Reason";
  }
  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason);
  device_info += "|Reset: ";
  device_info += reset_reason;

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
#endif

#if defined(USE_ESP8266) && !defined(CLANG_TIDY)
  ESP_LOGD(TAG, "Chip ID: 0x%08X", ESP.getChipId());
  ESP_LOGD(TAG, "SDK Version: %s", ESP.getSdkVersion());
  ESP_LOGD(TAG, "Core Version: %s", ESP.getCoreVersion().c_str());
  ESP_LOGD(TAG, "Boot Version=%u Mode=%u", ESP.getBootVersion(), ESP.getBootMode());
  ESP_LOGD(TAG, "CPU Frequency: %u", ESP.getCpuFreqMHz());
  ESP_LOGD(TAG, "Flash Chip ID=0x%08X", ESP.getFlashChipId());
  ESP_LOGD(TAG, "Reset Reason: %s", ESP.getResetReason().c_str());
  ESP_LOGD(TAG, "Reset Info: %s", ESP.getResetInfo().c_str());

  device_info += "|Chip: 0x" + format_hex(ESP.getChipId());
  device_info += "|SDK: ";
  device_info += ESP.getSdkVersion();
  device_info += "|Core: ";
  device_info += ESP.getCoreVersion().c_str();
  device_info += "|Boot: ";
  device_info += to_string(ESP.getBootVersion());
  device_info += "|Mode: " + to_string(ESP.getBootMode());
  device_info += "|CPU: " + to_string(ESP.getCpuFreqMHz());
  device_info += "|Flash: 0x" + format_hex(ESP.getFlashChipId());
  device_info += "|Reset: ";
  device_info += ESP.getResetReason().c_str();
  device_info += "|";
  device_info += ESP.getResetInfo().c_str();
#endif

#ifdef USE_TEXT_SENSOR
  if (this->device_info_ != nullptr) {
    if (device_info.length() > 255)
      device_info.resize(255);
    this->device_info_->publish_state(device_info);
  }
#endif  // USE_TEXT_SENSOR
}

void DebugComponent::loop() {
  // log when free heap space has halved
  uint32_t new_free_heap = get_free_heap();
  if (new_free_heap < this->free_heap_ / 2) {
    this->free_heap_ = new_free_heap;
    ESP_LOGD(TAG, "Free Heap Size: %u bytes", this->free_heap_);
    this->status_momentary_warning("heap", 1000);
  }

#ifdef USE_SENSOR
  // calculate loop time - from last call to this one
  if (this->loop_time_sensor_ != nullptr) {
    uint32_t now = millis();
    uint32_t loop_time = now - this->last_loop_timetag_;
    this->max_loop_time_ = std::max(this->max_loop_time_, loop_time);
    this->last_loop_timetag_ = now;
  }
#endif  // USE_SENSOR
}

void DebugComponent::update() {
#ifdef USE_SENSOR
  if (this->free_sensor_ != nullptr) {
    this->free_sensor_->publish_state(get_free_heap());
  }

  if (this->block_sensor_ != nullptr) {
#if defined(USE_ESP8266)
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    this->block_sensor_->publish_state(ESP.getMaxFreeBlockSize());
#elif defined(USE_ESP32)
    this->block_sensor_->publish_state(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
#endif
  }

#if defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
  if (this->fragmentation_sensor_ != nullptr) {
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    this->fragmentation_sensor_->publish_state(ESP.getHeapFragmentation());
  }
#endif

  if (this->loop_time_sensor_ != nullptr) {
    this->loop_time_sensor_->publish_state(this->max_loop_time_);
    this->max_loop_time_ = 0;
  }
#endif  // USE_SENSOR
}

float DebugComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace debug
}  // namespace esphome
