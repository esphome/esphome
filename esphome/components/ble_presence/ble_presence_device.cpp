#include "ble_presence_device.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_presence {

static const char *TAG = "something.something";

void BLEPresenceDevice::dump_config() {
  LOG_BINARY_SENSOR("", "BLE Presence", this);
}

}  // namespace ble_presence
}  // namespace esphome
