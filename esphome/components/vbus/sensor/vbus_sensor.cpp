#include "vbus_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.sensor");

static inline uint16_t get_u16(const std::vector<uint8_t> &message, size_t start) {
  return encode_value_le<uint16_t>(&message[start]);
}

static inline int16_t get_i16(const std::vector<uint8_t> &message, size_t start) {
  return encode_value_le<int16_t>(&message[start]);
}

// Heat quantity is reported either as three words (Wh, kWh, MWh) or as one 32 bit Wh counter.
static inline float get_heat_quantity(const std::vector<uint8_t> &message, size_t start) {
  return get_u16(message, start) + get_u16(message, start + 2) * 1000.0f + get_u16(message, start + 4) * 1000000.0f;
}

// See get_heat_quantity() above for the two encodings this component reads.
static inline float get_heat_quantity32(const std::vector<uint8_t> &message, size_t start) {
  return (static_cast<uint32_t>(get_u16(message, start + 2)) << 16) | get_u16(message, start);
}

void DeltaSolSensorBase::warn_short_message_(size_t actual, size_t expected) {
  if (this->warned_short_message_)
    return;
  this->warned_short_message_ = true;
  ESP_LOGW(TAG, "message too short: %zu < %zu", actual, expected);
}

void DeltaSolSensorBase::dump_sensors_(const LogString *model, bool single_relay) {
  ESP_LOGCONFIG(TAG, "%s:", LOG_STR_ARG(model));
  LOG_SENSOR("  ", "Temperature 1", this->temperature1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temperature2_sensor_);
  LOG_SENSOR("  ", "Temperature 3", this->temperature3_sensor_);
  LOG_SENSOR("  ", "Temperature 4", this->temperature4_sensor_);
  if (single_relay) {
    LOG_SENSOR("  ", "Pump Speed", this->pump_speed1_sensor_);
    LOG_SENSOR("  ", "Operating Hours", this->operating_hours1_sensor_);
  } else {
    LOG_SENSOR("  ", "Pump Speed 1", this->pump_speed1_sensor_);
    LOG_SENSOR("  ", "Pump Speed 2", this->pump_speed2_sensor_);
    LOG_SENSOR("  ", "Operating Hours 1", this->operating_hours1_sensor_);
    LOG_SENSOR("  ", "Operating Hours 2", this->operating_hours2_sensor_);
  }
  LOG_SENSOR("  ", "Heat Quantity", this->heat_quantity_sensor_);
  LOG_SENSOR("  ", "System Time", this->time_sensor_);
  LOG_SENSOR("  ", "FW Version", this->version_sensor_);
}

void DeltaSolCS4Sensor::dump_cs4_sensors_(const LogString *model) {
  this->dump_sensors_(model);
  LOG_SENSOR("  ", "Temperature 5", this->temperature5_sensor_);
  LOG_SENSOR("  ", "Flow Rate", this->flow_rate_sensor_);
}

void DeltaSolBSPlusSensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 28))
    this->warn_short_message_(message.size(), 28);
  if (fits(message, 0, 2))
    PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  if (fits(message, 2, 2))
    PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  if (fits(message, 4, 2))
    PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  if (fits(message, 6, 2))
    PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  if (fits(message, 8, 1))
    PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  if (fits(message, 9, 1))
    PUBLISH_STATE(this->pump_speed2_sensor_, message[9]);
  if (fits(message, 12, 2))
    PUBLISH_STATE(this->time_sensor_, get_u16(message, 12));
  if (fits(message, 16, 2))
    PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 16));
  if (fits(message, 18, 2))
    PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 18));
  if (fits(message, 20, 6))
    PUBLISH_STATE(this->heat_quantity_sensor_, get_heat_quantity(message, 20));
  if (fits(message, 26, 2))
    PUBLISH_STATE(this->version_sensor_, get_u16(message, 26) * 0.01f);
}

void DeltaSolBS2009Sensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 34))
    this->warn_short_message_(message.size(), 34);
  if (fits(message, 0, 2))
    PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  if (fits(message, 2, 2))
    PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  if (fits(message, 4, 2))
    PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  if (fits(message, 6, 2))
    PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  if (fits(message, 8, 1))
    PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  if (fits(message, 10, 2))
    PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 10));
  if (fits(message, 12, 1))
    PUBLISH_STATE(this->pump_speed2_sensor_, message[12]);
  if (fits(message, 18, 2))
    PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 18));
  if (fits(message, 22, 2))
    PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
  if (fits(message, 28, 4))
    PUBLISH_STATE(this->heat_quantity_sensor_, get_u16(message, 28) + get_u16(message, 30) * 1000.0f);
  if (fits(message, 32, 2))
    PUBLISH_STATE(this->version_sensor_, get_u16(message, 32) * 0.01f);
}

void DeltaSolCSensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 24))
    this->warn_short_message_(message.size(), 24);
  if (fits(message, 0, 2))
    PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  if (fits(message, 2, 2))
    PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  if (fits(message, 4, 2))
    PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  if (fits(message, 6, 2))
    PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  if (fits(message, 8, 1))
    PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  if (fits(message, 9, 1))
    PUBLISH_STATE(this->pump_speed2_sensor_, message[9]);
  if (fits(message, 12, 2))
    PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 12));
  if (fits(message, 14, 2))
    PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  if (fits(message, 16, 6))
    PUBLISH_STATE(this->heat_quantity_sensor_, get_heat_quantity(message, 16));
  if (fits(message, 22, 2))
    PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
}

void DeltaSolCS2Sensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 30))
    this->warn_short_message_(message.size(), 30);
  if (fits(message, 0, 2))
    PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  if (fits(message, 2, 2))
    PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  if (fits(message, 4, 2))
    PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  if (fits(message, 6, 2))
    PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  if (fits(message, 12, 1))
    PUBLISH_STATE(this->pump_speed1_sensor_, message[12]);
  if (fits(message, 14, 2))
    PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 14));
  if (fits(message, 24, 4))
    PUBLISH_STATE(this->heat_quantity_sensor_, get_heat_quantity32(message, 24));
  if (fits(message, 28, 2))
    PUBLISH_STATE(this->version_sensor_, get_u16(message, 28) * 0.01f);
}

void DeltaSolCS4Sensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 40))
    this->warn_short_message_(message.size(), 40);
  if (fits(message, 0, 2))
    PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  if (fits(message, 2, 2))
    PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  if (fits(message, 4, 2))
    PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  if (fits(message, 6, 2))
    PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  if (fits(message, 8, 1))
    PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  if (fits(message, 10, 2))
    PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 10));
  if (fits(message, 12, 1))
    PUBLISH_STATE(this->pump_speed2_sensor_, message[12]);
  if (fits(message, 14, 2))
    PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  if (fits(message, 22, 2))
    PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
  if (fits(message, 28, 4))
    PUBLISH_STATE(this->heat_quantity_sensor_, get_heat_quantity32(message, 28));
  if (fits(message, 32, 2))
    PUBLISH_STATE(this->version_sensor_, get_u16(message, 32) * 0.01f);
  if (fits(message, 36, 2))
    PUBLISH_STATE(this->temperature5_sensor_, get_i16(message, 36) * 0.1f);
  if (fits(message, 38, 2))
    PUBLISH_STATE(this->flow_rate_sensor_, get_u16(message, 38));
}

void DeltaSolBS2Sensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 26))
    this->warn_short_message_(message.size(), 26);
  if (fits(message, 0, 2))
    PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  if (fits(message, 2, 2))
    PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  if (fits(message, 4, 2))
    PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  if (fits(message, 6, 2))
    PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  if (fits(message, 8, 1))
    PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  if (fits(message, 9, 1))
    PUBLISH_STATE(this->pump_speed2_sensor_, message[9]);
  if (fits(message, 12, 2))
    PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 12));
  if (fits(message, 14, 2))
    PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  if (fits(message, 16, 6))
    PUBLISH_STATE(this->heat_quantity_sensor_, get_heat_quantity(message, 16));
  if (fits(message, 24, 2))
    PUBLISH_STATE(this->version_sensor_, get_u16(message, 24) * 0.01f);
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
  for (VBusCustomSubSensor *sensor : this->sensors_) {
    LOG_SENSOR("  ", "-", sensor);
  }
}

void VBusCustomSensor::handle_message(std::vector<uint8_t> &message) {
  for (VBusCustomSubSensor *sensor : this->sensors_)
    sensor->parse_message(message);
}

void VBusCustomSubSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace esphome::vbus
