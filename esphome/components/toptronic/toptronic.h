#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace esphome::toptronic {

class TopTronic;
enum TypeName { U8, U16, U32, S8, S16, S32, S64 };  // value encoding of a datapoint
enum SensorType { SENSOR, TEXTSENSOR };  // downcast discriminator (no dynamic_cast in ESPHome builds)

static constexpr size_t GET_REQUEST_LEN = 6;
using GetRequest = std::array<uint8_t, GET_REQUEST_LEN>;
// A SET is the GET header with the SET command and a value of up to 8 bytes appended
static constexpr size_t MAX_SET_VALUE_LEN = 8;
static constexpr size_t MAX_SET_REQUEST_LEN = GET_REQUEST_LEN + MAX_SET_VALUE_LEN;
using SetRequest = std::array<uint8_t, MAX_SET_REQUEST_LEN>;

// Shared entity base. update() fires the GET wired up by the hub.
class TopTronicBase : public PollingComponent {
 public:
  void set_function_group(uint8_t function_group) { this->function_group_ = function_group; }
  void set_function_number(uint8_t function_number) { this->function_number_ = function_number; }
  void set_datapoint(uint16_t datapoint) { this->datapoint_ = datapoint; }
  uint32_t get_id();  // fg | fn << 8 | datapoint << 16; hub map key
  const GetRequest &get_request_data() const { return this->request_data_; }
  void cache_request_data();
  virtual SensorType type() = 0;
  void update() override;
  void add_on_update_callback(std::function<void()> &&callback);
  void add_on_set_callback(std::function<void(const uint8_t *, size_t)> &&callback);
  /// Set by the hub for an input without a read entity: control() then publishes the written value itself
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  virtual const LogString *get_log_name() const = 0;

 protected:
  uint8_t function_group_;
  uint8_t function_number_;
  uint16_t datapoint_;
  GetRequest request_data_{};  // GET payload, built once at setup
  bool optimistic_{false};
  CallbackManager<void()> update_callback_;
  /// Builds the SET request for value (scaled, rounded and clamped to type) and hands it to the hub
  void send_set_(float value, TypeName type);

  CallbackManager<void(const uint8_t *, size_t)> set_callback_;
};

class TopTronicSensor : public sensor::Sensor, public TopTronicBase {
 public:
  void set_type(TypeName type) { this->type_ = type; }
  float parse_value(const uint8_t *data, size_t len);
  size_t value_width() const;
  SensorType type() override { return SENSOR; }
  const LogString *get_log_name() const override { return this->sensor::Sensor::get_log_name(); }

 protected:
  TypeName type_;
};

#ifdef USE_NUMBER
class TopTronicNumber : public number::Number, public TopTronicBase {
 public:
  TopTronicNumber() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }
  float get_multiplier() { return this->multiplier_; }
  SensorType type() override { return SENSOR; }
  const LogString *get_log_name() const override { return this->number::Number::get_log_name(); }
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
  const LogString *get_log_name() const override { return this->text_sensor::TextSensor::get_log_name(); }
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
  const LogString *get_log_name() const override { return this->select::Select::get_log_name(); }
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

class TopTronic : public Component {
 public:
  TopTronic(canbus::Canbus *canbus, uint16_t device_id);

  void add_sensor(TopTronicBase *sensor);
  void add_input(TopTronicBase *input);
  void register_sensor_callbacks();
  void register_input_callbacks();
  void setup() override;
  void dump_config() override;

 protected:
  // The 5-bit frame count field caps a message at 31 frames of up to 7 payload bytes
  static constexpr size_t MAX_FRAMES_PER_MESSAGE = 31;
  static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
  static constexpr size_t MAX_MESSAGE_LEN = MAX_FRAMES_PER_MESSAGE * MAX_CONT_FRAME_PAYLOAD;
  static_assert(MAX_MESSAGE_LEN <= UINT8_MAX, "Slot::len is a uint8_t");
  static constexpr uint32_t MAX_PENDING_AGE_MS = 5000;
  // One sender answers requests one after another; a second slot covers an abandoned message
  static constexpr size_t NUM_SLOTS = 2;

  struct Slot {
    uint32_t last_update_ms;
    uint8_t len;
    uint8_t tag;
    uint8_t remaining;  // continuation frames still expected, 0 when free
    uint8_t data[MAX_MESSAGE_LEN];

    bool is_live(uint32_t now) const {
      return this->remaining != 0 && now - this->last_update_ms <= MAX_PENDING_AGE_MS;
    }
  };

  /// Sends a request, split into a start frame and continuation frames when it does not fit in one
  void send_frames(uint32_t can_id, const uint8_t *data, size_t len);
  /// Feeds one frame from this device into reassembly. Returns true and points msg/len at the payload when
  /// a message is complete (CRC already checked and stripped); the pointer is valid until the next frame.
  bool parse_frame_(const uint8_t *data, size_t size, uint32_t can_id, const uint8_t **msg, size_t *len);
  void interpret_message_(const uint8_t *data, size_t len);
  void link_inputs_();
  TopTronicBase *get_sensor_(uint32_t sensor_id);

  canbus::Canbus *canbus_;
  std::unordered_map<uint32_t, TopTronicBase *> sensors_;
  std::unordered_map<uint32_t, TopTronicBase *> inputs_;
  std::array<Slot, NUM_SLOTS> slots_{};
  uint8_t msg_counter_{0};  // frame tag of the last multi-frame message sent by this hub
  uint16_t device_id_;
};

}  // namespace esphome::toptronic
