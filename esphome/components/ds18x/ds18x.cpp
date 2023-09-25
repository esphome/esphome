#include "ds18x.h"

#include "esphome/core/log.h"

static const char *const TAG = "ds18x";

using namespace esphome::ds18x;
using namespace esphome::ds248x;

bool DS18xTemperatureSensor::update()
{
    bool res = this->read_scratch_pad();

    if (!res) {
      ESP_LOGW(TAG, "'%s' - Resetting bus for read failed!", this->get_name().c_str());
      this->publish_state(NAN);
      this->parent_->status_set_warning();
      return false;
    }
    if (!this->check_scratch_pad()) {
      this->publish_state(NAN);
      this->parent_->status_set_warning();
      return false;
    }

    float value = this->get_value();
    ESP_LOGD(TAG, "'%s': Got value=%.1f", this->get_name().c_str(), value);
    this->publish_state(value);

    return true;
}

bool DS18xTemperatureSensor::setup_sensor() {
  bool r = this->read_scratch_pad();

  if (!r) {
    ESP_LOGE(TAG, "Reading scratchpad failed");
    return false;
  }
  if (!this->check_scratch_pad())
    return false;

  uint8_t resolution_register_val;
  switch (this->resolution_) {
    case 12:
      resolution_register_val = 0x7F;
      break;
    case 11:
      resolution_register_val = 0x5F;
      break;
    case 10:
      resolution_register_val = 0x3F;
      break;
    case 9:
    default:
      resolution_register_val = 0x1F;
      break;
  }


  if (this->scratch_pad_[4] == resolution_register_val)
    return true;
  this->scratch_pad_[4] = resolution_register_val; // update

  if (this->get_address8()[0] == DALLAS_MODEL_DS18S20) {
    // DS18S20 doesn't support resolution.
    ESP_LOGW(TAG, "DS18S20 doesn't support setting resolution.");
    return false;
  }

  bool result = this->reset_devices();
  if (!result) {
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->select();
  this->write_to_wire(DALLAS_COMMAND_WRITE_SCRATCH_PAD);
  this->write_to_wire(this->scratch_pad_[2]);  // high alarm temp
  this->write_to_wire(this->scratch_pad_[3]);  // low alarm temp
  this->write_to_wire(this->scratch_pad_[4]);  // resolution
  
  result = this->reset_devices();
  if (!result) {
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->select();
  this->write_to_wire(DALLAS_COMMAND_SAVE_EEPROM);

  delay(20);  // allow it to finish operation

  result = this->reset_devices();
  if (!result) {
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }
  ESP_LOGI(TAG, "set resolution to %d on sensor %s done", this->resolution_, this->get_address_name().c_str());
  return true;
}

uint8_t DS18xTemperatureSensor::get_resolution() const { return this->resolution_; }
void DS18xTemperatureSensor::set_resolution(uint8_t resolution) { this->resolution_ = resolution; }

uint16_t DS18xTemperatureSensor::millis_to_wait_for_conversion() const {
  switch (this->resolution_) {
    case 9:
      return 94;
    case 10:
      return 188;
    case 11:
      return 375;
    default:
      return 750;
  }
}

void DS18xTemperatureSensor::add_conversion_commands(std::set<uint8_t> &commands)
{
  commands.insert(DALLAS_COMMAND_START_CONVERSION);
}

float DS18xTemperatureSensor::get_temp_c() {
  int16_t temp = (int16_t(this->scratch_pad_[1]) << 11) | (int16_t(this->scratch_pad_[0]) << 3);
  if (this->get_address8()[0] == DALLAS_MODEL_DS18S20) {
    int diff = (this->scratch_pad_[7] - this->scratch_pad_[6]) << 7;
    temp = ((temp & 0xFFF0) << 3) - 16 + (diff / this->scratch_pad_[7]);
  }

  return temp / 128.0f;
}

float DS18xTemperatureSensor::get_value()
{
    return get_temp_c();
}