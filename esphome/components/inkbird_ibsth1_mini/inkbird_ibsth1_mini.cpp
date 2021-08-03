#include "inkbird_ibsth1_mini.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace inkbird_ibsth1_mini {

static const char *const TAG = "inkbird_ibsth1_mini";

void InkbirdIBSTH1_MINI::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkbird IBS TH1 MINI");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "External Temperature", this->external_temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool InkbirdIBSTH1_MINI::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // The below is based on my research and reverse engineering of a single device
  // It is entirely possible that some of that may be inaccurate or incomplete

  // for Inkbird IBS-TH1 Mini device we expect
  // 1) expected mac address
  // 2) device address type == PUBLIC
  // 3) no service data
  // 4) one manufacturer data
  // 5) the manufacturer data should contain a 16-bit uuid amd a 7-byte data vector
  // 6) the 7-byte data component should have data[2] == 0 and data[6] == 8

  // the address should match the address we declared
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  if (device.get_address_type() != BLE_ADDR_TYPE_PUBLIC) {
    ESP_LOGVV(TAG, "parse_device(): address is not public");
    return false;
  }
  if (device.get_service_datas().size() != 0) {
    ESP_LOGVV(TAG, "parse_device(): service_data is expected to be empty");
    return false;
  }
  auto mnfDatas = device.get_manufacturer_datas();
  if (mnfDatas.size() != 1) {
    ESP_LOGVV(TAG, "parse_device(): manufacturer_datas is expected to have a single element");
    return false;
  }
  auto mnfData = mnfDatas[0];
  if (mnfData.uuid.get_uuid().len != ESP_UUID_LEN_16) {
    ESP_LOGVV(TAG, "parse_device(): manufacturer data element is expected to have uuid of length 16");
    return false;
  }
  if (mnfData.data.size() != 7) {
    ESP_LOGVV(TAG, "parse_device(): manufacturer data element length is expected to be of length 7");
    return false;
  }
  if (mnfData.data[6] != 8) {
    ESP_LOGVV(TAG, "parse_device(): unexpected data");
    return false;
  }

  // sensor output encoding
  // data[5] is a battery level
  // data[0] and data[1] is humidity * 100 (in pct)
  // uuid is a temperature * 100 (in Celsius)
  // when data[2] == 0 temperature is from internal sensor (IBS-TH1 or IBS-TH1 Mini)
  // when data[2] == 1 temperature is from external sensor (IBS-TH1 only)

  // Create empty variables to pass automatic checks
  auto temperature = NAN;
  auto external_temperature = NAN;

  // Read bluetooth data into variable
  auto measured_temperature = mnfData.uuid.get_uuid().uuid.uuid16 / 100.0f;

  // Set temperature or external_temperature based on which sensor is in use
  if (mnfData.data[2] == 0) {
    temperature = measured_temperature;
  } else if (mnfData.data[2] == 1) {
    external_temperature = measured_temperature;
  } else {
    ESP_LOGVV(TAG, "parse_device(): unknown sensor type");
    return false;
  }

  auto battery_level = mnfData.data[5];
  auto humidity = ((mnfData.data[1] << 8) + mnfData.data[0]) / 100.0f;

  // Send temperature only if the value is set
  if (!isnan(temperature) && this->temperature_ != nullptr) {
    this->temperature_->publish_state(temperature);
  }
  if (!isnan(external_temperature) && this->external_temperature_ != nullptr) {
    this->external_temperature_->publish_state(external_temperature);
  }
  if (this->humidity_ != nullptr) {
    this->humidity_->publish_state(humidity);
  }
  if (this->battery_level_ != nullptr) {
    this->battery_level_->publish_state(battery_level);
  }

  return true;
}

}  // namespace inkbird_ibsth1_mini
}  // namespace esphome

#endif
