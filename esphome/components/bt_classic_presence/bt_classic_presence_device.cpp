#include "bt_classic_presence_device.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bt_classic_presence {

const char *const TAG = "bt_classic_presence";

void BTClassicPresenceDevice::dump_config() { LOG_BINARY_SENSOR("", "BT Classic Presence", this); }

void BTClassicPresenceDevice::update() {
  scans_remaining = num_scans;
  parent()->addScan(esp32_bt_classic::bt_scan_item(u64_addr, num_scans));
}

void BTClassicPresenceDevice::on_scan_result(const esp32_bt_classic::rmt_name_result &result) {
  const uint64_t result_addr = esp32_bt_classic::bd_addr_to_uint64(result.bda);
  if (result_addr == u64_addr) {
    if (ESP_BT_STATUS_SUCCESS == result.stat) {
      this->publish_state(true);
    } else {
      if (scans_remaining > 0) {
        scans_remaining--;
      } else {
        this->publish_state(false);
      }
    }
  }
}

}  // namespace bt_classic_presence
}  // namespace esphome

#endif
