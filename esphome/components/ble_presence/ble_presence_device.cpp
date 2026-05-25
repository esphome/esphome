#include "ble_presence_device.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::ble_presence {

static const char *const TAG = "ble_presence";

void BLEPresenceDevice::dump_config() { LOG_BINARY_SENSOR("", "BLE Presence", this); }

}  // namespace esphome::ble_presence

#endif
