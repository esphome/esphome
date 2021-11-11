#include "nspanel.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/util.h"

namespace esphome {
namespace nspanel {

static const char *const TAG = "nspanel";

static const char *const SUCCESS_RESPONSE = "{\"error\":0}";

void NSPanel::initialize() {
  // this->send_json_command_(
  //     0x84, "{\"HMI_ATCDevice\":{\"ctype\":\"device\",\"id\":\"thermostat\",\"outlet\":0,\"etype\":\"hot\"}");
  // this->send_json_command_(0x86, "{\"relation\":[{\"ctype\":\"device\",\"id\":\"panel\",\"name\":\"" + App.get_name()
  // +
  //                                    "\",\"online\":true}]}");

  this->send_relay_states_();

  this->send_all_widgets_();
}

void NSPanel::setup() {
  this->set_interval(60000, [this]() { this->send_time_(); });

  this->set_timeout(15000, [this]() { this->initialize(); });
}

void NSPanel::loop() {
  uint8_t d;

  while (this->available()) {
    this->read_byte(&d);
    this->buffer_.push_back(d);
    if (!this->process_data_()) {
      ESP_LOGD(TAG, "Received: 0x%02x", d);
      this->buffer_.clear();
    }
  }

  this->send_wifi_state_();
}

bool NSPanel::process_data_() {
  uint32_t at = this->buffer_.size() - 1;
  auto *data = &this->buffer_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0x55)
  if (at == 0)
    return new_byte == 0x55;
  // Byte 1: HEADER2 (always 0xAA)
  if (at == 1)
    return new_byte == 0xAA;

  if (at == 2)
    return true;
  uint8_t type = data[2];

  if (at == 3 || at == 4)
    return true;
  uint16_t length = encode_uint16(data[4], data[3]);

  // Wait until all data comes in
  if (at - 5 < length)
    return true;

  if (at == 5 + length || at == 5 + length + 1)
    return true;

  uint16_t crc16 = encode_uint16(data[5 + length + 1], data[5 + length]);
  uint16_t calculated_crc16 = NSPanel::crc16(data, 5 + length);

  if (crc16 != calculated_crc16) {
    ESP_LOGW(TAG, "Received invalid message checksum %02X!=%02X", crc16, calculated_crc16);
    return false;
  }

  const uint8_t *message_data = data + 5;
  std::string message(message_data, message_data + length);
  if (message == SUCCESS_RESPONSE) {
    ESP_LOGD(TAG, "Received success response");
    return false;
  }
  ESP_LOGD(TAG, "Received NSPanel: Type=0x%02X PAYLOAD=%s RAW=[%s]", type, message.c_str(),
           hexencode(message_data, length).c_str());
  json::parse_json(message, [this, type, message](JsonObject &root) { this->process_command_(type, root, message); });
  return false;
}

void NSPanel::process_command_(uint8_t type, JsonObject &root, const std::string &message) {
  uint8_t id = uint8_t(root["id"]);
  auto widget = this->widgets_[id - 1];

  switch (type) {
    case 0x86: {
      if (root.containsKey("ctype") && strcasecmp(root["ctype"], "group") == 0) {  // Group
        // {"ctype":"group","id":"3","params":{"switch":"off","switches":[{"switch":"off","outlet":0}]}}
        // {"ctype":"group","id":"6","params":{"switches":[{"switch":"on","outlet":0},{"switch":"off","outlet":1}]}}

        auto params = root["params"];

        auto &switches = params["switches"].as<JsonArray>();

        for (auto switch_object : switches) {
          uint8_t item_index = uint8_t(switch_object["outlet"]);
          bool on = parse_on_off(switch_object["switch"]) == PARSE_ON;
          auto trigger = widget.items[item_index].trigger;
          trigger->trigger(on);
        }

      } else {  // Scene
        widget.trigger->trigger();
      }
      break;
    }
    default:
      ESP_LOGW(TAG, "Unsupported command received! 0x%02X - %s", type, message.c_str());
      break;
  }
}

void NSPanel::control_switch(GroupItem item, bool state) {
  this->send_json_command_(0x86, "{\"relation\":{\"id\":\"" + to_string(item.widget_id) +
                                     "\",\"params\":{\"switches\":[{\"switch\":\"" + to_string(state ? "on" : "off") +
                                     "\",\"outlet\":" + to_string(item.id) + "}]}}}");
}

// {"relation":{"id":"2","params":{"switches":[{"switch":"off","outlet":1}]}}}
// {"relation":{"id":"5","params":{"switches":[{"switch":"off","outlet":0}]}}}

void NSPanel::send_relay_states_() {
  this->send_json_command_(
      0x87, "{\"switches\":[{\"outlet\":0,\"switch\":\"" + to_string(this->relay_1_->state ? "on" : "off") +
                "\"},{\"outlet\":1,\"switch\":\"" + to_string(this->relay_2_->state ? "on" : "off") + "\"}]}");
}

void NSPanel::send_time_() {
  auto time = this->time_->now();
  if (!time.is_valid())
    return;
  this->send_json_command_(0x82, "{\"year\":" + to_string(time.year) + ",\"mon\":" + to_string(time.month) +
                                     ",\"day\":" + to_string(time.day_of_month) + ",\"hour\":" + to_string(time.hour) +
                                     ",\"min\":" + to_string(time.minute) +
                                     ",\"week\":" + to_string(time.day_of_week - 1) + "}");
}

void NSPanel::send_temperature_(float temperature) {
  this->send_json_command_(0x83, "{\"temperature\":" + to_string(temperature).substr(0, 5) +
                                     ",\"tempUnit\":" + to_string(this->temperature_celcius_ ? 0 : 1) + "}");
}

void NSPanel::send_eco_mode_(bool eco_mode) {
  this->send_json_command_(0x87, "{\"HMI_dimOpen\":" + to_string(eco_mode ? 1 : 0) + "}");
}

void NSPanel::send_wifi_state_() {
  uint8_t now = millis();

  bool connected = wifi::global_wifi_component->is_connected() && remote_is_connected();

  uint8_t rssi = 0;
  if (!connected) {
    if (this->last_connected_) {
      this->send_json_command_(0x85, "{\"wifiState\":\"nonetwork\",\"rssiLevel\":0}");
      this->last_connected_ = false;
    }
  } else {
    if (!last_connected_ || this->last_wifi_sent_at_ + 60000 < now) {
      rssi = (wifi::global_wifi_component->wifi_rssi() * -1) / 20.0f;
      this->send_json_command_(0x85, "{\"wifiState\":\"connected\",\"rssiLevel\":" + to_string(rssi) + "}");
      this->last_connected_ = true;
      this->last_wifi_sent_at_ = now;
    }
  }
}

void NSPanel::send_weather_data(WeatherIcon icon, int8_t temperature, int8_t min, int8_t max) {
  std::string data = "{\"HMI_weather\":" + to_string((uint8_t) icon) +
                     ",\"HMI_outdoorTemp\":{\"current\":" + to_string(temperature) + ",\"range\":\"" +
                     (to_string(min) + "," + to_string(max)).substr(0, 5) + "\"}}";
  this->send_json_command_(0x81, data);
}

void NSPanel::send_all_widgets_() {
  for (uint8_t i = 1; i < 9; i++) {
    auto widget = this->widgets_[i - 1];
    switch (widget.type) {
      case EMPTY:
        this->send_json_command_(0x86, "{\"index\":" + to_string(i) + ",\"type\":\"delete\"}");
        continue;
      case DEVICE:
        this->send_json_command_(0x86, "{\"HMI_resources\":[{\"index\":" + to_string(i) +
                                           ",\"ctype\":\"device\",\"id\":\"" + to_string(i) +
                                           "\",\"uiid\":" + to_string(widget.uiid) + "}]}");
        this->send_json_command_(0x86, "{\"relation\":[{\"ctype\":\"device\",\"id\":\"" + to_string(i) +
                                           "\",\"name\":\"" + widget.name.substr(0, 7) +
                                           "\",\"online\":true, \"params\":{\"switch\":\"on\"}}]}");
        break;
      case GROUP: {
        this->send_json_command_(0x86, "{\"HMI_resources\":[{\"index\":" + to_string(i) +
                                           ",\"ctype\":\"group\",\"id\":\"" + to_string(i) +
                                           "\",\"uiid\":" + to_string(widget.uiid) + "}]}");

        std::string str = json::build_json([i, widget](JsonObject &root) {
          JsonArray &relation_list = root.createNestedArray("relation");
          JsonObject &relation = relation_list.createNestedObject();
          relation["ctype"] = "group";
          relation["id"] = to_string(i);
          relation["name"] = widget.name;
          relation["online"] = true;
          JsonObject &params = relation.createNestedObject("params");
          if (widget.items.size() == 1) {
            params["switch"] = "on";
          } else {
            JsonArray &switches_list = params.createNestedArray("switches");
            for (uint8_t j = 0; j < widget.items.size(); j++) {
              GroupItem item = widget.items[j];
              JsonObject &item_obj = switches_list.createNestedObject();
              item_obj["outlet"] = j;
              item_obj["name"] = item.name;
            }
          }
        });

        this->send_json_command_(0x86, str);
        break;
      }
      case SCENE:
        this->send_json_command_(0x86, "{\"HMI_resources\":[{\"index\":" + to_string(i) +
                                           ",\"ctype\":\"scene\",\"id\":\"" + to_string(i) + "\"}]}");
        this->send_json_command_(0x86, "{\"relation\":[{\"ctype\":\"scene\",\"id\":\"" + to_string(i) +
                                           "\",\"name\":\"" + widget.name.substr(0, 7) + "\"}]}");
        break;
    }
  }
}

void NSPanel::dump_config() { ESP_LOGCONFIG(TAG, "NSPanel:"); }

void NSPanel::send_nextion_command_(const std::string &command) {
  ESP_LOGD(TAG, "Sending: %s", command.c_str());
  this->write_str(command.c_str());
  const uint8_t to_send[3] = {0xFF, 0xFF, 0xFF};
  this->write_array(to_send, sizeof(to_send));
}

void NSPanel::send_json_command_(uint8_t type, const std::string &command) {
  ESP_LOGD(TAG, "Sending JSON command: %s", command.c_str());
  std::vector<uint8_t> data = {0x55, 0xAA};
  data.push_back(type);
  data.push_back(command.length() & 0xFF);
  data.push_back((command.length() >> 8) & 0xFF);
  data.insert(data.end(), command.begin(), command.end());
  auto crc = NSPanel::crc16(data.data(), data.size());
  data.push_back(crc & 0xFF);
  data.push_back((crc >> 8) & 0xFF);
  this->write_array(data);
}

uint16_t NSPanel::crc16(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

}  // namespace nspanel
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
