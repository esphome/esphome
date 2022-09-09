#include "mqtt_room.h"

#ifdef USE_ESP32

namespace esphome {
namespace mqtt_room {
static const char *const TAG = "mqtt_room";

void MqttRoom::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Room:");
  ESP_LOGCONFIG(TAG, "  MQTT topic: '%s'", this->mqtt_topic_.c_str());
}

void MqttRoom::set_topic(const std::string &topic) { this->mqtt_topic_ = topic; }

void MqttRoom::send_tracker_update(const std::string &id, int rssi, int signal_power) {
  // TODO: Meadian of last 3
  float ratio = (signal_power - rssi) / (35.0f);
  float distance = pow(10, ratio);
  ESP_LOGD(TAG, "'%s': Sending state %.2f m with 2 decimals of accuracy", id.c_str(), distance);

  mqtt::global_mqtt_client->publish_json(this->mqtt_topic_, [=](ArduinoJson::JsonObject root) -> void {
    root["distance"] = distance;
    root["id"] = id;
  });
}

bool MqttRoom::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  std::string id;
  int signal_power = (!device.get_tx_powers().empty()) ? -65 + device.get_tx_powers().at(0) : -71;

  if (!device.get_name().empty()) {
    id = std::string("name:") + MqttRoom::format_device_name(device.get_name());
  }

  // TODO: Better device detection
  // for (auto &it : device.get_service_uuids()) {
  // }

  // for (auto &it : device.get_service_datas()) {
  // }

  for (auto &it : device.get_manufacturer_datas()) {
    if (it.uuid == this->apple_uuid_) {
      if (device.get_ibeacon().has_value()) {
        auto ibeacon = device.get_ibeacon().value();
        char str[56];
        sprintf(str, "ibeacon:%s-%u-%u", ibeacon.get_uuid().to_string().c_str(), ibeacon.get_major(),
                ibeacon.get_minor());
        id = std::string(str);
        signal_power = ibeacon.get_signal_power();
      } else {
        // TODO
      }
    } else if (it.uuid == this->sonos_uuid_) {
      id = std::string("sonos:") + MqttRoom::format_device_address(device.address());
    } else if (it.uuid == this->samsung_uuid_) {
      id = std::string("samsung:") + MqttRoom::format_device_address(device.address());
    } else if (it.uuid == this->google_uuid_) {
      id = std::string("google:") + MqttRoom::format_device_address(device.address());
    } else {
      ESP_LOGV(TAG, "Found device with a unknown manufacture id: '%s' and mac: '%s'", it.uuid.to_string().c_str(),
               device.address_str().c_str());
    }
  }

  if (!id.empty()) {
    ESP_LOGV(TAG, "Found device with id: '%s'", id.c_str());
    this->send_tracker_update(id, device.get_rssi(), signal_power);
  } else {
    ESP_LOGV(TAG, "Unknown BLE Device found with adress: '%s'", device.address_str().c_str());
  }

  return false;
}

std::string MqttRoom::format_device_name(const std::string &device_name) {
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

std::string MqttRoom::format_device_address(const uint8_t *device_address) {
  char mac[13];
  snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", device_address[0], device_address[1], device_address[2],
           device_address[3], device_address[4], device_address[5]);
  return mac;
}

}  // namespace mqtt_room
}  // namespace esphome

#endif
