#include "debug_component.h"
#ifdef USE_LIBRETINY
#include "esphome/core/log.h"

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  // Return the static string directly
  return lt_get_reboot_reason_name(lt_get_reboot_reason());
}

const char *DebugComponent::get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) { return ""; }

uint32_t DebugComponent::get_free_heap_() { return lt_heap_get_free(); }

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

  char reason_buffer[RESET_REASON_BUFFER_SIZE];
  const char *reset_reason = get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE>(reason_buffer));
  uint32_t flash_kib = lt_flash_get_size() / 1024;
  uint32_t ram_kib = lt_ram_get_size() / 1024;
  uint32_t mac_id = lt_cpu_get_mac_id();

  ESP_LOGD(TAG,
           "LibreTiny Version: %s\n"
           "Chip: %s (%04x) @ %u MHz\n"
           "Chip ID: 0x%06" PRIX32 "\n"
           "Board: %s\n"
           "Flash: %" PRIu32 " KiB / RAM: %" PRIu32 " KiB\n"
           "Reset Reason: %s",
           lt_get_version(), lt_cpu_get_model_name(), lt_cpu_get_model(), lt_cpu_get_freq_mhz(), mac_id,
           lt_get_board_code(), flash_kib, ram_kib, reset_reason);

  pos = buf_append(buf, size, pos, "|Version: %s", LT_BANNER_STR + 10);
  pos = buf_append(buf, size, pos, "|Reset Reason: %s", reset_reason);
  pos = buf_append(buf, size, pos, "|Chip Name: %s", lt_cpu_get_model_name());
  pos = buf_append(buf, size, pos, "|Chip ID: 0x%06" PRIX32, mac_id);
  pos = buf_append(buf, size, pos, "|Flash: %" PRIu32 " KiB", flash_kib);
  pos = buf_append(buf, size, pos, "|RAM: %" PRIu32 " KiB", ram_kib);

  return pos;
}

void DebugComponent::update_platform_() {
#ifdef USE_SENSOR
  if (this->block_sensor_ != nullptr) {
    this->block_sensor_->publish_state(lt_heap_get_max_alloc());
  }
#endif
}

}  // namespace debug
}  // namespace esphome
#endif
