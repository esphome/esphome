#include "ble_scanner.h"
#include "esphome/core/log.h"

namespace esphome::ble_scanner {

ESPHOME_LOG_TAG(TAG, "ble_scanner");

void BLEScanner::dump_config() { LOG_TEXT_SENSOR("", "BLE Scanner", this); }

}  // namespace esphome::ble_scanner
