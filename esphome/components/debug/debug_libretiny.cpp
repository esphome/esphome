#include "debug_component.h"
#ifdef USE_LIBRETINY
#include "esphome/core/log.h"

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

std::string DebugComponent::get_reset_reason_() { return lt_get_reboot_reason_name(lt_get_reboot_reason()); }

uint32_t DebugComponent::get_free_heap_() { return lt_heap_get_free(); }

void DebugComponent::get_device_info_(std::string &device_info) {
  str::string reset_reason = get_reset_reason_();
  ESP_LOGD(TAG, "LibreTiny Version: %s", lt_get_version());
  ESP_LOGD(TAG, "Chip: %s (%04x) @ %u MHz", lt_cpu_get_model_name(), lt_cpu_get_model(), lt_cpu_get_freq_mhz());
  ESP_LOGD(TAG, "Chip ID: 0x%06X", lt_cpu_get_mac_id());
  ESP_LOGD(TAG, "Board: %s", lt_get_board_code());
  ESP_LOGD(TAG, "Flash: %u KiB / RAM: %u KiB", lt_flash_get_size() / 1024, lt_ram_get_size() / 1024);
  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason.c_str());

  device_info += "|Version: ";
  device_info += LT_BANNER_STR + 10;
  device_info += "|Reset Reason: ";
  device_info += reset_reason;
  device_info += "|Chip Name: ";
  device_info += lt_cpu_get_model_name();
  device_info += "|Chip ID: 0x" + format_hex(lt_cpu_get_mac_id());
  device_info += "|Flash: " + to_string(lt_flash_get_size() / 1024) + " KiB";
  device_info += "|RAM: " + to_string(lt_ram_get_size() / 1024) + " KiB";
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
