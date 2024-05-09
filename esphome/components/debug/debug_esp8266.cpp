#include "debug_component.h"
#ifdef USE_ESP8266
#include "esphome/core/log.h"
#include <Esp.h>

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

std::string DebugComponent::get_reset_reason_() {
#if !defined(CLANG_TIDY)
  return ESP.getResetReason().c_str();
#else
  return "";
#endif
}

uint32_t DebugComponent::get_free_heap_() {
  return ESP.getFreeHeap();  // NOLINT(readability-static-accessed-through-instance)
}

void DebugComponent::get_device_info_(std::string &device_info) {
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
    default:
      flash_mode = "UNKNOWN";
  }
  ESP_LOGD(TAG, "Flash Chip: Size=%ukB Speed=%uMHz Mode=%s",
           ESP.getFlashChipSize() / 1024,                                                   // NOLINT
           ESP.getFlashChipSpeed() / 1000000, flash_mode);                                  // NOLINT
  device_info += "|Flash: " + to_string(ESP.getFlashChipSize() / 1024) +                    // NOLINT
                 "kB Speed:" + to_string(ESP.getFlashChipSpeed() / 1000000) + "MHz Mode:";  // NOLINT
  device_info += flash_mode;

#if !defined(CLANG_TIDY)
  auto reset_reason = get_reset_reason_();
  ESP_LOGD(TAG, "Chip ID: 0x%08X", ESP.getChipId());
  ESP_LOGD(TAG, "SDK Version: %s", ESP.getSdkVersion());
  ESP_LOGD(TAG, "Core Version: %s", ESP.getCoreVersion().c_str());
  ESP_LOGD(TAG, "Boot Version=%u Mode=%u", ESP.getBootVersion(), ESP.getBootMode());
  ESP_LOGD(TAG, "CPU Frequency: %u", ESP.getCpuFreqMHz());
  ESP_LOGD(TAG, "Flash Chip ID=0x%08X", ESP.getFlashChipId());
  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason.c_str());
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
  device_info += reset_reason;
  device_info += "|";
  device_info += ESP.getResetInfo().c_str();
#endif
}

void DebugComponent::update_platform_() {
#ifdef USE_SENSOR
  if (this->block_sensor_ != nullptr) {
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    this->block_sensor_->publish_state(ESP.getMaxFreeBlockSize());
  }
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
  if (this->fragmentation_sensor_ != nullptr) {
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    this->fragmentation_sensor_->publish_state(ESP.getHeapFragmentation());
  }
#endif

#endif
}

}  // namespace debug
}  // namespace esphome
#endif
