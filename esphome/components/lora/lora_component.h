#pragma once
#include <regex>
#include "esphome/core/defines.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif  // USE_SWITCH

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif  // USE_TEXT_SENSOR

namespace esphome {
namespace lora {

#define LOG_LORA_PACKET(lora_packet) \
  ESP_LOGD(TAG, "Lora Packet"); \
  ESP_LOGD(TAG, "appname %s", ((lora_packet))->appname.c_str()); \
  ESP_LOGD(TAG, "component type %d", ((lora_packet))->component_type); \
  ESP_LOGD(TAG, "component name %s", ((lora_packet))->component_name.c_str()); \
  ESP_LOGD(TAG, "state %lf", ((lora_packet))->state); \
  ESP_LOGD(TAG, "state string %s", ((lora_packet))->state_str.c_str()); \
  ESP_LOGD(TAG, "RSSI %d", ((lora_packet))->rssi); \
  ESP_LOGD(TAG, "Snr: %lf", ((lora_packet))->snr);

class LoraPacket {
 public:
  std::string appname;
  int component_type = 0;
  std::string component_name;
  float state = 0;
  std::string state_str;
  int rssi = 0;
  float snr = 0.0;
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
#endif  // USE_SWITCH

#ifdef USE_SENSOR
class LoraSensorComponent : public LoraBaseComponent {
 public:
  sensor::Sensor *component;
};
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
class LoraBinarySensorComponent : public LoraBaseComponent {
 public:
  binary_sensor::BinarySensor *component;
};
#endif  // USE_BINARY_SENSOR

#ifdef USE_TEXT_SENSOR
class LoraTextSensorComponent : public LoraBaseComponent {
 public:
  text_sensor::TextSensor *component;
};
#endif  // USE_TEXT_SENSOR

class LoraComponent : public Component {
 public:
  virtual void send_printf(const char *format, ...) __attribute__((format(printf, 2, 3))) = 0;

  void set_sync_word_internal(int sync_word) { this->sync_word_ = sync_word; }
  void process_lora_packet(LoraPacket *lora_packet);
  std::string build_to_send(const std::string &type, const std::string &name, const std::string &state);

#if defined(USE_BINARY_SENSOR) || defined(USE_SWITCH)
  bool process_component(LoraBaseComponent *lora_component, bool state);
#endif  // defined(USE_BINARY_SENSOR) || defined(USE_SWITCH)

#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(binary_sensor::BinarySensor *component, bool send_to_lora, bool receive_from_lora,
                              const std::string &lora_name);
#endif  // USE_BINARY_SENSOR

#ifdef USE_SWITCH
  void register_switch(switch_::Switch *component, bool send_to_lora, bool receive_from_lora,
                       const std::string &lora_name);
#endif  // USE_SWITCH

#ifdef USE_TEXT_SENSOR
  bool process_component(LoraBaseComponent *lora_component, const std::string& state);
  void register_text_sensor(text_sensor::TextSensor *component, bool send_to_lora, bool receive_from_lora,
                            const std::string &lora_name);
#endif  // USE_TEXT_SENSOR

#ifdef USE_SENSOR
  bool process_component(LoraBaseComponent *lora_component, float state);
  void register_sensor(sensor::Sensor *sensor, bool send_to_lora, bool receive_from_lora, const std::string &lora_name);
#endif  // USE_SENSOR

  int last_rssi = 0;
  int last_snr = 0;

 protected:
#ifdef USE_SWITCH
  std::vector<LoraSwitchComponent *> switches_;
#endif  // USE_SWITCH

#ifdef USE_SENSOR
  std::vector<LoraSensorComponent *> sensors_;
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
  std::vector<LoraBinarySensorComponent *> binary_sensors_;
#endif  // USE_BINARY_SENSOR

#ifdef USE_TEXT_SENSOR
  std::vector<LoraTextSensorComponent *> text_sensors_;
#endif  // USE_TEXT_SENSOR

  std::string get_app_name_() { return str_sanitize(App.get_name()); }
  int sync_word_;
  std::string lora_delimiter_ = {0x1D};
  std::string lora_group_start_delimiter_ = {0x1E};
  std::string lora_group_end_delimiter_ = {0x1F};

  std::regex lora_regex_group_end_delimiter_{lora_group_end_delimiter_};
  std::regex lora_regex_delimiter_{lora_delimiter_};
  bool is_setup_;
};
}  // namespace lora
}  // namespace esphome
