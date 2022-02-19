#include "max9611.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c_bus.h"
namespace esphome {
namespace max9611 {
using namespace esphome::i2c;
// Sign extend
// http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
template<typename T, unsigned B> inline T signextend(const T x) {
  struct {
    T x : B;
  } s;
  return s.x = x;
}
// Map the gain register to in uV/LSB
float gain_to_lsb(MAX9611Multiplexer gain) {
  float lsb = 0.0;
  if (gain == MAX9611_MULTIPLEXER_CSA_GAIN1) {
    lsb = 107.50;
  } else if (gain == MAX9611_MULTIPLEXER_CSA_GAIN4) {
    lsb = 26.88;
  } else if (gain == MAX9611_MULTIPLEXER_CSA_GAIN8) {
    lsb = 13.44;
  }
  return lsb;
}
static const char *const TAG = "max9611";
static const uint8_t SETUP_DELAY = 4;         // Wait 2 integration periods.
static const float VOUT_LSB = 14.0 / 1000.0;  // 14mV/LSB
static const float TEMP_LSB = 0.48;           // 0.48C/LSB
static const float MICRO_VOLTS_PER_VOLT = 1000000.0;
void MAX9611Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up max9611...");
  // Perform dummy-read
  uint8_t value;
  this->read(&value, 1);
  // Configuration Stage.
  // First send an integration request with the specified gain
  const uint8_t setup_dat[] = {CONTROL_REGISTER_1_ADRR, static_cast<uint8_t>(gain_)};
  // Then send a request that samples all channels as fast as possible, using the last provided gain
  const uint8_t fast_mode_dat[] = {CONTROL_REGISTER_1_ADRR, MAX9611Multiplexer::MAX9611_MULTIPLEXER_FAST_MODE};

  if (this->write(reinterpret_cast<const uint8_t *>(&setup_dat), sizeof(setup_dat)) != ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to setup Max9611 during GAIN SET");
    return;
  }
  delay(SETUP_DELAY);
  if (this->write(reinterpret_cast<const uint8_t *>(&fast_mode_dat), sizeof(fast_mode_dat)) != ErrorCode::ERROR_OK) {
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
  // Setup read from 0x0 register base
  const uint8_t reg_base = 0x0;
  const ErrorCode write_result = this->write(&reg_base, 1);
  // Just read the entire register map in a bulk read, faster than individually querying register.
  const ErrorCode read_result = this->read(register_map_, sizeof(register_map_));
  if (write_result != ErrorCode::ERROR_OK || read_result != ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "MAX9611 Update FAILED!");
    return;
  }
  uint16_t csa_register = ((register_map_[CSA_DATA_BYTE_MSB_ADRR] << 8) | (register_map_[CSA_DATA_BYTE_LSB_ADRR])) >> 4;
  uint16_t rs_register = ((register_map_[RS_DATA_BYTE_MSB_ADRR] << 8) | (register_map_[RS_DATA_BYTE_LSB_ADRR])) >> 4;
  uint16_t t_register = ((register_map_[TEMP_DATA_BYTE_MSB_ADRR] << 8) | (register_map_[TEMP_DATA_BYTE_LSB_ADRR])) >> 7;
  float voltage = rs_register * VOUT_LSB;
  float shunt_voltage = (csa_register * gain_to_lsb(gain_)) / MICRO_VOLTS_PER_VOLT;
  float temp = signextend<signed int, 9>(t_register) * TEMP_LSB;
  float amps = shunt_voltage / current_resistor_;
  float watts = amps * voltage;

  if (voltage_sensor_ != nullptr) {
    voltage_sensor_->publish_state(voltage);
  }
  if (current_sensor_ != nullptr) {
    current_sensor_->publish_state(amps);
  }
  if (watt_sensor_ != nullptr) {
    watt_sensor_->publish_state(watts);
  }
  if (temperature_sensor_ != nullptr) {
    temperature_sensor_->publish_state(temp);
  }

  ESP_LOGD(TAG, "V: %f, A: %f, W: %f, Deg C: %f", voltage, amps, watts, temp);
}
}  // namespace max9611
}  // namespace esphome
