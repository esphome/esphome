#include "max9611.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c_bus.h"

namespace esphome {
namespace max9611 {

using namespace esphome::i2c;

//Sign extend
//http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
template <typename T, unsigned B>
inline T signextend(const T x)
{
  struct {T x:B;} s;
  return s.x = x;
}

//Map the gain register to in uV/LSB
const float gainToLSB(MAX9611Multiplexer gain) {
  float lsb = 0.0;
  if(gain == MAX9611_MULTIPLEXER_CSA_GAIN1) {
    lsb = 107.50;
  } else if(gain == MAX9611_MULTIPLEXER_CSA_GAIN4) {
    lsb = 26.88;
  } else if(gain == MAX9611_MULTIPLEXER_CSA_GAIN8) {
    lsb = 13.44;
  }
  return lsb;
}

static const char *const TAG = "max9611";
static const uint8_t SETUP_DELAY = 4; //Wait 2 integration periods.
static const float Vout_LSB = 14.0 / 1000.0; // 14mV/LSB
static const float Temp_LSB = 0.48; // 0.48C/LSB
static const float microVoltsPerVolt = 1000000.0;

void MAX9611Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up max9611...");
  //First send an integration request with the specified gain
  const uint8_t setupDat[] = {CONTROL_REGISTER_1_ADRR, gain_};
  //Then send a request that samples all channels as fast as possible, using the last provided gain
  const uint8_t fastModeDat[] = {CONTROL_REGISTER_1_ADRR, MAX9611Multiplexer::MAX9611_MULTIPLEXER_FAST_MODE};
  
  if(this->write(reinterpret_cast<const uint8_t *>(&setupDat), sizeof(setupDat)) != ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to setup Max9611 during GAIN SET");
    return;
  }
  delay(SETUP_DELAY);
  if(this->write(reinterpret_cast<const uint8_t *>(&fastModeDat), sizeof(fastModeDat)) != ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to setup Max9611 during FAST MODE SET");
    return;
  }
}
void MAX9611Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Dump Config max9611...");
  ESP_LOGCONFIG(TAG, "    CSA Gain Register: %x", gain_);
  LOG_I2C_DEVICE(this);
}
void MAX9611Component::update() {
  //Setup read from 0x0 register base
  const uint8_t regBase = 0x0;
  const ErrorCode writeResult = this->write(&regBase, 1);
  //Just read the entire register map in a bulk read, faster than individually querying register. 
  const ErrorCode readResult = this->read(registerMap_, sizeof(registerMap_));
  if(writeResult != ErrorCode::ERROR_OK || readResult != ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "MAX9611 Update FAILED!");
    return;
  }
  uint16_t csaRegister = ((registerMap_[CSA_DATA_BYTE_MSB_ADRR ] << 8) | (registerMap_[CSA_DATA_BYTE_LSB_ADRR ])) >> 4;
  uint16_t rsRegister  = ((registerMap_[RS_DATA_BYTE_MSB_ADRR  ] << 8) | (registerMap_[RS_DATA_BYTE_LSB_ADRR  ])) >> 4;
  uint16_t tRegister   = ((registerMap_[TEMP_DATA_BYTE_MSB_ADRR] << 8) | (registerMap_[TEMP_DATA_BYTE_LSB_ADRR])) >> 7;
  float voltage = rsRegister * Vout_LSB;
  float shuntVoltage = (csaRegister * gainToLSB(gain_)) / microVoltsPerVolt;
  float temp = signextend<signed int,9>(tRegister) * Temp_LSB;
  float amps = shuntVoltage / currentResistor_;
  float watts = amps * voltage;
  
  if(voltageSensor_ != nullptr) { 
      voltageSensor_->publish_state(voltage);
  }
  if(currentSensor_ != nullptr) { 
      currentSensor_->publish_state(amps);
  }
  if(wattSensor_ != nullptr) { 
      wattSensor_->publish_state(watts);
  }
  if(temperatureSensor_ != nullptr) { 
      temperatureSensor_->publish_state(temp);
  }
  
  ESP_LOGI(TAG, "V: %f, A: %f, W: %f, Deg C: %f",voltage,amps,watts,temp);
  
}

}  // namespace max9611
}  // namespace esphome
