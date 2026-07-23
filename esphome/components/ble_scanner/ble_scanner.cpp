#include "ble_scanner.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::ble_scanner {

static const char *const TAG = "ble_scanner";

void BLEScanner::dump_config() { LOG_TEXT_SENSOR("", "BLE Scanner", this); }

}  // namespace esphome::ble_scanner

#endif
