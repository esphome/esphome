#include "xiaomi_sensor.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_ble {

static const char *TAG = "xiaomi_ble.sensor";

void XiaomiSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Sensor");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance", this->impedance_);
}

bool XiaomiSensor::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  auto res = xiaomi_ble::parse_xiaomi(device);
  return !;
}

}  // namespace xiaomi_ble
}  // namespace esphome

#endif
