#pragma once

#include <cinttypes>
#include <cstdio>
#include <ctime>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/text_sensor/text_sensor.h"

// No platform #ifdef: ble_device_base provides the BLE types on every platform,
// and this component is only compiled when configured — which requires a BLE
// hub — so it builds on any platform with a BLEHub tracker without a per-chip
// guard.
namespace esphome::ble_scanner {

class BLEScanner final : public text_sensor::TextSensor, public ble_device_base::ESPBTDeviceListener, public Component {
 public:
  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    // Escape special characters in the device name for valid JSON. Control characters stay in the \u00XX form this
    // sensor has always published.
    char escaped_name[128];
    json_escape_into_buffer(escaped_name, device.get_name(), /*short_control_escapes=*/false);

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"timestamp\":%" PRId64 ",\"address\":\"%s\",\"rssi\":%d,\"name\":\"%s\"}",
             static_cast<int64_t>(::time(nullptr)), device.address_str_to(addr_buf), device.get_rssi(), escaped_name);
    this->publish_state(buf);
    return true;
  }
  void dump_config() override;
};

}  // namespace esphome::ble_scanner
