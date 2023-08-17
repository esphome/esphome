#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"

#include "climate_mitsubishi.h"
#include "mitsubishi_protocol.h"

using namespace esphome::climate;

namespace esphome {
namespace climate_mitsubishi {

static const char *const TAG = "mitsubishi.climate";

ClimateMitsubishi::ClimateMitsubishi()
  :   compressor_frequency_sensor_(nullptr),
    fan_velocity_sensor_(nullptr),
    conflicted_sensor_(nullptr),
    preheat_sensor_(nullptr),
    control_temperature_sensor_(nullptr),
    inject_enable_switch_(nullptr),
    remote_temperature_number_(nullptr),
    vertical_airflow_select_(nullptr),
    high_precision_temp_setting(false),
    last_status_request_(0),
    last_settings_request_(0), 
    status_rotation_(0), 
    temperature_offset_(0) {
  this->traits_ = climate::ClimateTraits();
  this->traits_.set_supported_modes({climate::CLIMATE_MODE_OFF,
                                     climate::CLIMATE_MODE_COOL,
                                     climate::CLIMATE_MODE_HEAT,
                                     climate::CLIMATE_MODE_FAN_ONLY,
                                     climate::CLIMATE_MODE_DRY,
                                     climate::CLIMATE_MODE_HEAT_COOL});
  this->traits_.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_QUIET
  });
  this->traits_.set_supported_custom_fan_modes({"1",
                                                "2",
                                                "3",
                                                "4"});
  this->traits_.set_supports_current_temperature(true);
  this->traits_.set_supports_action(true);
  this->traits_.set_visual_current_temperature_step(0.1);
  this->traits_.set_visual_target_temperature_step(1);
}

ClimateTraits ClimateMitsubishi::traits() { return traits_; }

ClimateMode ClimateMitsubishi::mode_to_climate_mode(uint8_t mode) {
  switch (mode)
  {
  case (uint8_t)mitsubishi_protocol::Mode::HEAT:
    return ClimateMode::CLIMATE_MODE_HEAT;
    break;
  case (uint8_t)mitsubishi_protocol::Mode::COOL:
    return ClimateMode::CLIMATE_MODE_COOL;
    break;
  case (uint8_t)mitsubishi_protocol::Mode::DRY:
    return ClimateMode::CLIMATE_MODE_DRY;
    break;
  case (uint8_t)mitsubishi_protocol::Mode::FAN:
    return ClimateMode::CLIMATE_MODE_FAN_ONLY;
    break;
  case (uint8_t)mitsubishi_protocol::Mode::AUTO:
    return ClimateMode::CLIMATE_MODE_HEAT_COOL;
    break;
  default:
    return ClimateMode::CLIMATE_MODE_OFF;
    break;
  }
}

std::string ClimateMitsubishi::fan_to_custom_fan_mode(uint8_t fan) {
  switch (fan)
  {
  case (uint8_t)mitsubishi_protocol::FanMode::AUTO:
    return "Auto";
    break;
  case (uint8_t)mitsubishi_protocol::FanMode::FAN_QUIET:
    return "Quiet";
    break;
  case (uint8_t)mitsubishi_protocol::FanMode::FAN_1:
    return "1";
    break;
  case (uint8_t)mitsubishi_protocol::FanMode::FAN_2:
    return "2";
    break;
  case (uint8_t)mitsubishi_protocol::FanMode::FAN_3:
    return "3";
    break;
  case (uint8_t)mitsubishi_protocol::FanMode::FAN_4:
    return "4";
    break;
  default:
    return "Unknown";
    break;
  }
}

std::string ClimateMitsubishi::vertical_vane_to_vertical_airflow_select(uint8_t vertical_vane) {
  switch (vertical_vane)
  {
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_AUTO:
    return "Auto";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_1:
    return "1";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_2:
    return "2";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_3:
    return "3";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_4:
    return "4";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_5:
    return "5";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_SWING:
    return "Swing";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_EXPERIMENTAL_6:
    return "Experimental 6";
  case (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_EXPERIMENTAL_8:
    return "Experimental 8";
  default:
    return "Auto";
    break;
  }
}

uint8_t ClimateMitsubishi::climate_mode_to_mode(ClimateMode mode) {
  switch (mode)
  {
  case CLIMATE_MODE_HEAT:
    return (uint8_t)mitsubishi_protocol::Mode::HEAT;
  case CLIMATE_MODE_COOL:
    return (uint8_t)mitsubishi_protocol::Mode::COOL;
  case CLIMATE_MODE_DRY:
    return (uint8_t)mitsubishi_protocol::Mode::DRY;
  case CLIMATE_MODE_FAN_ONLY:
    return (uint8_t)mitsubishi_protocol::Mode::FAN;
  case CLIMATE_MODE_HEAT_COOL:
    return CLIMATE_MODE_HEAT_COOL;
  default:
    return CLIMATE_MODE_OFF;
  }
}

uint8_t ClimateMitsubishi::custom_fan_mode_to_fan(std::string fan_mode) {
  if(str_equals_case_insensitive("Auto", fan_mode)) {
    return (uint8_t)mitsubishi_protocol::FanMode::AUTO;
  } else if (str_equals_case_insensitive("Quiet", fan_mode)) {
    return (uint8_t)mitsubishi_protocol::FanMode::FAN_QUIET;
  } else if (str_equals_case_insensitive("1", fan_mode)) {
    return (uint8_t)mitsubishi_protocol::FanMode::FAN_1;
  } else if (str_equals_case_insensitive("2", fan_mode)) {
    return (uint8_t)mitsubishi_protocol::FanMode::FAN_2;
  } else if (str_equals_case_insensitive("3", fan_mode)) {
    return (uint8_t)mitsubishi_protocol::FanMode::FAN_3;
  } else if (str_equals_case_insensitive("4", fan_mode)) {
    return (uint8_t)mitsubishi_protocol::FanMode::FAN_4;
  } else {
    return (uint8_t)mitsubishi_protocol::FanMode::AUTO;
  }
}

uint8_t fan_mode_to_fan(climate::ClimateFanMode fan_mode) {
  if(fan_mode == climate::CLIMATE_FAN_AUTO) {
    return (uint8_t)mitsubishi_protocol::FanMode::AUTO;
  } else if(fan_mode == climate::CLIMATE_FAN_QUIET) {
    return (uint8_t)mitsubishi_protocol::FanMode::FAN_QUIET;
  } else {
    return (uint8_t)mitsubishi_protocol::FanMode::AUTO;
  }
}

uint8_t ClimateMitsubishi::vertical_airflow_select_to_vertical_vane(std::string swing_mode) {
  if(str_equals_case_insensitive("Auto", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_AUTO;
  } else if(str_equals_case_insensitive("1", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_1;
  } else if(str_equals_case_insensitive("2", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_2;
  } else if(str_equals_case_insensitive("3", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_3;
  } else if(str_equals_case_insensitive("4", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_4;
  } else if(str_equals_case_insensitive("5", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_5;
  } else if(str_equals_case_insensitive("Swing", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_SWING;
  } else if(str_equals_case_insensitive("Experimental 6", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_EXPERIMENTAL_6;
  } else if(str_equals_case_insensitive("Experimental 8", swing_mode)) {
    return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_EXPERIMENTAL_8;
  }
  return (uint8_t)mitsubishi_protocol::VerticalVaneMode::VANE_AUTO;
}

float ClimateMitsubishi::temp_05_to_celsius(uint8_t temp) {
  temp -= 128;
  return (float)temp / 2;
}
float ClimateMitsubishi::room_temp_to_celsius(uint8_t temp) {
  return (float)temp+10.0;
}

float ClimateMitsubishi::setting_temp_to_celsius(uint8_t temp) {
  return 31.0 - (float)temp;
}

uint8_t ClimateMitsubishi::celsius_to_temp_05(float celsius) {
  return ((int)(celsius*2)) + 128;
}

uint8_t ClimateMitsubishi::celsius_to_setting_temp(float celsius) {
  return (int)celsius - 31;
}

void ClimateMitsubishi::setup() {
  //flush read buffer
  this->read_array(nullptr, available());
  //send setup packet
  ESP_LOGD(TAG, "writing connect packet");
  this->write_array(mitsubishi_protocol::connect_packet, mitsubishi_protocol::connect_len);
  this->flush();
  
  ESP_LOGD(TAG, "reading response");
  if(read_packet() != PacketType::connect_success) {
    ESP_LOGE(TAG, "Invalid response to connect request received");
    this->connected_ = false;
    return;
  }
  ESP_LOGD(TAG, "valid response");
  this->connected_ = true;
  request_info((uint8_t)mitsubishi_protocol::InfoType::SENSORS);
  read_packet();
  this->read_array(nullptr, available());
}

void ClimateMitsubishi::loop() {
  if(!this->connected_ && millis() > last_connect_attempt_+3000) {
    this->setup();
    last_connect_attempt_ = millis();
  }
  while(available() != 0) {
    read_packet();
  }
  if(millis() > last_status_request_+400) {
    switch(status_rotation_) {
      case 0:
        request_info((uint8_t)mitsubishi_protocol::InfoType::STATUS);
        read_packet();
        break;
      case 1:
        request_info((uint8_t)mitsubishi_protocol::InfoType::SENSORS);
        read_packet();
        break;
      case 2:
        request_info((uint8_t)mitsubishi_protocol::InfoType::ROOM_TEMP);
        read_packet();
        break;
    }
    status_rotation_++;
    if(status_rotation_ > 3) status_rotation_=0;
    last_status_request_ = millis();
  }

  if(millis() > last_settings_request_+5000) {
    request_info((uint8_t)mitsubishi_protocol::InfoType::SETTINGS);
    read_packet();
    last_settings_request_ = 0;
  }
}

void ClimateMitsubishi::control(const esphome::climate::ClimateCall &call) {
  uint8_t packet[mitsubishi_protocol::packet_len];
  memset(packet, 0, mitsubishi_protocol::packet_len);
  memcpy(packet, mitsubishi_protocol::set_request_header, mitsubishi_protocol::set_request_header_len);

  ESP_LOGD(TAG, "got control request");

  if(call.get_mode().has_value()) {
    //packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::POWER;
    if(call.get_mode() == ClimateMode::CLIMATE_MODE_OFF) {
      ESP_LOGD(TAG, "powering off");
      packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::POWER;
      packet[(int)mitsubishi_protocol::Offset::POWER] = (uint8_t)mitsubishi_protocol::Power::OFF;
    } else {
      if(!power) {
        ESP_LOGD(TAG, "powering on");
        packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::POWER;
        packet[(int)mitsubishi_protocol::Offset::POWER] = (uint8_t)mitsubishi_protocol::Power::ON;
      }
      packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::MODE;
      packet[(int)mitsubishi_protocol::Offset::MODE] = climate_mode_to_mode(call.get_mode().value());
    }
  }
  if(call.get_target_temperature().has_value()) {
    packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::TARGET_TEMP;
    if(high_precision_temp_setting) {
      packet[(int)mitsubishi_protocol::Offset::TARGET_TEMP_SET_05] = celsius_to_temp_05(call.get_target_temperature().value());
    } else {
      packet[(int)mitsubishi_protocol::Offset::TARGET_TEMP] = celsius_to_setting_temp(call.get_target_temperature().value());
    }
  }
  if(call.get_custom_fan_mode().has_value()) {
    packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::FAN;
    packet[(int)mitsubishi_protocol::Offset::FAN] = custom_fan_mode_to_fan(call.get_custom_fan_mode().value());
  } else if(call.get_fan_mode().has_value()) {
    packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::FAN;
    packet[(int)mitsubishi_protocol::Offset::FAN] = fan_mode_to_fan(call.get_fan_mode().value());
  }
  packet[mitsubishi_protocol::packet_len - 1] = checksum(packet, mitsubishi_protocol::packet_len - 1);
  write_array(packet, mitsubishi_protocol::packet_len );
}

void ClimateMitsubishi::set_vertical_airflow_direction(const std::string &direction) {
  uint8_t packet[mitsubishi_protocol::packet_len];
  memset(packet, 0, mitsubishi_protocol::packet_len);
  memcpy(packet, mitsubishi_protocol::set_request_header, mitsubishi_protocol::set_request_header_len);

  packet[(int)mitsubishi_protocol::Offset::SETTING_MASK_1] += (uint8_t)mitsubishi_protocol::SettingsMask1::VERTICAL_VANE;
  packet[(int)mitsubishi_protocol::Offset::VERTICAL_VANE] = this->vertical_airflow_select_to_vertical_vane(direction);

  packet[mitsubishi_protocol::packet_len - 1] = checksum(packet, mitsubishi_protocol::packet_len - 1);
  write_array(packet, mitsubishi_protocol::packet_len );
}

void ClimateMitsubishi::set_temperature_offset(float offset) {
  this->temperature_offset_ = offset;
}

void ClimateMitsubishi::inject_temperature(float temperature) {
  if(!this->inject_enable_) {
    if(this->remote_temperature_number_ != nullptr) {
      this->remote_temperature_number_->publish_state(this->current_temperature);
    }
    return;
  }

  this->current_temperature = temperature;

  temperature += this->temperature_offset_;
  uint8_t packet[mitsubishi_protocol::packet_len];
  memcpy(packet, mitsubishi_protocol::temperature_inject_header, mitsubishi_protocol::temperature_inject_header_len);

  packet[(int)mitsubishi_protocol::Offset::TEMPERATURE_INJECT_ENABLE] = true;

  //round to 0.5 increments
  temperature = temperature * 2;
  temperature = (float)(int)temperature;
  temperature = temperature / 2;

  packet[(int)mitsubishi_protocol::Offset::TEMPERATURE_INJECT_TEMP_1] = (uint8_t)(3+ ((temperature - 10) *2));
  packet[(int)mitsubishi_protocol::Offset::TEMPERATURE_INJECT_TEMP_2] = celsius_to_temp_05(temperature);

  packet[mitsubishi_protocol::packet_len-1] = checksum(packet, mitsubishi_protocol::packet_len-1);
  write_array(packet, mitsubishi_protocol::packet_len);
}

void ClimateMitsubishi::disable_injection() {
  uint8_t packet[mitsubishi_protocol::packet_len];
  memcpy(packet, mitsubishi_protocol::temperature_inject_header, mitsubishi_protocol::temperature_inject_header_len);

  packet[(int)mitsubishi_protocol::Offset::TEMPERATURE_INJECT_ENABLE] = false;


  packet[(int)mitsubishi_protocol::Offset::TEMPERATURE_INJECT_TEMP_1] = 0;
  packet[(int)mitsubishi_protocol::Offset::TEMPERATURE_INJECT_TEMP_2] = 0x80;

  packet[mitsubishi_protocol::packet_len-1] = checksum(packet, mitsubishi_protocol::packet_len-1);
  write_array(packet, mitsubishi_protocol::packet_len);
}

void ClimateMitsubishi::request_info(uint8_t type) {
  ESP_LOGD(TAG, "requesting info");
  uint8_t packet[mitsubishi_protocol::packet_len];
  memcpy(packet, mitsubishi_protocol::info_header, mitsubishi_protocol::info_header_len);

  packet[(int)mitsubishi_protocol::Offset::INFO_TYPE] = type;

  for(int i=6; i<21; i++) {
    packet[i] = 0x00;
  }

  packet[mitsubishi_protocol::packet_len-1] = checksum(packet, mitsubishi_protocol::packet_len-1);
  write_array(packet, mitsubishi_protocol::packet_len);
}

PacketType ClimateMitsubishi::read_packet() {
  ESP_LOGD(TAG, "parsing packet");
  uint8_t packet[mitsubishi_protocol::packet_len + mitsubishi_protocol::info_header_len];
  uint8_t data_length;

  int delay_count = 0;
  while(available() == 0) {
    delay(50);
    delay_count++;
    if(delay_count > 10) {
      ESP_LOGE(TAG, "timed out waiting for packet");
      return PacketType::no_response;
    }
  }

  //seek for first header byte
  bool found_first_byte = false;
  while(available() > 0) {
    packet[0] = read();
    ESP_LOGD(TAG, "attempting read first byte: %i", packet[0]);
    if(packet[0] == mitsubishi_protocol::info_header[0]) {
      ESP_LOGD(TAG, "found first byte");
      found_first_byte = true;
      break;
    }
  }
  if(!found_first_byte) {
    ESP_LOGD(TAG, "did not find first byte: %i", packet[0]);
    return PacketType::no_response;
  }

  read_array(&packet[1], mitsubishi_protocol::info_header_len - 1);

  if(packet[0] != mitsubishi_protocol::info_header[0] || packet[2] != mitsubishi_protocol::info_header[2] || packet[3] != mitsubishi_protocol::info_header[3]) {
    ESP_LOGD(TAG, "invalid header");
    return PacketType::invalid;
  }

  data_length = packet[4];
  read_array(&packet[5], data_length);

  packet[data_length+mitsubishi_protocol::info_header_len] = read();

  if(packet[data_length+mitsubishi_protocol::info_header_len] != checksum(packet, data_length+mitsubishi_protocol::info_header_len)) {
    ESP_LOGD(TAG, "checksum invalid");
  }

  switch(packet[1]) {
    case 0x7a: //connect success
      ESP_LOGD(TAG, "connect success");
      return PacketType::connect_success;
      break;
    case 0x61: //set success
      ESP_LOGD(TAG, "set success");
      request_info((uint8_t)mitsubishi_protocol::InfoType::SETTINGS);
      read_packet();
      return PacketType::set_success;
      break;
    case 0x62: //info packet
      ESP_LOGD(TAG, "got info packet");
      switch(packet[(int)mitsubishi_protocol::Offset::INFO_TYPE]) {
        case (uint8_t)mitsubishi_protocol::InfoType::SETTINGS:
          ESP_LOGD(TAG, "got settings info");
          this->power = packet[(int)mitsubishi_protocol::Offset::POWER];
          if(packet[(int)mitsubishi_protocol::Offset::POWER] == (uint8_t)mitsubishi_protocol::Power::OFF) {
            this->mode = ClimateMode::CLIMATE_MODE_OFF;
          } else {
            this->mode = mode_to_climate_mode(packet[(int)mitsubishi_protocol::Offset::MODE]);
          }
          if(packet[(int)mitsubishi_protocol::Offset::FAN] ==(uint8_t)mitsubishi_protocol::FanMode::AUTO) {
            this->fan_mode = climate::CLIMATE_FAN_AUTO;
            this->custom_fan_mode.reset();
          } else if(packet[(int)mitsubishi_protocol::Offset::FAN] ==(uint8_t)mitsubishi_protocol::FanMode::FAN_QUIET) {
            this->fan_mode = climate::CLIMATE_FAN_QUIET;
            this->custom_fan_mode.reset();
          } else {
            this->fan_mode.reset();
            this->custom_fan_mode = fan_to_custom_fan_mode(packet[(int)mitsubishi_protocol::Offset::FAN]);
          }
          if(this->vertical_airflow_select_ != nullptr) {
            this->vertical_airflow_select_->publish_state(vertical_vane_to_vertical_airflow_select(packet[(int)mitsubishi_protocol::Offset::VERTICAL_VANE]));
          }

          if(packet[(int)mitsubishi_protocol::Offset::TARGET_TEMP_GET_05] != 0) {
            this->high_precision_temp_setting = true;
            this->traits_.set_visual_target_temperature_step(0.5);
            this->target_temperature = temp_05_to_celsius(packet[(int)mitsubishi_protocol::Offset::TARGET_TEMP_GET_05]);
          } else {
            this->target_temperature = setting_temp_to_celsius(packet[(int)mitsubishi_protocol::Offset::TARGET_TEMP]);
          }

          this->publish_state();
          return PacketType::settings;
          break;
        case (uint8_t)mitsubishi_protocol::InfoType::ROOM_TEMP:
          ESP_LOGD(TAG, "got room temp info");
          float temperature;
          if(packet[(int)mitsubishi_protocol::Offset::ROOM_TEMP_05] != 0) {
            temperature = temp_05_to_celsius(packet[(int)mitsubishi_protocol::Offset::ROOM_TEMP_05]);
          } else {
            temperature = room_temp_to_celsius(packet[(int)mitsubishi_protocol::Offset::ROOM_TEMP]);
          }
          if(!this->inject_enable_) {
            this->current_temperature = temperature;
            if(this->remote_temperature_number_ != nullptr) {
              this->remote_temperature_number_->publish_state(temperature);
            }
            this->publish_state();
          }
          if(this->control_temperature_sensor_ != nullptr) {
            this->control_temperature_sensor_->publish_state(temperature);
          }
          return PacketType::room_temp;
          break;
        case (uint8_t)mitsubishi_protocol::InfoType::STATUS:
          ESP_LOGD(TAG, "got status info");
          if(this->compressor_frequency_sensor_ != nullptr) {
            this->compressor_frequency_sensor_->publish_state((float)packet[(int)mitsubishi_protocol::Offset::COMPRESSOR_FREQUENCY]);
          }
          if(this->mode == ClimateMode::CLIMATE_MODE_OFF) {
            this->action = ClimateAction::CLIMATE_ACTION_OFF;
          } else if(packet[(int)mitsubishi_protocol::Offset::OPERATING] == 0) {
            this->action = ClimateAction::CLIMATE_ACTION_IDLE;
          } else if(this->mode == ClimateMode::CLIMATE_MODE_COOL) {
            this->action = ClimateAction::CLIMATE_ACTION_COOLING;
          } else if(this->mode == ClimateMode::CLIMATE_MODE_DRY) {
            this->action = ClimateAction::CLIMATE_ACTION_DRYING;
          } else if(this->mode == ClimateMode::CLIMATE_MODE_HEAT) {
            this->action = ClimateAction::CLIMATE_ACTION_HEATING;
          } else if(this->mode == ClimateMode::CLIMATE_MODE_FAN_ONLY) {
            this->action = ClimateAction::CLIMATE_ACTION_FAN;
          } else if(this->mode == ClimateMode::CLIMATE_MODE_HEAT_COOL) {
            if(this->current_temperature > this->target_temperature) {
              this->action = ClimateAction::CLIMATE_ACTION_COOLING;
            } else {
              this->action = ClimateAction::CLIMATE_ACTION_HEATING;
            }
          }
          this->publish_state();
          return PacketType::status;
          break;
        case (uint8_t)mitsubishi_protocol::InfoType::SENSORS:
          ESP_LOGD(TAG, "got room sensors info");
          if(this->preheat_sensor_ != nullptr) {
            this->preheat_sensor_->publish_state(packet[(int)mitsubishi_protocol::Offset::LOOP_FLAGS] & (uint8_t)mitsubishi_protocol::LoopFlagsMask::PREHEAT);
          }
          if(this->conflicted_sensor_ != nullptr) {
            this->conflicted_sensor_->publish_state(packet[(int)mitsubishi_protocol::Offset::LOOP_FLAGS] & (uint8_t)mitsubishi_protocol::LoopFlagsMask::CONFLICT);
          }
          if(this->fan_velocity_sensor_ != nullptr) {
            this->fan_velocity_sensor_->publish_state((float)packet[(int)mitsubishi_protocol::Offset::FAN_VELOCITY]);
          }
          return PacketType::sensors;
          break;
        default:
          ESP_LOGD(TAG, "got unknown info: %i", packet[(int)mitsubishi_protocol::Offset::INFO_TYPE]);
          return PacketType::unknown;
          break;
      }
      break;
    default:
      ESP_LOGD(TAG, "unknown packet type");
      return PacketType::unknown;
      break;
  }
  return PacketType::unknown;
}

uint8_t ClimateMitsubishi::checksum(uint8_t *packet, size_t len) {
  uint8_t sum = 0;
  for(int i=0; i<len; i++) {
    sum+=packet[i];
  }
  return (0xfc - sum);
}

void ClimateMitsubishi::set_compressor_frequency_sensor(esphome::sensor::Sensor *sensor) {
  this->compressor_frequency_sensor_ = sensor;
}

void ClimateMitsubishi::set_fan_velocity_sensor(esphome::sensor::Sensor *sensor) {
  this->fan_velocity_sensor_ = sensor;
}

void ClimateMitsubishi::set_preheat_sensor(esphome::binary_sensor::BinarySensor *sensor) {
  this->preheat_sensor_ = sensor;
}

void ClimateMitsubishi::set_conflicted_sensor(esphome::binary_sensor::BinarySensor *sensor) {
  this->conflicted_sensor_ = sensor;
}

void ClimateMitsubishi::set_control_temperature_sensor(esphome::sensor::Sensor *sensor) {
  this->control_temperature_sensor_ = sensor;
}

void ClimateMitsubishi::set_remote_temperature_number(esphome::number::Number *number) {
  this->remote_temperature_number_ = number;
}

void ClimateMitsubishi::set_vertical_airflow_select(esphome::select::Select *select) {
  this->vertical_airflow_select_ = select;
}

void ClimateMitsubishi::set_inject_enable(bool enable) {
  inject_enable_ = enable;
  if(!enable) {
    ESP_LOGI(TAG, "using internal temperature");
    this->disable_injection();
  } else {
    ESP_LOGI(TAG, "enabling temperature injection");
    this->inject_temperature(this->current_temperature);
    this->last_control_temperature_ = this->current_temperature;
  }
}

void ClimateMitsubishiInjectEnableSwitch::write_state(bool state) {
  ESP_LOGD(TAG, "set inject enable state to %i", state);
  this->climate_->set_inject_enable(state);
  this->publish_state(state);
}

void ClimateMitsubishiRemoteTemperatureNumber::control(float value) {
  this->climate_->inject_temperature(value);
}

void ClimateMitsubishiTemperatureOffsetNumber::control(float value) {
  this->climate_->set_temperature_offset(value);
  this->publish_state(value);
}

ClimateMitsubishiTemperatureOffsetNumber::ClimateMitsubishiTemperatureOffsetNumber() {
  this->publish_state(0);
}

void ClimateMitsubishiVerticalAirflowSelect::control(const std::string &value) {
  this->climate_->set_vertical_airflow_direction(value);
}

void ClimateMitsubishiInjectEnableSwitch::set_climate(ClimateMitsubishi *climate) {
  this->climate_ = climate;
}

void ClimateMitsubishiRemoteTemperatureNumber::set_climate(ClimateMitsubishi *climate) {
  this->climate_ = climate;
}

void ClimateMitsubishiTemperatureOffsetNumber::set_climate(ClimateMitsubishi *climate) {
  this->climate_ = climate;
}

void ClimateMitsubishiVerticalAirflowSelect::set_climate(ClimateMitsubishi *climate) {
  this->climate_ = climate;
}

}
}