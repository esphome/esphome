#include "inkbird_ibsth1_mini.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace inkbird_ibsth1_mini {

static const char *TAG = "inkbird_ibsth1_mini";

void InkbirdIBSTH1_MINI::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkbird IBS TH1 MINI");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool InkbirdIBSTH1_MINI::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // The below is based on my research and reverse engineering of a single device
  // It is entirely possible that some of that may be inaccurate or incomplete

  // for Inkbird IBS-TH1 Mini device we expect
  // 1) expected mac address
  // 2) device address type == PUBLIC
  // 3) no service datas
  // 4) one manufacturer datas
  // 5) the manufacturer datas should contain a 16-bit uuid amd a 7-byte data vector
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
  if ((mnfData.data[2] != 0) || (mnfData.data[6] != 8)) {
    ESP_LOGVV(TAG, "parse_device(): unexpected data");
    return false;
  }

  // sensor output encoding
  // data[5] is a battery level
  // data[0] and data[1] is humidity * 100 (in pct)
  // uuid is a temperature * 100 (in Celcius)
  auto battery_level = mnfData.data[5];
  auto temperature = mnfData.uuid.get_uuid().uuid.uuid16 / 100.0f;
  auto humidity = ((mnfData.data[1] << 8) + mnfData.data[0]) / 100.0f;

  if (this->temperature_ != nullptr) {
    this->temperature_->publish_state(temperature);
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
