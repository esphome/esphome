#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <array>
#include <unordered_map>
#include <vector>

namespace esphome::toptronic {

class TopTronic;
enum TypeName { U8, U16, U32, S8, S16, S32, S64 };  // value encoding of a datapoint
enum SensorType { SENSOR, TEXTSENSOR };             // downcast discriminator (no dynamic_cast in ESPHome builds)

static constexpr size_t GET_REQUEST_LEN = 6;
using GetRequest = std::array<uint8_t, GET_REQUEST_LEN>;

// Shared entity base. update() fires the GET wired up by the hub; SET entities never poll.
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
  void add_on_set_callback(std::function<void(const std::vector<uint8_t> &)> &&callback);
  virtual const LogString *get_log_name() const = 0;

 protected:
  uint8_t function_group_;
  uint8_t function_number_;
  uint16_t datapoint_;
  GetRequest request_data_{};  // GET payload, built once at setup
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
  const LogString *get_log_name() const override { return this->sensor::Sensor::get_log_name(); }

 protected:
  TypeName type_;
};
#endif

class TopTronicDevice {
 public:
  std::unordered_map<uint32_t, TopTronicBase *> &get_sensors() { return this->sensors_; }

 private:
  std::unordered_map<uint32_t, TopTronicBase *> sensors_;
};

class TopTronic : public Component {
 public:
  TopTronic(canbus::Canbus *canbus, uint16_t device_id);

  void add_sensor(TopTronicBase *sensor);
  void register_sensor_callbacks();
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

  /// Feeds one frame from this device into reassembly. Returns true and points msg/len at the payload when
  /// a message is complete (CRC already checked and stripped); the pointer is valid until the next frame.
  bool parse_frame_(const uint8_t *data, size_t size, uint32_t can_id, const uint8_t **msg, size_t *len);
  void interpret_message_(const uint8_t *data, size_t len);

  canbus::Canbus *canbus_;
  TopTronicDevice device_;
  std::array<Slot, NUM_SLOTS> slots_{};
  uint16_t device_id_;
};

}  // namespace esphome::toptronic
