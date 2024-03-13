#include "canbus_bms.h"
#include "esphome/core/log.h"
#include <sstream>
#include <cmath>

namespace esphome {
namespace canbus_bms {

static const char *const TAG = "canbus_bms";

static const char *const CONF_ALARMS = "alarms";  // these must match the corresponding constants in __init__.py
static const char *const CONF_WARNINGS = "warnings";

static const size_t CAN_MAX_DATA_LENGTH = 8;
static const size_t ALARM_MAX_STR_LEN = 256;  // maximum length of generated alarm/warning summary string

/**
 * Join a set of strings with a comma separator. Store into the given buffer, up to buflen-1 characters.
 * The result will be null-terminated.
 */
static void join(std::set<const char *> *strings, char *buffer, size_t buflen) {
  if (strings->empty()) {
    buffer[0] = 0;
    return;
  }
  std::ostringstream ss;
  // copy with comma separator.
  std::copy(strings->begin(), strings->end(), std::ostream_iterator<std::string>(ss, ","));
  const auto str = ss.str();
  size_t len = std::min(str.size(), buflen);
  strncpy(buffer, str.c_str(), len - 1);  // copy and drop the trailing comma
  buffer[len - 1] = 0;
}

/*
 * Decode a value from a sequence of bytes. Little endian data of length bytes is extracted from data[offset]
 * All data is treated as signed - some things aren't, but will
 * never be big enough to overflow.
 */

static int decode_value(std::vector<uint8_t> data, size_t offset, size_t length) {
  if (length == 0)
    return 0;
  int value = 0;
  for (int i = 0; i != length; i++) {
    value += data[i + offset] << (i * 8);
  }
  // sign extend if required.
  if ((value & 1 << (length * 8 - 1)) != 0)
    value |= ~0U << length * 8;
  return value;
}

void CanbusBmsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CANBus BMS:");
  ESP_LOGCONFIG(TAG, "  Name: %s", this->name_);
  ESP_LOGCONFIG(TAG, "  Throttle: %dms", this->throttle_);
  ESP_LOGCONFIG(TAG, "  Timeout: %dms", this->timeout_);
  ESP_LOGCONFIG(TAG, "  Sensors: %d", this->sensors_.size());
  for (SensorDesc *sensor : this->sensors_) {
    ESP_LOGCONFIG(TAG, "    %s: 0x%X: %d", sensor->key_, sensor->msg_id_, sensor->offset_);
  }
  ESP_LOGCONFIG(TAG, "  Binary Sensors: %d", this->binary_sensors_.size());
  ESP_LOGCONFIG(TAG, "  Text Sensors: %d", this->text_sensors_.size());
}

float CanbusBmsComponent::get_setup_priority() const { return setup_priority::DATA; }

// process sensors for timeout. Only numeric sensors can currently publish an invalid state
// this is called at the throttle rate, which should be more frequent than the timeout value.
// however it may still take longer than the programmed timeout to recognise missing data due to the
// granularity of this check.

void CanbusBmsComponent::update() {
  uint32_t now = millis();
  if (this->timeout_ == 0)
    return;
  for (auto &sensor : this->sensors_) {
    if (!sensor->filtered_ && sensor->last_time_ + this->timeout_ < now) {
      sensor->publish_(NAN);
    }
  }
}

// Check alarm and warning bits, send message to the respective text and binary sensors.
void CanbusBmsComponent::update_alarms_() {
  bool warnings = false;
  bool alarms = false;
  char buffer[ALARM_MAX_STR_LEN];
  std::set<const char *> warnings_set;
  std::set<const char *> alarms_set;

  // collapse warning and alarm flags.
  for (const FlagDesc *flag : this->flags_) {
    if (flag->warned_) {
      warnings = true;
      warnings_set.insert(flag->message_);
    }
    if (flag->alarmed_) {
      alarms = true;
      alarms_set.insert(flag->message_);
    }
  }
  // publish alarms and warnings, if configured.
  if (this->warning_binary_sensor_)
    this->warning_binary_sensor_->publish_state(warnings);
  if (this->alarm_binary_sensor_)
    this->alarm_binary_sensor_->publish_state(alarms);
  if (this->alarm_text_sensor_) {
    join(&alarms_set, buffer, sizeof buffer);
    this->alarm_text_sensor_->publish_state(buffer);
  }
  if (this->warning_text_sensor_) {
    join(&warnings_set, buffer, sizeof buffer);
    this->warning_text_sensor_->publish_state(buffer);
  }
}

// called when a message is received from the associated CANBUS component
void CanbusBmsComponent::play(std::vector<uint8_t> data, uint32_t can_id, bool remote_transmission_request) {
  bool handled = false;
  if (this->debug_)
    ESP_LOGI(TAG, "%s: Received id 0x%02X, len %d", this->name_, can_id, data.size());

  // extract alarm and warning flags if this message contains them
  if (this->flag_map_.count(can_id) != 0) {
    for (FlagDesc *entry : *this->flag_map_[can_id]) {
      if (data.size() >= entry->offset_ && data.size() >= entry->warn_offset_) {
        entry->alarmed_ = (data[entry->offset_] & 1 << entry->bit_no_) != 0;
        entry->warned_ = (data[entry->warn_offset_] & 1 << entry->warn_bit_no_) != 0;
        handled = true;
      }
    }
    if (handled)
      update_alarms_();
  }
  // process numeric sensors
  uint32_t now = millis();
  if (this->sensor_map_.count(can_id) != 0) {
    for (SensorDesc *sensor : *this->sensor_map_[can_id]) {
      if (sensor->last_time_ + this->throttle_ < now && data.size() >= sensor->offset_ + sensor->length_) {
        int16_t value = decode_value(data, sensor->offset_, sensor->length_);
        sensor->publish_((float) value * sensor->scale_);
        handled = true;
      }
    }
  }
  // process binary sensors
  if (this->binary_sensor_map_.count(can_id) != 0) {
    for (BinarySensorDesc *sensor : *this->binary_sensor_map_[can_id]) {
      if (sensor->last_time_ + this->throttle_ < now && data.size() >= sensor->offset_) {
        bool value = (data[sensor->offset_] & 1 << sensor->bit_no_) != 0;
        sensor->sensor_->publish_state(value);
        sensor->last_time_ = now;
        handled = true;
      }
    }
  }
  // process text sensors
  if (this->text_sensor_map_.count(can_id) != 0) {
    for (TextSensorDesc *sensor : *this->text_sensor_map_[can_id]) {
      if (sensor->last_time_ + this->throttle_ < now && !data.empty()) {
        char str[CAN_MAX_DATA_LENGTH + 1];
        size_t len = std::min(CAN_MAX_DATA_LENGTH, data.size());
        memcpy(str, &data[0], len);
        str[len] = 0;
        sensor->sensor_->publish_state(str);
        sensor->last_time_ = now;
        handled = true;
      }
    }
  }
  if (!handled && (this->debug_ || this->received_ids_.count((int) can_id) == 0)) {
    ESP_LOGW(TAG, "%s: Received unhandled id 0x%02X, len %d", this->name_, can_id, data.size());
    this->received_ids_.insert((int) can_id);
  }
}

// implement the Bms interface getters
float CanbusBmsComponent::get_voltage() { return this->get_value(CONF_VOLTAGE); }

float CanbusBmsComponent::get_current() { return this->get_value(CONF_CURRENT); }

float CanbusBmsComponent::get_charge() { return this->get_value(CONF_CHARGE); }

float CanbusBmsComponent::get_temperature() { return this->get_value(CONF_TEMPERATURE); }

float CanbusBmsComponent::get_health() { return this->get_value(CONF_HEALTH); }

float CanbusBmsComponent::get_max_voltage() { return this->get_value(CONF_MAX_CHARGE_VOLTAGE); }

float CanbusBmsComponent::get_min_voltage() { return this->get_value(CONF_MIN_DISCHARGE_VOLTAGE); }

float CanbusBmsComponent::get_max_charge_current() { return this->get_value(CONF_MAX_CHARGE_CURRENT); }

float CanbusBmsComponent::get_max_discharge_current() { return this->get_value(CONF_MAX_DISCHARGE_CURRENT); }

}  // namespace canbus_bms
}  // namespace esphome
