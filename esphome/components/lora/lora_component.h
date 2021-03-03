#pragma once
#include <regex>
#include "esphome/core/defines.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace lora {

#define LOG_LORA_PACKET(lora_packet) \
  ESP_LOGD(TAG, "Lora Packet"); \
  ESP_LOGD(TAG, "appname %s", lora_packet.appname.c_str()); \
  ESP_LOGD(TAG, "component type %d", lora_packet.component_type); \
  ESP_LOGD(TAG, "component name %s", lora_packet.component_name.c_str()); \
  ESP_LOGD(TAG, "state %lf", lora_packet.state); \
  ESP_LOGD(TAG, "RSSI %d", lora_packet.rssi);

class LoraPacket {
 public:
  std::string appname;
  int component_type = 0;
  std::string component_name;
  float state = 0;
  int rssi = 0;

  // float accuracy = 0;
  // std::string device_class;
  // std::string icon;
  // std::string uom;
};

class LoraBaseComponent {
 public:
  bool send_to_lora = false;
  bool receive_from_lora = false;
  std::string lora_name;
  std::string type;
};

#ifdef USE_SWITCH
class LoraSwitchComponent : public LoraBaseComponent {
 public:
  switch_::Switch *component;
};
#endif

#ifdef USE_SENSOR
class LoraSensorComponent : public LoraBaseComponent {
 public:
  sensor::Sensor *component;
};
#endif

#ifdef USE_BINARY_SENSOR
class LoraBinarySensorComponent : public LoraBaseComponent {
 public:
  binary_sensor::BinarySensor *component;
};
#endif

class LoraComponent : public Component {
 public:
  virtual void send_printf(const char *format, ...) __attribute__((format(printf, 2, 3))) = 0;

  void set_sync_word_internal(int sync_word) { this->sync_word_ = sync_word; }
  void process_lora_packet(LoraPacket lora_packet);
  std::string build_to_send_(std::string type, std::string name, std::string state);

#if defined(USE_BINARY_SENSOR) || defined(USE_BINARY_SENSOR)
  bool process_component_(LoraBaseComponent *lora_sensor, bool state);
#endif

#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(binary_sensor::BinarySensor *component, bool send_to_lora, bool receive_from_lora,
                              std::string lora_name);
#endif

#ifdef USE_SWITCH
  void register_switch(switch_::Switch *component, bool send_to_lora, bool receive_from_lora, std::string lora_name);
#endif

#ifdef USE_SENSOR
  bool process_component_(LoraBaseComponent *lora_sensor, float state);
  void register_sensor(sensor::Sensor *sensor, bool send_to_lora, bool receive_from_lora, std::string lora_name);
#endif
  int last_rssi = 0;

 protected:
#ifdef USE_SWITCH
  std::vector<LoraSwitchComponent *> switches_;
#endif

#ifdef USE_SENSOR
  std::vector<LoraSensorComponent *> sensors_;
#endif

#ifdef USE_BINARY_SENSOR
  std::vector<LoraBinarySensorComponent *> binary_sensors_;
#endif

  std::string get_app_name_() { return sanitize_string_allowlist(App.get_name(), HOSTNAME_CHARACTER_ALLOWLIST); }
  int sync_word_;
  std::string lora_delimiter_ = "|";
  std::regex lora_regex_delimiter_{"\\|"};
};
}  // namespace lora
}  // namespace esphome
