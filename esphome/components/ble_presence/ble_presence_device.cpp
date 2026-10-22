#include "ble_presence_device.h"
#include "esphome/core/log.h"

namespace esphome::ble_presence {

ESPHOME_LOG_TAG(TAG, "ble_presence");

void BLEPresenceDevice::dump_config() { LOG_BINARY_SENSOR("", "BLE Presence", this); }

}  // namespace esphome::ble_presence
