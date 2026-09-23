#include "ble_rssi_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::ble_rssi {

static const char *const TAG = "ble_rssi";

void BLERSSISensor::dump_config() { LOG_SENSOR("", "BLE RSSI Sensor", this); }

}  // namespace esphome::ble_rssi

#endif
