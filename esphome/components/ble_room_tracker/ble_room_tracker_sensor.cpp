#include "ble_room_tracker_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_room_tracker {

static const char *const TAG = "ble_room_tracker";

bool BLERoomTracker::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  std::string id;

  if (!device.get_name().empty()) {
    id = std::string("name:") + this->format_device_name_(device.get_name());
  }

  for (auto &it : device.get_service_uuids()) {
    // TODO
  }

  for (auto &it : device.get_service_datas()) {
    // TODO
  }

  for (auto &it : device.get_manufacturer_datas()) {
    if (it.uuid == APPLE_UUID) {
      if (device.get_ibeacon().has_value()) {
        auto ibeacon = device.get_ibeacon().value();
        char str[56];
        sprintf(str, "ibeacon:%s-%u-%u", ibeacon.get_uuid().to_string().c_str(), ibeacon.get_major(),
                ibeacon.get_minor());
        id = std::string(str);
      } else {
        ESP_LOGD(TAG, "Found unknown apple device");
      }
    } else if (it.uuid == SONOS_UUID) {
      id = std::string("sonos:") + this->format_device_address_(device.address());
    } else if (it.uuid == SAMSUNG_UUID) {
      id = std::string("samsung:") + this->format_device_address_(device.address());
    } else if (it.uuid == GOOGLE_UUID) {
      id = std::string("google:") + this->format_device_address_(device.address());
    } else {
      ESP_LOGI(TAG, "Found device with a unknown manufacture id: '%s' and mac: '%s'", it.uuid.to_string().c_str(),
               device.address_str().c_str());
    }
  }

  if (!id.empty()) {
    ESP_LOGD(TAG, "Found device with id: '%s'", id.c_str());
  } else {
    ESP_LOGW(TAG, "Unknown BLE Device found with adress: '%s'", device.address_str().c_str());
  }

  return false;
}

void BLERoomTracker::update_tracker(int rssi, int signal_power) {
  if (this->rssi_sensor_ != nullptr)
    this->rssi_sensor_->publish_state(rssi);

  if (this->signal_power_sensor_ != nullptr && signal_power != 0)
    this->signal_power_sensor_->publish_state(signal_power);

  if (signal_power == 0)
    signal_power = -71;

  float distance = pow(10, (signal_power - rssi) / (10.0f * 3.5f));

  if (this->distance_sensor_ != nullptr)
    this->distance_sensor_->publish_state(distance);

  // this->mqtt_room_->send_tracker_update(this->id_, this->name_, distance);
}

void BLERoomTracker::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Room Tracker:");
  // ESP_LOGCONFIG(TAG, "  Tracker ID: '%s'", this->id_.c_str());
  // ESP_LOGCONFIG(TAG, "  Name: '%s'", this->name_.c_str());
  LOG_SENSOR("  ", "RSSI", this->rssi_sensor_);
  LOG_SENSOR("  ", "Signal power", this->signal_power_sensor_);
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
}

std::string BLERoomTracker::format_device_name_(const std::string &device_name) {
  char cstr[256];
  int i = 0;
  bool previous_is_space = false;
  for (const char &c : device_name) {
    if (c == ' ' && !previous_is_space) {
      cstr[i] = '-';
      i++;
      previous_is_space = true;
    } else if (c != ' ') {
      cstr[i] = toLowerCase(c);
      i++;
      previous_is_space = false;
    }
  }
  cstr[i] = 0;
  auto str = std::string(cstr);
  return str.substr(str.find_first_not_of('-'), str.find_last_not_of('-') + 1);
}

std::string BLERoomTracker::format_device_address_(const uint8_t *device_address) {
  char mac[13];
  snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", device_address[0], device_address[1], device_address[2],
           device_address[3], device_address[4], device_address[5]);
  return mac;
}

}  // namespace ble_room_tracker
}  // namespace esphome

#endif
