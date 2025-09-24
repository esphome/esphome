#include "ble_presence_device.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_presence {

static const char *const TAG = "ble_presence";

void BLEPresenceDevice::dump_config() { LOG_BINARY_SENSOR("", "BLE Presence", this); }

}  // namespace ble_presence
}  // namespace esphome

#endif
