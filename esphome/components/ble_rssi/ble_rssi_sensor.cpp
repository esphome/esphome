#include "ble_rssi_sensor.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_rssi {

static const char *TAG = "ble_rssi";

void BLERSSISensor::dump_config() { LOG_SENSOR("", "BLE RSSI Sensor", this); }

}  // namespace ble_rssi
}  // namespace esphome

#endif
