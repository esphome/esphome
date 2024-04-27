#include "ams5915.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ams5915 {
static const char *const TAG = "ams5915";

void Ams5915::set_transducer_type(Transducer type){
  _type = type; 
}

/* starts the I2C communication and sets the pressure and temperature ranges using getTransducer */
int Ams5915::begin(){
	// setting the min and max pressure based on the chip
	this->getTransducer();
  // checking to see if we can talk with the sensor
  for (size_t i=0; i < _maxAttempts; i++) {
    _status = readBytes(&_pressureCounts,&_temperatureCounts);
    if (_status > 0) {break;}
    delay(10);
  }
  return _status;
}

/* reads data from the sensor */
int Ams5915::readSensor(){
  // get pressure and temperature counts off transducer
  _status = readBytes(&_pressureCounts,&_temperatureCounts);
  // convert counts to pressure, PA
  _data.Pressure_Pa = (((float)(_pressureCounts - _digOutPmin))/(((float)(_digOutPmax - _digOutPmin))/((float)(_pMax - _pMin)))+(float)_pMin)*_mBar2Pa;
  // convert counts to temperature, C
  _data.Temp_C = (float)((_temperatureCounts*200))/2048.0f-50.0f;
  return _status;
}

/* returns the pressure value, PA */
float Ams5915::getPressure_Pa(){
  return _data.Pressure_Pa;
}

/* returns the temperature value, C */
float Ams5915::getTemperature_C(){
  return _data.Temp_C;
}

/* sets the pressure range based on the chip */
void Ams5915::getTransducer(){
  // setting the min and max pressures based on which transducer it is
  switch(_type) {
    case AMS5915_0005_D:
      _pMin = AMS5915_0005_D_P_MIN;
      _pMax = AMS5915_0005_D_P_MAX;
      break;
    case AMS5915_0010_D:
      _pMin = AMS5915_0010_D_P_MIN;
      _pMax = AMS5915_0010_D_P_MAX;
      break;
    case AMS5915_0005_D_B:
      _pMin = AMS5915_0005_D_B_P_MIN;
      _pMax = AMS5915_0005_D_B_P_MAX;
      break;
    case AMS5915_0010_D_B:
      _pMin = AMS5915_0010_D_B_P_MIN;
      _pMax = AMS5915_0010_D_B_P_MAX;
      break;
    case AMS5915_0020_D:
      _pMin = AMS5915_0020_D_P_MIN;
      _pMax = AMS5915_0020_D_P_MAX;
      break;
    case AMS5915_0050_D:
      _pMin = AMS5915_0050_D_P_MIN;
      _pMax = AMS5915_0050_D_P_MAX;
      break;
    case AMS5915_0100_D:
      _pMin = AMS5915_0100_D_P_MIN;
      _pMax = AMS5915_0100_D_P_MAX;
      break;
    case AMS5915_0020_D_B:
      _pMin = AMS5915_0020_D_B_P_MIN;
      _pMax = AMS5915_0020_D_B_P_MAX;
      break;
    case AMS5915_0050_D_B:
      _pMin = AMS5915_0050_D_B_P_MIN;
      _pMax = AMS5915_0050_D_B_P_MAX;
      break;
    case AMS5915_0100_D_B:
      _pMin = AMS5915_0100_D_B_P_MIN;
      _pMax = AMS5915_0100_D_B_P_MAX;
      break;
    case AMS5915_0200_D:
      _pMin = AMS5915_0200_D_P_MIN;
      _pMax = AMS5915_0200_D_P_MAX;
      break;
    case AMS5915_0350_D:
      _pMin = AMS5915_0350_D_P_MIN;
      _pMax = AMS5915_0350_D_P_MAX;
      break;
    case AMS5915_1000_D:
      _pMin = AMS5915_1000_D_P_MIN;
      _pMax = AMS5915_1000_D_P_MAX;
      break;
    case AMS5915_2000_D:
      _pMin = AMS5915_2000_D_P_MIN;
      _pMax = AMS5915_2000_D_P_MAX;
      break;
    case AMS5915_4000_D:
      _pMin = AMS5915_4000_D_P_MIN;
      _pMax = AMS5915_4000_D_P_MAX;
      break;
    case AMS5915_7000_D:
      _pMin = AMS5915_7000_D_P_MIN;
      _pMax = AMS5915_7000_D_P_MAX;
      break;
    case AMS5915_10000_D:
      _pMin = AMS5915_10000_D_P_MIN;
      _pMax = AMS5915_10000_D_P_MAX;
      break;
    case AMS5915_0200_D_B:
      _pMin = AMS5915_0200_D_B_P_MIN;
      _pMax = AMS5915_0200_D_B_P_MAX;
      break;
    case AMS5915_0350_D_B:
      _pMin = AMS5915_0350_D_B_P_MIN;
      _pMax = AMS5915_0350_D_B_P_MAX;
      break;
    case AMS5915_1000_D_B:
      _pMin = AMS5915_1000_D_B_P_MIN;
      _pMax = AMS5915_1000_D_B_P_MAX;
      break;
    case AMS5915_1000_A:
      _pMin = AMS5915_1000_A_P_MIN;
      _pMax = AMS5915_1000_A_P_MAX;
      break;
    case AMS5915_1200_B:
      _pMin = AMS5915_1200_B_P_MIN;
      _pMax = AMS5915_1200_B_P_MAX;
      break;
  }
}

std::string Ams5915::intToBinary(int n) {
    std::string binary = "";
    while(n > 0) {
        binary = std::to_string(n % 2) + binary;
        n = n / 2;
    }
    return binary;
}

/* reads pressure and temperature and returns values in counts */
int Ams5915::readBytes(uint16_t* pressureCounts,uint16_t* temperatureCounts){
  i2c::ErrorCode err = this->read(_buffer,sizeof(_buffer));
  for (int i = 0; i < sizeof(_buffer); i++){
    ESP_LOGD(TAG, "data: [%i]", _buffer[i]);
    ESP_LOGD(TAG, "binary: [%s]", this->intToBinary(_buffer[i]).c_str());
  }
  if (err != i2c::ERROR_OK){
    _status = -1;
  } else {
    *pressureCounts = (((uint16_t) (_buffer[0]&0x3F)) <<8) + (((uint16_t) _buffer[1]));
    *temperatureCounts = (((uint16_t) (_buffer[2])) <<3) + (((uint16_t) _buffer[3]&0xE0)>>5);
    _status = 1;
  }
  return _status;
}

void Ams5915::setup() {
  if (this->begin() < 0) {
    ESP_LOGE(TAG, "Failed to read pressure from Ams5915");
    this->mark_failed();
  }
}

void Ams5915::update() {
  this->readSensor();
  float temperature = this->getTemperature_C();
  float pressure = this->getPressure_Pa();


  ESP_LOGD(TAG, "Got temperature=%.1f°C pressure=%.1fpa", temperature, pressure);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);
}


void Ams5915::dump_config() {
  ESP_LOGCONFIG(TAG, "Ams5915:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}


}  // namespace Ams5915
}  // namespace esphome
