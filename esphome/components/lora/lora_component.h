#pragma once
#include <regex>
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace lora {

class LoraPacket {
 public:
  std::string appname;
  int data_type = 0;
  std::string sensor_name;
  float state = 0;
  int rssi = 0;

  float accuracy = 0;
  std::string device_class;
  std::string icon;
  std::string uom;
};

#define LOG_LORA_PACKET(lora_packet) \
  ESP_LOGD(TAG, "Lora Packet"); \
  ESP_LOGD(TAG, "appname %s", lora_packet.appname.c_str()); \
  ESP_LOGD(TAG, "data_type %d", lora_packet.data_type); \
  ESP_LOGD(TAG, "sensor_name %s", lora_packet.sensor_name.c_str()); \
  ESP_LOGD(TAG, "state %lf", lora_packet.state); \
  ESP_LOGD(TAG, "RSSI %d", lora_packet.rssi);

class LoraBaseComponent {
 public:
  bool send_to_lora = false;
  bool receive_from_lora = false;
  std::string lora_name;
};

#ifdef USE_SENSOR
class LoraSensorComponent : public LoraBaseComponent {
 public:
  sensor::Sensor *sensor;
};
#endif

class LoraComponent : public Component {
 public:
  virtual void send_printf(const char *format, ...) __attribute__((format(printf, 2, 3))) = 0;

  void set_sync_word_internal(int sync_word) { this->sync_word_ = sync_word; }
  void process_lora_packet(LoraPacket lora_packet);

#ifdef USE_SENSOR
  void register_sensor(sensor::Sensor *sensor, bool send_to_lora, bool receive_from_lora, std::string lora_name);
#endif

 protected:
#ifdef USE_SENSOR
  std::vector<LoraSensorComponent *> sensors_;
  bool process_sensor_(LoraSensorComponent *lora_sensor, float state);
#endif

  std::string get_app_name_() { return sanitize_string_allowlist(App.get_name(), HOSTNAME_CHARACTER_ALLOWLIST); }
  int sync_word_;
  std::string lora_delimiter_ = "|";
  std::regex lora_regex_delimiter_{"\\|"};
};
}  // namespace lora
}  // namespace esphome
