#pragma once

#include <cinttypes>
#include <cstdio>
#include <ctime>

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32

namespace esphome::ble_scanner {

class BLEScanner : public text_sensor::TextSensor, public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    // Escape special characters in the device name for valid JSON
    const char *name = device.get_name().c_str();
    char escaped_name[128];
    size_t pos = 0;
    for (; *name != '\0' && pos < sizeof(escaped_name) - 7; name++) {
      uint8_t c = static_cast<uint8_t>(*name);
      if (c == '"' || c == '\\') {
        escaped_name[pos++] = '\\';
        escaped_name[pos++] = c;
      } else if (c < 0x20) {
        pos += snprintf(escaped_name + pos, sizeof(escaped_name) - pos, "\\u%04x", c);
      } else {
        escaped_name[pos++] = c;
      }
    }
    escaped_name[pos] = '\0';

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"timestamp\":%" PRId64 ",\"address\":\"%s\",\"rssi\":%d,\"name\":\"%s\"}",
             static_cast<int64_t>(::time(nullptr)), device.address_str_to(addr_buf), device.get_rssi(), escaped_name);
    this->publish_state(buf);
    return true;
  }
  void dump_config() override;
};

}  // namespace esphome::ble_scanner

#endif
