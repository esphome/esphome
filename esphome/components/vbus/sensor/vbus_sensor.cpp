#include "vbus_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace vbus {

static const char *const TAG = "vbus.sensor";

static inline uint16_t get_u16(std::vector<uint8_t> &message, int start) {
  return (message[start + 1] << 8) + message[start];
}

static inline int16_t get_i16(std::vector<uint8_t> &message, int start) {
  return (int16_t) ((message[start + 1] << 8) + message[start]);
}

void DeltaSolBSPlusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Deltasol BS Plus:");
  LOG_SENSOR("  ", "Temperature 1", this->temperature1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temperature2_sensor_);
  LOG_SENSOR("  ", "Temperature 3", this->temperature3_sensor_);
  LOG_SENSOR("  ", "Temperature 4", this->temperature4_sensor_);
  LOG_SENSOR("  ", "Pump Speed 1", this->pump_speed1_sensor_);
  LOG_SENSOR("  ", "Pump Speed 2", this->pump_speed2_sensor_);
  LOG_SENSOR("  ", "Operating Hours 1", this->operating_hours1_sensor_);
  LOG_SENSOR("  ", "Operating Hours 2", this->operating_hours2_sensor_);
  LOG_SENSOR("  ", "Heat Quantity", this->heat_quantity_sensor_);
  LOG_SENSOR("  ", "System Time", this->time_sensor_);
  LOG_SENSOR("  ", "FW Version", this->version_sensor_);
}

void DeltaSolBSPlusSensor::handle_message(std::vector<uint8_t> &message) {
  if (this->temperature1_sensor_ != nullptr)
    this->temperature1_sensor_->publish_state(get_i16(message, 0) * 0.1f);
  if (this->temperature2_sensor_ != nullptr)
    this->temperature2_sensor_->publish_state(get_i16(message, 2) * 0.1f);
  if (this->temperature3_sensor_ != nullptr)
    this->temperature3_sensor_->publish_state(get_i16(message, 4) * 0.1f);
  if (this->temperature4_sensor_ != nullptr)
    this->temperature4_sensor_->publish_state(get_i16(message, 6) * 0.1f);
  if (this->pump_speed1_sensor_ != nullptr)
    this->pump_speed1_sensor_->publish_state(message[8]);
  if (this->pump_speed2_sensor_ != nullptr)
    this->pump_speed2_sensor_->publish_state(message[9]);
  if (this->operating_hours1_sensor_ != nullptr)
    this->operating_hours1_sensor_->publish_state(get_u16(message, 16));
  if (this->operating_hours2_sensor_ != nullptr)
    this->operating_hours2_sensor_->publish_state(get_u16(message, 18));
  if (this->heat_quantity_sensor_ != nullptr) {
    this->heat_quantity_sensor_->publish_state(get_u16(message, 20) + get_u16(message, 22) * 1000 +
                                               get_u16(message, 24) * 1000000);
  }
  if (this->time_sensor_ != nullptr)
    this->time_sensor_->publish_state(get_u16(message, 12));
  if (this->version_sensor_ != nullptr)
    this->version_sensor_->publish_state(get_u16(message, 26) * 0.01f);
}

void DeltaSolBS2009Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Deltasol BS 2009:");
  LOG_SENSOR("  ", "Temperature 1", this->temperature1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temperature2_sensor_);
  LOG_SENSOR("  ", "Temperature 3", this->temperature3_sensor_);
  LOG_SENSOR("  ", "Temperature 4", this->temperature4_sensor_);
  LOG_SENSOR("  ", "Pump Speed 1", this->pump_speed1_sensor_);
  LOG_SENSOR("  ", "Pump Speed 2", this->pump_speed2_sensor_);
  LOG_SENSOR("  ", "Operating Hours 1", this->operating_hours1_sensor_);
  LOG_SENSOR("  ", "Operating Hours 2", this->operating_hours2_sensor_);
  LOG_SENSOR("  ", "Heat Quantity", this->heat_quantity_sensor_);
  LOG_SENSOR("  ", "System Time", this->time_sensor_);
  LOG_SENSOR("  ", "FW Version", this->version_sensor_);
}

void DeltaSolBS2009Sensor::handle_message(std::vector<uint8_t> &message) {
  if (this->temperature1_sensor_ != nullptr)
    this->temperature1_sensor_->publish_state(get_i16(message, 0) * 0.1f);
  if (this->temperature2_sensor_ != nullptr)
    this->temperature2_sensor_->publish_state(get_i16(message, 2) * 0.1f);
  if (this->temperature3_sensor_ != nullptr)
    this->temperature3_sensor_->publish_state(get_i16(message, 4) * 0.1f);
  if (this->temperature4_sensor_ != nullptr)
    this->temperature4_sensor_->publish_state(get_i16(message, 6) * 0.1f);
  if (this->pump_speed1_sensor_ != nullptr)
    this->pump_speed1_sensor_->publish_state(message[8]);
  if (this->pump_speed2_sensor_ != nullptr)
    this->pump_speed2_sensor_->publish_state(message[12]);
  if (this->operating_hours1_sensor_ != nullptr)
    this->operating_hours1_sensor_->publish_state(get_u16(message, 10));
  if (this->operating_hours2_sensor_ != nullptr)
    this->operating_hours2_sensor_->publish_state(get_u16(message, 18));
  if (this->heat_quantity_sensor_ != nullptr) {
    this->heat_quantity_sensor_->publish_state(get_u16(message, 28) + get_u16(message, 30) * 1000);
  }
  if (this->time_sensor_ != nullptr)
    this->time_sensor_->publish_state(get_u16(message, 22));
  if (this->version_sensor_ != nullptr)
    this->version_sensor_->publish_state(get_u16(message, 32) * 0.01f);
}

void DeltaSolCSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Deltasol C:");
  LOG_SENSOR("  ", "Temperature 1", this->temperature1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temperature2_sensor_);
  LOG_SENSOR("  ", "Temperature 3", this->temperature3_sensor_);
  LOG_SENSOR("  ", "Temperature 4", this->temperature4_sensor_);
  LOG_SENSOR("  ", "Pump Speed 1", this->pump_speed1_sensor_);
  LOG_SENSOR("  ", "Pump Speed 2", this->pump_speed2_sensor_);
  LOG_SENSOR("  ", "Operating Hours 1", this->operating_hours1_sensor_);
  LOG_SENSOR("  ", "Operating Hours 2", this->operating_hours2_sensor_);
  LOG_SENSOR("  ", "Heat Quantity", this->heat_quantity_sensor_);
  LOG_SENSOR("  ", "System Time", this->time_sensor_);
}

void DeltaSolCSensor::handle_message(std::vector<uint8_t> &message) {
  if (this->temperature1_sensor_ != nullptr)
    this->temperature1_sensor_->publish_state(get_i16(message, 0) * 0.1f);
  if (this->temperature2_sensor_ != nullptr)
    this->temperature2_sensor_->publish_state(get_i16(message, 2) * 0.1f);
  if (this->temperature3_sensor_ != nullptr)
    this->temperature3_sensor_->publish_state(get_i16(message, 4) * 0.1f);
  if (this->temperature4_sensor_ != nullptr)
    this->temperature4_sensor_->publish_state(get_i16(message, 6) * 0.1f);
  if (this->pump_speed1_sensor_ != nullptr)
    this->pump_speed1_sensor_->publish_state(message[8]);
  if (this->pump_speed2_sensor_ != nullptr)
    this->pump_speed2_sensor_->publish_state(message[9]);
  if (this->operating_hours1_sensor_ != nullptr)
    this->operating_hours1_sensor_->publish_state(get_u16(message, 12));
  if (this->operating_hours2_sensor_ != nullptr)
    this->operating_hours2_sensor_->publish_state(get_u16(message, 14));
  if (this->heat_quantity_sensor_ != nullptr) {
    this->heat_quantity_sensor_->publish_state(get_u16(message, 16) + get_u16(message, 18) * 1000 +
                                               get_u16(message, 20) * 1000000);
  }
  if (this->time_sensor_ != nullptr)
    this->time_sensor_->publish_state(get_u16(message, 22));
}

void DeltaSolCS2Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Deltasol CS2:");
  LOG_SENSOR("  ", "Temperature 1", this->temperature1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temperature2_sensor_);
  LOG_SENSOR("  ", "Temperature 3", this->temperature3_sensor_);
  LOG_SENSOR("  ", "Temperature 4", this->temperature4_sensor_);
  LOG_SENSOR("  ", "Pump Speed", this->pump_speed_sensor_);
  LOG_SENSOR("  ", "Operating Hours", this->operating_hours_sensor_);
  LOG_SENSOR("  ", "Heat Quantity", this->heat_quantity_sensor_);
  LOG_SENSOR("  ", "FW Version", this->version_sensor_);
}

void DeltaSolCS2Sensor::handle_message(std::vector<uint8_t> &message) {
  if (this->temperature1_sensor_ != nullptr)
    this->temperature1_sensor_->publish_state(get_i16(message, 0) * 0.1f);
  if (this->temperature2_sensor_ != nullptr)
    this->temperature2_sensor_->publish_state(get_i16(message, 2) * 0.1f);
  if (this->temperature3_sensor_ != nullptr)
    this->temperature3_sensor_->publish_state(get_i16(message, 4) * 0.1f);
  if (this->temperature4_sensor_ != nullptr)
    this->temperature4_sensor_->publish_state(get_i16(message, 6) * 0.1f);
  if (this->pump_speed_sensor_ != nullptr)
    this->pump_speed_sensor_->publish_state(message[12]);
  if (this->operating_hours_sensor_ != nullptr)
    this->operating_hours_sensor_->publish_state(get_u16(message, 14));
  if (this->heat_quantity_sensor_ != nullptr)
    this->heat_quantity_sensor_->publish_state((get_u16(message, 26) << 16) + get_u16(message, 24));
  if (this->version_sensor_ != nullptr)
    this->version_sensor_->publish_state(get_u16(message, 28) * 0.01f);
}

void DeltaSolCSPlusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Deltasol CS Plus:");
  LOG_SENSOR("  ", "Temperature 1", this->temperature1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temperature2_sensor_);
  LOG_SENSOR("  ", "Temperature 3", this->temperature3_sensor_);
  LOG_SENSOR("  ", "Temperature 4", this->temperature4_sensor_);
  LOG_SENSOR("  ", "Temperature 5", this->temperature5_sensor_);
  LOG_SENSOR("  ", "Pump Speed 1", this->pump_speed1_sensor_);
  LOG_SENSOR("  ", "Pump Speed 2", this->pump_speed2_sensor_);
  LOG_SENSOR("  ", "Operating Hours 1", this->operating_hours1_sensor_);
  LOG_SENSOR("  ", "Operating Hours 2", this->operating_hours2_sensor_);
  LOG_SENSOR("  ", "Heat Quantity", this->heat_quantity_sensor_);
  LOG_SENSOR("  ", "System Time", this->time_sensor_);
  LOG_SENSOR("  ", "FW Version", this->version_sensor_);
  LOG_SENSOR("  ", "Flow Rate", this->flow_rate_sensor_);
}

void DeltaSolCSPlusSensor::handle_message(std::vector<uint8_t> &message) {
  if (this->temperature1_sensor_ != nullptr)
    this->temperature1_sensor_->publish_state(get_i16(message, 0) * 0.1f);
  if (this->temperature2_sensor_ != nullptr)
    this->temperature2_sensor_->publish_state(get_i16(message, 2) * 0.1f);
  if (this->temperature3_sensor_ != nullptr)
    this->temperature3_sensor_->publish_state(get_i16(message, 4) * 0.1f);
  if (this->temperature4_sensor_ != nullptr)
    this->temperature4_sensor_->publish_state(get_i16(message, 6) * 0.1f);
  if (this->temperature5_sensor_ != nullptr)
    this->temperature5_sensor_->publish_state(get_i16(message, 36) * 0.1f);
  if (this->pump_speed1_sensor_ != nullptr)
    this->pump_speed1_sensor_->publish_state(message[8]);
  if (this->pump_speed2_sensor_ != nullptr)
    this->pump_speed2_sensor_->publish_state(message[12]);
  if (this->operating_hours1_sensor_ != nullptr)
    this->operating_hours1_sensor_->publish_state(get_u16(message, 10));
  if (this->operating_hours2_sensor_ != nullptr)
    this->operating_hours2_sensor_->publish_state(get_u16(message, 14));
  if (this->heat_quantity_sensor_ != nullptr)
    this->heat_quantity_sensor_->publish_state((get_u16(message, 30) << 16) + get_u16(message, 28));
  if (this->time_sensor_ != nullptr)
    this->time_sensor_->publish_state(get_u16(message, 22));
  if (this->version_sensor_ != nullptr)
    this->version_sensor_->publish_state(get_u16(message, 32) * 0.01f);
  if (this->flow_rate_sensor_ != nullptr)
    this->flow_rate_sensor_->publish_state(get_u16(message, 38));
}

void VBusCustomSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "VBus Custom Sensor:");
  if (this->source_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Source address: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Source address: 0x%04x", this->source_);
  }
  if (this->dest_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Dest address: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Dest address: 0x%04x", this->dest_);
  }
  if (this->command_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Command: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Command: 0x%04x", this->command_);
  }
  ESP_LOGCONFIG(TAG, "  Sensors:");
  for (VBusCustomSubSensor *sensor : this->sensors_)
    LOG_SENSOR("  ", "-", sensor);
}

void VBusCustomSensor::handle_message(std::vector<uint8_t> &message) {
  for (VBusCustomSubSensor *sensor : this->sensors_)
    sensor->parse_message(message);
}

void VBusCustomSubSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace vbus
}  // namespace esphome
