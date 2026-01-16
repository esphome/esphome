#include "debug_component.h"
#ifdef USE_ESP8266
#include "esphome/core/log.h"
#include <Esp.h>

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  char *buf = buffer.data();
#if !defined(CLANG_TIDY)
  String reason = ESP.getResetReason();  // NOLINT
  snprintf_P(buf, RESET_REASON_BUFFER_SIZE, PSTR("%s"), reason.c_str());
  return buf;
#else
  buf[0] = '\0';
  return buf;
#endif
}

const char *DebugComponent::get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  // ESP8266 doesn't have detailed wakeup cause like ESP32
  return "";
}

uint32_t DebugComponent::get_free_heap_() {
  return ESP.getFreeHeap();  // NOLINT(readability-static-accessed-through-instance)
}

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

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
  uint32_t flash_size = ESP.getFlashChipSize() / 1024;       // NOLINT
  uint32_t flash_speed = ESP.getFlashChipSpeed() / 1000000;  // NOLINT
  ESP_LOGD(TAG, "Flash Chip: Size=%" PRIu32 "kB Speed=%" PRIu32 "MHz Mode=%s", flash_size, flash_speed, flash_mode);
  pos = buf_append(buf, size, pos, "|Flash: %" PRIu32 "kB Speed:%" PRIu32 "MHz Mode:%s", flash_size, flash_speed,
                   flash_mode);

#if !defined(CLANG_TIDY)
  char reason_buffer[RESET_REASON_BUFFER_SIZE];
  const char *reset_reason = get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE>(reason_buffer));
  uint32_t chip_id = ESP.getChipId();
  uint8_t boot_version = ESP.getBootVersion();
  uint8_t boot_mode = ESP.getBootMode();
  uint8_t cpu_freq = ESP.getCpuFreqMHz();
  uint32_t flash_chip_id = ESP.getFlashChipId();

  ESP_LOGD(TAG,
           "Chip ID: 0x%08" PRIX32 "\n"
           "SDK Version: %s\n"
           "Core Version: %s\n"
           "Boot Version=%u Mode=%u\n"
           "CPU Frequency: %u\n"
           "Flash Chip ID=0x%08" PRIX32 "\n"
           "Reset Reason: %s\n"
           "Reset Info: %s",
           chip_id, ESP.getSdkVersion(), ESP.getCoreVersion().c_str(), boot_version, boot_mode, cpu_freq, flash_chip_id,
           reset_reason, ESP.getResetInfo().c_str());

  pos = buf_append(buf, size, pos, "|Chip: 0x%08" PRIX32, chip_id);
  pos = buf_append(buf, size, pos, "|SDK: %s", ESP.getSdkVersion());
  pos = buf_append(buf, size, pos, "|Core: %s", ESP.getCoreVersion().c_str());
  pos = buf_append(buf, size, pos, "|Boot: %u", boot_version);
  pos = buf_append(buf, size, pos, "|Mode: %u", boot_mode);
  pos = buf_append(buf, size, pos, "|CPU: %u", cpu_freq);
  pos = buf_append(buf, size, pos, "|Flash: 0x%08" PRIX32, flash_chip_id);
  pos = buf_append(buf, size, pos, "|Reset: %s", reset_reason);
  pos = buf_append(buf, size, pos, "|%s", ESP.getResetInfo().c_str());
#endif

  return pos;
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
