#include "ds2438.h"

#include "esphome/core/log.h"

static const char* const TAG = "ds2438";
using namespace esphome::ds2438;

bool DS2438BatterySensor::setup_sensor() {
  return true;
}

bool DS2438BatterySensor::update()
{
  //TX  Reset  Reset pulse
  //RX  Presence  Presence pulse
  this->select_channel();
  
  bool result = this->reset_devices();
  if (!result) {
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  //TX  CCh  Skip ROM 
  //TX  B8h00h  Issue Recall Memory page 00h command 
  this->select();
  this->write_to_wire(DALLAS_COMMAND_RECALL_MEMORY);
  this->write_to_wire(0x00); // select page

  //TX  Reset  Reset pulse 
  //RX  Presence  Presence pulse 
  //TX  CCh  Skip ROM 
  //TX  BEh00h  Issue Read SP 00h command  
  //RX  <9 data bytes>  Read scratchpad data and CRC. This page contains 
  //temperature, voltage, and current measurements. 
  bool res = this->read_scratch_pad(0);  // select page

  if (!res) {
    ESP_LOGW(TAG, "'%s' - Reading scratchpad failed!", this->get_name().c_str());
    this->publish_state(NAN);
    if (NULL != this->temperature_sensor_) this->temperature_sensor_->publish_state(NAN);
    if (NULL != this->voltage_sensor_) this->voltage_sensor_->publish_state(NAN);
    if (NULL != this->current_sensor_) this->current_sensor_->publish_state(NAN);
    this->parent_->status_set_warning();
    return false;
  }
  if (!this->check_scratch_pad()) {
    this->publish_state(NAN);
    if (NULL != this->temperature_sensor_) this->temperature_sensor_->publish_state(NAN);
    if (NULL != this->voltage_sensor_) this->voltage_sensor_->publish_state(NAN);
    if (NULL != this->current_sensor_) this->current_sensor_->publish_state(NAN);
    this->parent_->status_set_warning();
    return false;
  }

  updateTemp();
  updateVoltage();
  updateCurrent();

  return true;
}

void DS2438BatterySensor::updateTemp()
{
  if (NULL == this->temperature_sensor_) return;
  float temp = ((int8_t)scratch_pad_[2]) + ((float)(scratch_pad_[1] >> 3) / 128.0f);
  this->temperature_sensor_->publish_state(temp);
}

void DS2438BatterySensor::updateVoltage()
{
  if (NULL == this->voltage_sensor_) return;
  float voltage = (((uint16_t)scratch_pad_[4] << 8) + scratch_pad_[3]) / 100.0f;
  this->voltage_sensor_->publish_state(voltage);
}

void DS2438BatterySensor::updateCurrent()
{
  if (NULL == this->current_sensor_) return;
  float current = (((int16_t)scratch_pad_[6] << 8) + (int16_t)scratch_pad_[5]) * 0.2441f / 1000.0f;
  current_sensor_->publish_state(current);
}


void DS2438BatterySensor::add_conversion_commands(std::set<uint8_t> &commands)
{
  commands.insert(DALLAS_COMMAND_START_CONVERSION);
  commands.insert(DALLAS_COMMAND_START_TCONVERSION);
}
