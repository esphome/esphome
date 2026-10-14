#pragma once
#ifdef USE_ESP32
#include "esphome/core/component.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/core/helpers.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
namespace esphome::toptronic {
class TopTronic;
enum TypeName { U8, U16, U32, S8, S16, S32, S64 };  // value encoding of a datapoint
enum SensorType { SENSOR, TEXTSENSOR };             // downcast discriminator (no dynamic_cast in ESPHome builds)
uint32_t build_can_id(uint16_t sender_id, uint16_t receiver_mask);
std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint);
std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint,
                                       const std::vector<uint8_t> &value);
// Shared entity base. update() fires the GET wired up by the hub; SET entities never poll.
class TopTronicBase : public PollingComponent {
 public:
  void set_function_group(uint8_t function_group) { this->function_group_ = function_group; }
  void set_function_number(uint8_t function_number) { this->function_number_ = function_number; }
  void set_datapoint(uint16_t datapoint) { this->datapoint_ = datapoint; }
  uint32_t get_id();  // fg | fn << 8 | datapoint << 16; hub map key
  const std::vector<uint8_t> &get_request_data();
  void cache_request_data();
  virtual SensorType type() = 0;
  void update() override;
  void add_on_update_callback(std::function<void()> &&callback);
  void add_on_set_callback(std::function<void(const std::vector<uint8_t> &)> &&callback);
  virtual const StringRef &get_name() const = 0;

 protected:
  uint8_t function_group_;
  uint8_t function_number_;
  uint16_t datapoint_;
  std::vector<uint8_t> request_data_;  // GET payload, built once at setup
  CallbackManager<void()> update_callback_;
  CallbackManager<void(const std::vector<uint8_t> &)> set_callback_;
};
#ifdef USE_SENSOR
class TopTronicSensor : public sensor::Sensor, public TopTronicBase {
 public:
  void set_type(TypeName type) { this->type_ = type; }
  float parse_value(const uint8_t *data, size_t len);
  size_t value_width() const;
  SensorType type() override { return SENSOR; }
  const StringRef &get_name() const override { return this->sensor::Sensor::get_name(); }

 protected:
  TypeName type_;
};
#endif
#ifdef USE_NUMBER
class TopTronicNumber : public number::Number, public TopTronicBase {
 public:
  TopTronicNumber() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }
  float get_multiplier() { return this->multiplier_; }
  SensorType type() override { return SENSOR; }
  const StringRef &get_name() const override { return this->number::Number::get_name(); }
  void control(float value) override;

 protected:
  TypeName type_;
  float multiplier_{1.0f};  // from YAML decimal (decimal: 1 -> x10)
};
#endif
#ifdef USE_TEXT_SENSOR
class TopTronicTextSensor : public text_sensor::TextSensor, public TopTronicBase {
 public:
  std::string parse_value(const uint8_t *data, size_t len);
  SensorType type() override { return TEXTSENSOR; }
  const StringRef &get_name() const override { return this->text_sensor::TextSensor::get_name(); }
  void add_option(uint8_t value, const std::string &text) {
    this->to_text_[value] = text;
    this->to_value_[text] = value;
  }

 protected:
  std::map<uint8_t, std::string> to_text_;
  std::map<std::string, uint8_t> to_value_;
};
#endif
#ifdef USE_SELECT
class TopTronicSelect : public select::Select, public TopTronicBase {
 public:
  TopTronicSelect() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  SensorType type() override { return TEXTSENSOR; }
  const StringRef &get_name() const override { return this->select::Select::get_name(); }
  void add_option(uint8_t value, const std::string &text) {
    this->to_text_[value] = text;
    this->to_value_[text] = value;
  }
  void control(const std::string &text) override;

 protected:
  std::map<uint8_t, std::string> to_text_;
  std::map<std::string, uint8_t> to_value_;
  TypeName type_{U8};
};
#endif
class TopTronicDevice {
 public:
  std::unordered_map<uint32_t, TopTronicBase *> &get_sensors() { return this->sensors_; }
  std::unordered_map<uint32_t, TopTronicBase *> &get_inputs() { return this->inputs_; }

 private:
  std::unordered_map<uint32_t, TopTronicBase *> sensors_;
  std::unordered_map<uint32_t, TopTronicBase *> inputs_;
};
class TopTronic : public Component {
 public:
  explicit TopTronic(canbus::Canbus *canbus);
  void add_sensor(TopTronicBase *sensor);
  void add_input(TopTronicBase *input);
  void parse_frame(const std::vector<uint8_t> &data, uint32_t can_id, bool remote_transmission_request);
  void register_sensor_callbacks();
  void register_input_callbacks();
  void configure_hub();  // config-phase wiring (see __init__.py) instead of setup()
  void update_all();     // GET for every sensor, same path as the polling scheduler
  void set_device_type(uint16_t device_type) { this->device_type_ = device_type; }
  void set_device_addr(uint8_t device_addr) { this->device_addr_ = device_addr; }
  uint16_t get_device_id() { return this->device_type_ | this->device_addr_; }
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  struct PendingMessage {
    std::vector<uint8_t> data;
    uint8_t remaining_frames{0};
    uint32_t last_update_ms{0};
  };
  TopTronicDevice *get_or_create_device_(uint32_t device_id);
  void link_inputs_();
  TopTronicBase *get_sensor_(uint32_t device_id, uint32_t sensor_id);
  void interpret_message_(const uint8_t *data, size_t len, uint32_t can_id);
  bool owns_device_(uint32_t device_id) const { return this->devices_.contains(device_id); }
  canbus::Canbus *canbus_;
  std::unordered_map<uint32_t, std::unique_ptr<TopTronicDevice>> devices_;
  std::unordered_map<uint32_t, PendingMessage> pending_messages_;
  uint16_t device_type_;
  uint8_t device_addr_;
  uint32_t last_cleanup_ms_{0};
};
}  // namespace esphome::toptronic
#endif  // USE_ESP32
