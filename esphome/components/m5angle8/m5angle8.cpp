#include "m5angle8.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace m5angle8 {

static const char *const TAG = "m5angle8";

static const uint8_t M5ANGLE8_REGISTER_ANALOG_INPUT_12B = 0x00;
static const uint8_t M5ANGLE8_REGISTER_ANALOG_INPUT_8B = 0x10;
static const uint8_t M5ANGLE8_REGISTER_DIGITAL_INPUT = 0x20;
static const uint8_t M5ANGLE8_REGISTER_RGB_24B = 0x30;
static const uint8_t M5ANGLE8_REGISTER_FW_VERSION = 0xFE;
static const uint8_t M5ANGLE8_REGISTER_ADDRESS = 0xFF;

static const uint8_t M5ANGLE8_NUM_KNOBS = 8;
static const uint8_t M5ANGLE8_NUM_LEDS = 9;



void M5Angle8Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5ANGLE8...");
  
  i2c::ErrorCode err = this->read_register(M5ANGLE8_REGISTER_FW_VERSION, &this->fw_version_, 1);
  if (err != i2c::NO_ERROR) {
      ESP_LOGE(TAG, "I2C error %02X...", err);
    //this->mark_failed();
    //return;
  };
}

void M5Angle8Component::dump_config() {
  ESP_LOGCONFIG(TAG, "M5ANGLE8:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Firmware version: %d ", this->fw_version_);
  LOG_UPDATE_INTERVAL(this);

  //LOG_SENSOR("  ", "Bus Voltage #1", this->channels_[0].bus_voltage_sensor_);
  //LOG_SENSOR("  ", "Shunt Voltage #1", this->channels_[0].shunt_voltage_sensor_);
}

void M5Angle8Component::update() {
  uint8_t knob_pos[M5ANGLE8_NUM_KNOBS];
  
  i2c::ErrorCode err=i2c::NO_ERROR;
  uint8_t retries;
  
  
  for (retries=5; retries>0; retries--){
    //err = this->read_register(M5ANGLE8_REGISTER_ANALOG_INPUT_8B, (uint8_t*)knob_pos, 8);
    if (err == i2c::NO_ERROR)
        break;      
    //err = this->write_register(0,&retries,1);
    yield();
  };

  if (err != i2c::NO_ERROR && retries==0)
      ESP_LOGE(TAG, "I2C error 0x%02X...", err);

  for (int i = 0; i < M5ANGLE8_NUM_KNOBS; i++) {
  
      if (this->knob_pos_sensor_[i] != nullptr){
      err = this->read_register(M5ANGLE8_REGISTER_ANALOG_INPUT_8B + i, (uint8_t*)&knob_pos[i], 1);
  
        this->knob_pos_sensor_[i]->publish_state(knob_pos[i] / 255.0f);
        }
      yield();
    };
}

float M5Angle8Component::get_setup_priority() const { return setup_priority::DATA; }



}  // namespace m5angle8
}  // namespace esphome
