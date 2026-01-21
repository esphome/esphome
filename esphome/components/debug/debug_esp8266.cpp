#include "debug_component.h"
#ifdef USE_ESP8266
#include "esphome/core/log.h"
#include <Esp.h>

extern "C" {
#include <user_interface.h>

// Global reset info struct populated by SDK at boot
extern struct rst_info resetInfo;

// Core version - either a string pointer or a version number to format as hex
extern uint32_t core_version;
extern const char *core_release;
}

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

// Get reset reason string from reason code (no heap allocation)
// Returns LogString* pointing to flash (PROGMEM) on ESP8266
static const LogString *get_reset_reason_str(uint32_t reason) {
  switch (reason) {
    case REASON_DEFAULT_RST:
      return LOG_STR("Power On");
    case REASON_WDT_RST:
      return LOG_STR("Hardware Watchdog");
    case REASON_EXCEPTION_RST:
      return LOG_STR("Exception");
    case REASON_SOFT_WDT_RST:
      return LOG_STR("Software Watchdog");
    case REASON_SOFT_RESTART:
      return LOG_STR("Software/System restart");
    case REASON_DEEP_SLEEP_AWAKE:
      return LOG_STR("Deep-Sleep Wake");
    case REASON_EXT_SYS_RST:
      return LOG_STR("External System");
    default:
      return LOG_STR("Unknown");
  }
}

// Size for core version hex buffer
static constexpr size_t CORE_VERSION_BUFFER_SIZE = 12;

// Get core version string (no heap allocation)
// Returns either core_release directly or formats core_version as hex into provided buffer
static const char *get_core_version_str(std::span<char, CORE_VERSION_BUFFER_SIZE> buffer) {
  if (core_release != nullptr) {
    return core_release;
  }
  snprintf_P(buffer.data(), CORE_VERSION_BUFFER_SIZE, PSTR("%08x"), core_version);
  return buffer.data();
}

// Size for reset info buffer
static constexpr size_t RESET_INFO_BUFFER_SIZE = 200;

// Get detailed reset info string (no heap allocation)
// For watchdog/exception resets, includes detailed exception info
static const char *get_reset_info_str(std::span<char, RESET_INFO_BUFFER_SIZE> buffer, uint32_t reason) {
  if (reason >= REASON_WDT_RST && reason <= REASON_SOFT_WDT_RST) {
    snprintf_P(buffer.data(), RESET_INFO_BUFFER_SIZE,
               PSTR("Fatal exception:%d flag:%d (%s) epc1:0x%08x epc2:0x%08x epc3:0x%08x excvaddr:0x%08x depc:0x%08x"),
               static_cast<int>(resetInfo.exccause), static_cast<int>(reason),
               LOG_STR_ARG(get_reset_reason_str(reason)), resetInfo.epc1, resetInfo.epc2, resetInfo.epc3,
               resetInfo.excvaddr, resetInfo.depc);
    return buffer.data();
  }
  return LOG_STR_ARG(get_reset_reason_str(reason));
}

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  // Copy from flash to provided buffer
  strncpy_P(buffer.data(), (PGM_P) get_reset_reason_str(resetInfo.reason), RESET_REASON_BUFFER_SIZE - 1);
  buffer[RESET_REASON_BUFFER_SIZE - 1] = '\0';
  return buffer.data();
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

  const LogString *flash_mode;
  switch (ESP.getFlashChipMode()) {  // NOLINT(readability-static-accessed-through-instance)
    case FM_QIO:
      flash_mode = LOG_STR("QIO");
      break;
    case FM_QOUT:
      flash_mode = LOG_STR("QOUT");
      break;
    case FM_DIO:
      flash_mode = LOG_STR("DIO");
      break;
    case FM_DOUT:
      flash_mode = LOG_STR("DOUT");
      break;
    default:
      flash_mode = LOG_STR("UNKNOWN");
  }
  uint32_t flash_size = ESP.getFlashChipSize() / 1024;       // NOLINT(readability-static-accessed-through-instance)
  uint32_t flash_speed = ESP.getFlashChipSpeed() / 1000000;  // NOLINT(readability-static-accessed-through-instance)
  ESP_LOGD(TAG, "Flash Chip: Size=%" PRIu32 "kB Speed=%" PRIu32 "MHz Mode=%s", flash_size, flash_speed,
           LOG_STR_ARG(flash_mode));
  pos = buf_append_printf(buf, size, pos, "|Flash: %" PRIu32 "kB Speed:%" PRIu32 "MHz Mode:%s", flash_size, flash_speed,
                          LOG_STR_ARG(flash_mode));

  char reason_buffer[RESET_REASON_BUFFER_SIZE];
  const char *reset_reason = get_reset_reason_(reason_buffer);
  char core_version_buffer[CORE_VERSION_BUFFER_SIZE];
  char reset_info_buffer[RESET_INFO_BUFFER_SIZE];
  // NOLINTBEGIN(readability-static-accessed-through-instance)
  uint32_t chip_id = ESP.getChipId();
  uint8_t boot_version = ESP.getBootVersion();
  uint8_t boot_mode = ESP.getBootMode();
  uint8_t cpu_freq = ESP.getCpuFreqMHz();
  uint32_t flash_chip_id = ESP.getFlashChipId();
  const char *sdk_version = ESP.getSdkVersion();
  // NOLINTEND(readability-static-accessed-through-instance)

  ESP_LOGD(TAG,
           "Chip ID: 0x%08" PRIX32 "\n"
           "SDK Version: %s\n"
           "Core Version: %s\n"
           "Boot Version=%u Mode=%u\n"
           "CPU Frequency: %u\n"
           "Flash Chip ID=0x%08" PRIX32 "\n"
           "Reset Reason: %s\n"
           "Reset Info: %s",
           chip_id, sdk_version, get_core_version_str(core_version_buffer), boot_version, boot_mode, cpu_freq,
           flash_chip_id, reset_reason, get_reset_info_str(reset_info_buffer, resetInfo.reason));

  pos = buf_append_printf(buf, size, pos, "|Chip: 0x%08" PRIX32, chip_id);
  pos = buf_append_printf(buf, size, pos, "|SDK: %s", sdk_version);
  pos = buf_append_printf(buf, size, pos, "|Core: %s", get_core_version_str(core_version_buffer));
  pos = buf_append_printf(buf, size, pos, "|Boot: %u", boot_version);
  pos = buf_append_printf(buf, size, pos, "|Mode: %u", boot_mode);
  pos = buf_append_printf(buf, size, pos, "|CPU: %u", cpu_freq);
  pos = buf_append_printf(buf, size, pos, "|Flash: 0x%08" PRIX32, flash_chip_id);
  pos = buf_append_printf(buf, size, pos, "|Reset: %s", reset_reason);
  pos = buf_append_printf(buf, size, pos, "|%s", get_reset_info_str(reset_info_buffer, resetInfo.reason));

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
