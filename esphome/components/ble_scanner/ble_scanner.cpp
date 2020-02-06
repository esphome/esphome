#include "ble_scanner.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_scanner {

static const char *TAG = "ble_scanner";

void BLEScanner::dump_config() { LOG_TEXT_SENSOR("", "BLE Scanner", this); }

}  // namespace ble_scanner
}  // namespace esphome

#endif
