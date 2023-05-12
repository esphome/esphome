// ENS160 sensor with I2C interface from ScioSense
//
// Datasheet: https://www.sciosense.com/wp-content/uploads/documents/SC-001224-DS-7-ENS160-Datasheet.pdf
//
// Implementation based on:
//   https://github.com/sciosense/ENS160_driver

#include "ens160.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ens160 {

static const char *const TAG = "ens160";

static const uint8_t ENS160_BOOTING = 10;

static const uint16_t ENS160_PART_ID = 0x0160;

static const uint8_t ENS160_REG_PART_ID = 0x00;
static const uint8_t ENS160_REG_OPMODE = 0x10;
static const uint8_t ENS160_REG_CONFIG = 0x11;
static const uint8_t ENS160_REG_COMMAND = 0x12;
static const uint8_t ENS160_REG_TEMP_IN = 0x13;
static const uint8_t ENS160_REG_RH_IN = 0x15;
static const uint8_t ENS160_REG_DATA_STATUS = 0x20;
static const uint8_t ENS160_REG_DATA_AQI = 0x21;
static const uint8_t ENS160_REG_DATA_TVOC = 0x22;
static const uint8_t ENS160_REG_DATA_ECO2 = 0x24;
static const uint8_t ENS160_REG_DATA_BL = 0x28;
static const uint8_t ENS160_REG_DATA_T = 0x30;
static const uint8_t ENS160_REG_DATA_RH = 0x32;
static const uint8_t ENS160_REG_DATA_MISR = 0x38;
static const uint8_t ENS160_REG_GPR_WRITE_0 = 0x40;
static const uint8_t ENS160_REG_GPR_WRITE_1 = ENS160_REG_GPR_WRITE_0 + 1;
static const uint8_t ENS160_REG_GPR_WRITE_2 = ENS160_REG_GPR_WRITE_0 + 2;
static const uint8_t ENS160_REG_GPR_WRITE_3 = ENS160_REG_GPR_WRITE_0 + 3;
static const uint8_t ENS160_REG_GPR_WRITE_4 = ENS160_REG_GPR_WRITE_0 + 4;
static const uint8_t ENS160_REG_GPR_WRITE_5 = ENS160_REG_GPR_WRITE_0 + 5;
static const uint8_t ENS160_REG_GPR_WRITE_6 = ENS160_REG_GPR_WRITE_0 + 6;
static const uint8_t ENS160_REG_GPR_WRITE_7 = ENS160_REG_GPR_WRITE_0 + 7;
static const uint8_t ENS160_REG_GPR_READ_0 = 0x48;
static const uint8_t ENS160_REG_GPR_READ_4 = ENS160_REG_GPR_READ_0 + 4;
static const uint8_t ENS160_REG_GPR_READ_6 = ENS160_REG_GPR_READ_0 + 6;
static const uint8_t ENS160_REG_GPR_READ_7 = ENS160_REG_GPR_READ_0 + 7;

static const uint8_t ENS160_COMMAND_NOP = 0x00;
static const uint8_t ENS160_COMMAND_CLRGPR = 0xCC;
static const uint8_t ENS160_COMMAND_GET_APPVER = 0x0E;

static const uint8_t ENS160_OPMODE_RESET = 0xF0;
static const uint8_t ENS160_OPMODE_IDLE = 0x01;
static const uint8_t ENS160_OPMODE_STD = 0x02;

static const uint8_t ENS160_DATA_STATUS_STATAS = 0x80;
static const uint8_t ENS160_DATA_STATUS_STATER = 0x40;
static const uint8_t ENS160_DATA_STATUS_RESERVED_B = 0x20;
static const uint8_t ENS160_DATA_STATUS_RESERVED_A = 0x10;
static const uint8_t ENS160_DATA_STATUS_VALID_B = 0x08;
static const uint8_t ENS160_DATA_STATUS_VALID_A = 0x04;
static const uint8_t ENS160_DATA_STATUS_NEWDAT = 0x02;
static const uint8_t ENS160_DATA_STATUS_NEWGPR = 0x01;

// helps extract operating state from status register 
static const uint8_t ENS160_VALIDITY_FLAG_MASK = 0x0C;

// helps remove reserved bits in aqi data register
static const uint8_t ENS160_DATA_AQI_MASK = 0x07;


void ENS160Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ENS160...");

  // check part_id
  uint16_t part_id;
  if (this->read_register(ENS160_REG_PART_ID, reinterpret_cast<uint8_t *>(&part_id), 2) != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return; 	
  }
  
  if (part_id != ENS160_PART_ID) {
    this->error_code_ = INVALID_ID;
    this->mark_failed();
    return; 	 
  }

  // set mode to reset
  if (!this->write_byte(ENS160_REG_OPMODE, ENS160_OPMODE_RESET)){
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return; 
  }
  delay(ENS160_BOOTING);
  
  // check status
  uint8_t status;
  if (!this->read_byte(ENS160_REG_DATA_STATUS,&status)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return; 
  }
  this->validity_flag_ = static_cast<ValidityFlag>((status & ENS160_VALIDITY_FLAG_MASK) >> 2);
   
  if (this->validity_flag_ == INVALID_OUTPUT) {
    this->error_code_ = VALIDITY_INVALID_OUTPUT;
    this->mark_failed();
    return; 
  }
  
  // set mode to idle
  if (!this->write_byte(ENS160_REG_OPMODE, ENS160_OPMODE_IDLE)){
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  // clear command
  if (!this->write_byte(ENS160_REG_COMMAND, ENS160_COMMAND_NOP)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  
  if (!this->write_byte(ENS160_REG_COMMAND, ENS160_COMMAND_CLRGPR)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  delay(ENS160_BOOTING);
  
  // read firmware version
  if (!this->write_byte(ENS160_REG_COMMAND, ENS160_COMMAND_GET_APPVER)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  delay(ENS160_BOOTING);
  
  auto version_data = this->read_bytes<3>(ENS160_REG_GPR_READ_4);
  if (version_data.has_value()) {
    this->firmware_ver_major_ = (*version_data)[0];
    this->firmware_ver_minor_ = (*version_data)[1];
    this->firmware_ver_build_ = (*version_data)[2];
    //sprintf(this->firmware_version_, "major:%d minor:%d build:%d", fw_ver_major, fw_ver_minor, fw_ver_build);
  }
  //else sprintf(this->firmware_version_, "not available");
  
  // set mode to standard
  if (!this->write_byte(ENS160_REG_OPMODE, ENS160_OPMODE_STD)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return; 
  }
  
  // read opmode and check standard mode is achieved before finishing Setup
  uint8_t op_mode;
  if (!this->read_byte(ENS160_REG_OPMODE,&op_mode)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return; 
  }	

  if (op_mode != ENS160_OPMODE_STD) {
    this->error_code_ = STANDARD_OPMODE_FAILED;
    this->mark_failed();
    return; 
  }
}

void ENS160Component::update() {

  uint8_t op_mode, status_value, data_ready;
  
  if (!this->read_byte(ENS160_REG_DATA_STATUS,&status_value)) {
    ESP_LOGW(TAG, "Error reading status register");
    this->status_set_warning();
    return;
  }
  
  // verbose status logging
  ESP_LOGV(TAG, "Status Register value: %u", status_value);
  ESP_LOGV(TAG, "Status: ENS160_DATA_STATUS_STATAS 0x%x",
           (ENS160_DATA_STATUS_STATAS & (status_value)) == ENS160_DATA_STATUS_STATAS);
  ESP_LOGV(TAG, "Status: ENS160_DATA_STATUS_STATER 0x%x",
           (ENS160_DATA_STATUS_STATER & (status_value)) == ENS160_DATA_STATUS_STATER);
  ESP_LOGV(TAG, "Status: ENS160_DATA_STATUS_VALID_B 0x%x",
           (ENS160_DATA_STATUS_VALID_B & (status_value)) == ENS160_DATA_STATUS_VALID_B);
  ESP_LOGV(TAG, "Status: ENS160_DATA_STATUS_VALID_A 0x%x",
           (ENS160_DATA_STATUS_VALID_A & (status_value)) == ENS160_DATA_STATUS_VALID_A);
  ESP_LOGV(TAG, "Status: ENS160_DATA_STATUS_NEWDAT 0x%x",
           (ENS160_DATA_STATUS_NEWDAT & (status_value)) == ENS160_DATA_STATUS_NEWDAT);
  ESP_LOGV(TAG, "Status: ENS160_DATA_STATUS_NEWGPR 0x%x",
           (ENS160_DATA_STATUS_NEWGPR & (status_value)) == ENS160_DATA_STATUS_NEWGPR);

  data_ready = status_value & ENS160_DATA_STATUS_NEWDAT; 
  this->validity_flag_ = static_cast<ValidityFlag>((status_value & ENS160_VALIDITY_FLAG_MASK) >> 2);

  switch (validity_flag_) {
    case NORMAL_OPERATION:
      if (data_ready != ENS160_DATA_STATUS_NEWDAT) {
        ESP_LOGD(TAG, "ENS160 Sensor readings not available yet - Normal Operation but readings not ready"); 
        return;
      }
      break;
    case INITIAL_STARTUP:
      if (!this->initial_startup_) {
        this->initial_startup_ = true;
        ESP_LOGI(TAG, "ENS160 Sensor readings not available yet - Initial Start up requires 1 hour after first power on");
      }
      return;
    case WARMING_UP:
      if (!this->warming_up_) {
        this->warming_up_ = true;
        ESP_LOGI(TAG, "ENS160 Sensor readings not available yet - Warming up requires 3 minutes");
        this->send_env_data_();
      }
      return;
    case INVALID_OUTPUT:
      ESP_LOGE(TAG, "ENS160 Invalid Status - No Invalid Output");
      this->status_set_warning();
      return;
  }

  // read new data
  auto warning = false;
  auto buf_eco2 = this->read_bytes<2>(ENS160_REG_DATA_ECO2);
  auto buf_tvoc = this->read_bytes<2>(ENS160_REG_DATA_TVOC);
  

  if (buf_eco2.has_value() && this->co2_ != nullptr) {
    uint16_t data_eco2 = encode_uint16((*buf_eco2)[1], (*buf_eco2)[0]);
    this->co2_->publish_state(data_eco2);
  } else {
    ESP_LOGW(TAG, "No data for eCO2!");
    warning = true;
  }
  if (buf_tvoc.has_value() && this->tvoc_ != nullptr) {
    uint16_t data_tvoc = encode_uint16((*buf_tvoc)[1], (*buf_tvoc)[0]);
    this->tvoc_->publish_state(data_tvoc);
  } else {
    ESP_LOGW(TAG, "No data for TVOC!");
    warning = true;
  }
  
  uint8_t data_aqi;
  if (!this->read_byte(ENS160_REG_DATA_AQI,&data_aqi)) {
    ESP_LOGW(TAG, "Error reading AQI data register");
    warning = true;
  }
  
  if (this->aqi_ != nullptr) {
    if (data_aqi != 0) {
      //remove reserved bit in aqi njust in case they are used in future
      data_aqi = data_aqi & ENS160_DATA_AQI_MASK;
      
      this->aqi_->publish_state(data_aqi);
    } else {
      ESP_LOGW(TAG, "Invalid Data, AQI is 0!");
      warning = true;
    }   
  } else {
    ESP_LOGW(TAG, "No data for AQI!");
    warning = true;
  }

  // set status
  this->status_clear_error();
  if (warning) {
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }

  // set env data
  this->send_env_data_();
}

void ENS160Component::send_env_data_() {
  if (this->humidity_ == nullptr && this->temperature_ == nullptr)
    return;

  float humidity = NAN;
  if (this->humidity_ != nullptr)
    humidity = this->humidity_->state;

  float temperature = NAN;
  if (this->temperature_ != nullptr)
    temperature = this->temperature_->state;

  uint16_t t = (uint16_t)((temperature + 273.15f) * 64.0f);
  uint16_t h = (uint16_t)(humidity * 512.0f);

  uint8_t data[4];
  data[0] = t & 0xff;
  data[1] = (t >> 8) & 0xff;
  data[2] = h & 0xff;
  data[3] = (h >> 8) & 0xff;

  if (!this->write_bytes(ENS160_REG_TEMP_IN, data, 4)) {
    ESP_LOGE(TAG, "Error writing compensation values");
    this->status_set_warning();
    return;
  }
}

void ENS160Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ENS160:");
  LOG_I2C_DEVICE(this);
  
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication failed! Is the sensor connected?");
      break;
    case INVALID_ID:
      ESP_LOGE(TAG, "Sensor reported an invalid ID. Is this a ENS160?");
      break;
    case VALIDITY_INVALID_OUTPUT:
      ESP_LOGE(TAG, "Invalid Device Status - No valid output");
      break;
    case STANDARD_OPMODE_FAILED:
      ESP_LOGE(TAG, "Device failed to achieve Standard Operating Mode");
      break;
    case NONE:
      ESP_LOGD(TAG, "Setup successful");
      break;
  }
  ESP_LOGD(TAG,"Firmware Version: %d.%d.%d", this->firmware_ver_major_,this->firmware_ver_minor_,this->firmware_ver_build_);
  
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2 Sensor:", this->co2_);
  LOG_SENSOR("  ", "TVOC Sensor:", this->tvoc_);
  LOG_SENSOR("  ", "AQI Sensor:", this->aqi_);
  
  if (this->temperature_ != nullptr && this->humidity_ != nullptr) {
    LOG_SENSOR("  ", "Temperature Compensation:", this->temperature_);
    LOG_SENSOR("  ", "Humidity Compensation:", this->humidity_);
  } else {
    ESP_LOGCONFIG(TAG, "Compensation: Not configured");
  }
}

}  // namespace ens160
}  // namespace esphome
