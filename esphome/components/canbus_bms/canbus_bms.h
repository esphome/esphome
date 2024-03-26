#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include <set>
#include <vector>
#include <map>

namespace esphome {
namespace canbus_bms {

static const char *const CONF_HEALTH = "health";
static const char *const CONF_CHARGE = "charge";
static const char *const CONF_CURRENT = "current";
static const char *const CONF_VOLTAGE = "voltage";
static const char *const CONF_TEMPERATURE = "temperature";
static const char *const CONF_MAX_CHARGE_VOLTAGE = "max_charge_voltage";
static const char *const CONF_MAX_CHARGE_CURRENT = "max_charge_current";
static const char *const CONF_MAX_DISCHARGE_CURRENT = "max_discharge_current";
static const char *const CONF_MIN_DISCHARGE_VOLTAGE = "min_discharge_voltage";

// alarm and warning flags.

static const uint32_t FLAG_GENERAL_ALARM = 0x0001;
static const uint32_t FLAG_HIGH_VOLTAGE = 0x0002;
static const uint32_t FLAG_LOW_VOLTAGE = 0x0004;
static const uint32_t FLAG_HIGH_TEMPERATURE = 0x0008;
static const uint32_t FLAG_LOW_TEMPERATURE = 0x0010;
static const uint32_t FLAG_HIGH_TEMPERATURE_CHARGE = 0x0020;
static const uint32_t FLAG_LOW_TEMPERATURE_CHARGE = 0x0040;
static const uint32_t FLAG_HIGH_CURRENT = 0x0080;
static const uint32_t FLAG_HIGH_CURRENT_CHARGE = 0x0100;
static const uint32_t FLAG_CONTACTOR_ERROR = 0x0200;
static const uint32_t FLAG_SHORT_CIRCUIT = 0x0400;
static const uint32_t FLAG_BMS_INTERNAL_ERROR = 0x0800;
static const uint32_t FLAG_CELL_IMBALANCE = 0x1000;

// request flags

enum Requests {
  REQ_CHARGE_ENABLE,
  REQ_DISCHARGE_ENABLE,
  REQ_FORCE_CHARGE_1,
  REQ_FORCE_CHARGE_2,
  REQ_FULL_CHARGE,
};

// this should be split out into a top-level bms component, with canbus_bms as a platform.
class Bms {
 public:
  virtual float get_voltage() = 0;
  virtual float get_current() = 0;
  virtual float get_charge() = 0;
  virtual float get_temperature() = 0;
  virtual float get_health() = 0;
  virtual float get_max_voltage() = 0;
  virtual float get_min_voltage() = 0;
  virtual float get_max_charge_current() = 0;
  virtual float get_max_discharge_current() = 0;
  virtual uint32_t get_alarms() = 0;
  virtual uint32_t get_warnings() = 0;
  virtual uint32_t get_requests() = 0;
};

class TextSensorDesc {
  friend class CanbusBmsComponent;

 public:
  TextSensorDesc(text_sensor::TextSensor *sensor, int msg_id) : sensor_{sensor}, msg_id_{msg_id} {}

 protected:
  text_sensor::TextSensor *sensor_;
  const int msg_id_;
  uint32_t last_time_ = 0;  // records last time a value was sent
};

class FlagDesc {
  friend class CanbusBmsComponent;

 public:
  FlagDesc(const char *key, const char *message, int msg_id, int offset, int bit_no, int warn_offset, int warn_bit_no,
           int bit_mask)
      : key_{key},
        message_{message},
        msg_id_{msg_id},
        offset_{offset},
        bit_no_{bit_no},
        warn_offset_{warn_offset},
        warn_bit_no_{warn_bit_no},
        bit_mask_{bit_mask} {}

 protected:
  const char *key_;
  const char *message_;
  const int msg_id_;
  const int offset_;
  const int bit_no_;
  const int warn_offset_;
  const int warn_bit_no_;
  const int bit_mask_;
  bool warned_ = false;
  bool alarmed_ = false;
};

class BinarySensorDesc {
  friend class CanbusBmsComponent;

 public:
  BinarySensorDesc(const char *key, binary_sensor::BinarySensor *sensor, int msg_id, int offset, int bit_no,
                   bool filtered, Requests index)
      : key_{key},
        sensor_{sensor},
        msg_id_{msg_id},
        offset_{offset},
        bit_no_{bit_no},
        filtered_{filtered},
        bit_mask_{1u << index} {}

 protected:
  const char *key_;
  binary_sensor::BinarySensor *sensor_;
  const int msg_id_;
  const int offset_;
  const int bit_no_;
  const bool filtered_;  // if sensor has its own filter chain
  const uint32_t bit_mask_;
  uint32_t last_time_ = 0;  // records last time a value was sent
  bool last_value_ = 0;

  void publish_(bool value) {
    this->last_time_ = millis();
    this->last_value_ = value;
    if (this->sensor_ != nullptr)
      this->sensor_->publish_state(value);
  }
};

class SensorDesc {
  friend class CanbusBmsComponent;

 public:
  SensorDesc(const char *key, sensor::Sensor *sensor, int msg_id, int offset, int length, float scale, bool filtered)
      : key_{key},
        sensor_{sensor},
        msg_id_{msg_id},
        offset_{offset},
        length_{length},
        scale_{scale},
        filtered_{filtered} {}

 protected:
  const char *key_;
  sensor::Sensor *sensor_;
  const int msg_id_;        // message id for this sensor
  const int offset_;        // byte position in message
  const int length_;        // length in bytes
  const float scale_;       // scale factor
  const bool filtered_;     // if sensor has its own filter chain
  uint32_t last_time_ = 0;  // records last time a value was sent
  float *sensor_value_{};   // guaranteed to be non-null before use

  void publish_(float value) {
    if (!this->filtered_)
      this->last_time_ = millis();
    if (this->sensor_ != nullptr)
      this->sensor_->publish_state(value);
    *this->sensor_value_ = value;
  }
};

/**
 * This class captures state from a CANBus connected BMS, and reports sensor values.
 * It implements Action so that it can be connected to an Automation.
 */

class CanbusBmsComponent : public Action<std::vector<uint8_t>, uint32_t, bool>, public PollingComponent, public Bms {
 public:
  CanbusBmsComponent(uint32_t throttle, uint32_t timeout, const char *name, bool debug)
      : PollingComponent(std::min(throttle, (uint32_t) 15000U)),
        name_{name},
        debug_{debug},
        throttle_{throttle},
        timeout_{timeout} {}

  void update() override;
  void dump_config() override;
  float get_voltage() override;
  float get_current() override;
  float get_charge() override;
  float get_temperature() override;
  float get_health() override;
  float get_max_voltage() override;
  float get_min_voltage() override;
  float get_max_charge_current() override;
  float get_max_discharge_current() override;
  uint32_t get_alarms() override;
  uint32_t get_warnings() override;
  uint32_t get_requests() override;

  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  // called when a CAN Bus message is received
  void play(std::vector<uint8_t> data, uint32_t can_id, bool remote_transmission_request) override;
  float get_setup_priority() const override;

  // add a list of sensors that are encoded in a given message.
  void add_sensor_list(uint32_t msg_id, std::vector<SensorDesc *> *sensors) {
    this->sensor_map_[msg_id] = sensors;
    for (SensorDesc *sensor : *sensors) {
      this->sensor_values_[sensor->key_] = NAN;
      sensor->sensor_value_ = &this->sensor_values_[sensor->key_];
      this->sensors_.push_back(sensor);
    }
  }

  void add_binary_sensor_list(uint32_t msg_id, std::vector<BinarySensorDesc *> *sensors) {
    this->binary_sensor_map_[msg_id] = sensors;
    for (BinarySensorDesc *sensor : *sensors) {
      this->binary_sensors_.push_back(sensor);
    }
  }

  void add_text_sensor_list(uint32_t msg_id, std::vector<TextSensorDesc *> *sensors) {
    this->text_sensor_map_[msg_id] = sensors;
    for (TextSensorDesc *sensor : *sensors) {
      this->text_sensors_.push_back(sensor);
    }
  }

  void add_flag_list(uint32_t msg_id, std::vector<FlagDesc *> *flags) {
    this->flag_map_[msg_id] = flags;
    for (FlagDesc *flag : *flags) {
      this->flags_.push_back(flag);
    }
  }

  // set special sensors

  void set_warning_binary_sensor(binary_sensor::BinarySensor *sensor) { this->warning_binary_sensor_ = sensor; }

  void set_alarm_binary_sensor(binary_sensor::BinarySensor *sensor) { this->alarm_binary_sensor_ = sensor; }

  void set_warning_text_sensor(text_sensor::TextSensor *sensor) { this->warning_text_sensor_ = sensor; }

  void set_alarm_text_sensor(text_sensor::TextSensor *sensor) { this->alarm_text_sensor_ = sensor; }

  // get the last known value of a value with given key
  float get_value(const char *key) {
    if (this->sensor_values_.count(key) != 0)
      return this->sensor_values_[key];
    return NAN;
  }

  // send data to the underlying canbus
  void send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                 const std::vector<uint8_t> &data) {
    if (this->canbus_)
      this->canbus_->send_data(can_id, use_extended_id, remote_transmission_request, data);
  }

 protected:
  // our name
  canbus::Canbus *canbus_;
  const char *name_;
  const bool debug_;
  // min and max intervals between publish
  const uint32_t throttle_;
  const uint32_t timeout_;
  // log received canbus message IDs
  std::set<int> received_ids_;
  // all the sensors we are handling
  std::vector<SensorDesc *> sensors_{};
  std::vector<BinarySensorDesc *> binary_sensors_{};
  std::vector<TextSensorDesc *> text_sensors_{};
  std::vector<FlagDesc *> flags_{};

  // construct maps of the above for efficient message processing
  std::map<int, std::vector<SensorDesc *> *> sensor_map_;
  std::map<int, std::vector<BinarySensorDesc *> *> binary_sensor_map_;
  std::map<int, std::vector<TextSensorDesc *> *> text_sensor_map_;
  std::map<int, std::vector<FlagDesc *> *> flag_map_;

  std::map<const char *, float> sensor_values_;

  text_sensor::TextSensor *alarm_text_sensor_{};
  text_sensor::TextSensor *warning_text_sensor_{};
  binary_sensor::BinarySensor *alarm_binary_sensor_{};
  binary_sensor::BinarySensor *warning_binary_sensor_{};

  void update_alarms_();
};

/**
 * Class used to link the canbus receiver to our component. Creation of an instance automatically
 * sets up the necessary chain of events:
 *       canbus->trigger->automation->bmscomponent
 * so that a received canbus message results in the CanbusBmsComponent::play() function being called.
 */
class BmsTrigger : public canbus::CanbusTrigger {
 public:
  BmsTrigger(Action<std::vector<uint8_t>, uint32_t, bool> *bms_component, canbus::Canbus *parent)
      : canbus::CanbusTrigger(parent, 0, 0, false) {
    (new Automation<std::vector<uint8_t>, uint32_t, bool>(this))->add_actions({bms_component});
  }
};

}  // namespace canbus_bms
}  // namespace esphome
