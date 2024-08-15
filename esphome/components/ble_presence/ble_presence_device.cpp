#include "ble_presence_device.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_presence {

static const char *const TAG = "ble_presence";

void BLEPresenceDevice::dump_config() { LOG_BINARY_SENSOR("", "BLE Presence", this); }

void BLEPresenceDevice::set_minimum_rssi_input(number::Number *min_rssi_number) {
    min_rssi_number->add_on_state_callback([this](float state) {
        int rssi = int(state);
        if (rssi < -100 || rssi > -30) {
            ESP_LOGW(TAG, "Valid RSSI range is -30dB to -100dB");
            return;
        }
        ESP_LOGI(TAG, "Setting minimum rssi to %d", rssi);
        this->set_minimum_rssi_(int(state));
    });

    this->check_minimum_rssi_ = true;
}


}  // namespace ble_presence
}  // namespace esphome

#endif
