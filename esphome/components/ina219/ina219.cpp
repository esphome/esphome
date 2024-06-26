#include "ina219.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ina219 {

static const char *const TAG = "ina219";

// | A0   | A1   | Address |
// | GND  | GND  | 0x40    |
// | GND  | V_S+ | 0x41    |
// | GND  | SDA  | 0x42    |
// | GND  | SCL  | 0x43    |
// | V_S+ | GND  | 0x44    |
// | V_S+ | V_S+ | 0x45    |
// | V_S+ | SDA  | 0x46    |
// | V_S+ | SCL  | 0x47    |
// | SDA  | GND  | 0x48    |
// | SDA  | V_S+ | 0x49    |
// | SDA  | SDA  | 0x4A    |
// | SDA  | SCL  | 0x4B    |
// | SCL  | GND  | 0x4C    |
// | SCL  | V_S+ | 0x4D    |
// | SCL  | SDA  | 0x4E    |
// | SCL  | SCL  | 0x4F    |

static const uint8_t INA219_READ = 0x01;
static const uint8_t INA219_REGISTER_CONFIG = 0x00;
static const uint8_t INA219_REGISTER_SHUNT_VOLTAGE = 0x01;
static const uint8_t INA219_REGISTER_BUS_VOLTAGE = 0x02;
static const uint8_t INA219_REGISTER_POWER = 0x03;
static const uint8_t INA219_REGISTER_CURRENT = 0x04;
static const uint8_t INA219_REGISTER_CALIBRATION = 0x05;

void INA219Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA219...");
  // Config Register
  // 0bx000000000000000 << 15 RESET Bit (1 -> trigger reset)
  if (!this->write_byte_16(INA219_REGISTER_CONFIG, 0x8000)) {
    this->mark_failed();
    return;
  }

  delay(1);

  // 0b00000xxxx0000000 << 7 Bus ADC Resolution/Averaging
  // 0b000000000xxxx000 << 3 Shunt ADC Resolution/Averaging

  // Value  Resolution, Averaging, Conversion
  // 0b0X00 -> 9 bit, 1 sample, 84 µs
  // 0b0X01 -> 10 bit, 1 sample, 148 µs
  // 0b0X10 -> 11 bit, 1 sample, 276 µs
  // 0b0X11 -> 12 bit, 1 sample, 532 µs
  // 0b1001 -> 12 bit, 2 samples, 1.06 ms
  // 0b1010 -> 12 bit, 4 samples, 2.13 ms
  // 0b1011 -> 12 bit, 8 samples, 4.26 ms
  // 0b1100 -> 12 bit, 16 samples, 8.51 ms
  // 0b1101 -> 12 bit, 32 samples, 17.02 ms
  // 0b1110 -> 12 bit, 64 samples, 34.05 ms
  // 0b1111 -> 12 bit, 128 samples, 68.10 ms <--

  // 0b0000000000000xxx << 0 Mode (Bus and Shunt continuous -> 0b111)

  // Bus ADC and Shunt ADC 12 bit+128 samples
  uint16_t config = 0x0000;
  // Continuous operation of Bus and Shunt ADCs
  config |= 0b0000000000000111;
  // Bus ADC and Shunt ADC 12 bit+128 samples -> 68.10 ms
  config |= 0b0000011110000000;
  config |= 0b0000000001111000;
  const float shunt_max_voltage = this->shunt_resistance_ohm_ * this->max_current_a_;

  // 0b00x0000000000000 << 13 Bus Voltage Range (0 -> 16V, 1 -> 32V)
  bool bus_32v_range = this->max_voltage_v_ > 16.0f || shunt_max_voltage > 0.16f;
  float multiplier;
  if (bus_32v_range) {
    config |= 0b0010000000000000;
    multiplier = 0.5f;
  } else {
    config |= 0b0000000000000000;
    multiplier = 1.0f;
  }

  // 0b000xx00000000000 << 11 Shunt Voltage Gain (0b00 -> 40mV, 0b01 -> 80mV, 0b10 -> 160mV, 0b11 -> 320mV)
  uint16_t shunt_gain;
  if (shunt_max_voltage * multiplier <= 0.02f) {
    shunt_gain = 0b00;  // 40mV
  } else if (shunt_max_voltage * multiplier <= 0.04f) {
    shunt_gain = 0b01;  // 80mV
  } else if (shunt_max_voltage * multiplier <= 0.08f) {
    shunt_gain = 0b10;  // 160mV
  } else {
    if (int(shunt_max_voltage * multiplier * 100) > 16) {
      ESP_LOGW(TAG,
               "    Max voltage across shunt resistor (resistance*current) exceeds %dmV. "
               "This could damage the sensor!",
               int(160 / multiplier));
    }
    shunt_gain = 0b11;  // 320mV
  }

  config |= shunt_gain << 11;
  ESP_LOGCONFIG(TAG, "    Using %dV-Range Shunt Gain=%dmV", bus_32v_range ? 32 : 16, 40 << shunt_gain);
  if (!this->write_byte_16(INA219_REGISTER_CONFIG, config)) {
    this->mark_failed();
    return;
  }

  auto min_lsb = uint32_t(ceilf(this->max_current_a_ * 1000000.0f / 0x8000));
  auto max_lsb = uint32_t(floorf(this->max_current_a_ * 1000000.0f / 0x1000));
  uint32_t lsb = min_lsb;
  for (; lsb <= max_lsb; lsb++) {
    float max_current_before_overflow = lsb * 0x7FFF / 1000000.0f;
    if (this->max_current_a_ <= max_current_before_overflow)
      break;
  }
  if (lsb > max_lsb) {
    lsb = max_lsb;
    ESP_LOGW(TAG, "    The requested current (%0.02fA) cannot be achieved without an overflow", this->max_current_a_);
  }

  this->calibration_lsb_ = lsb;
  auto calibration = uint32_t(0.04096f / (0.000001 * lsb * this->shunt_resistance_ohm_));
  ESP_LOGV(TAG, "    Using LSB=%" PRIu32 " calibration=%" PRIu32, lsb, calibration);
  if (!this->write_byte_16(INA219_REGISTER_CALIBRATION, calibration)) {
    this->mark_failed();
    return;
  }
}

void INA219Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA219:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA219 failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
}

float INA219Component::get_setup_priority() const { return setup_priority::DATA; }

void INA219Component::update() {
  if (this->bus_voltage_sensor_ != nullptr) {
    uint16_t raw_bus_voltage;
    if (!this->read_byte_16(INA219_REGISTER_BUS_VOLTAGE, &raw_bus_voltage)) {
      this->status_set_warning();
      return;
    }
    raw_bus_voltage >>= 3;
    float bus_voltage_v = int16_t(raw_bus_voltage) * 0.004f;
    this->bus_voltage_sensor_->publish_state(bus_voltage_v);
  }

  if (this->shunt_voltage_sensor_ != nullptr) {
    uint16_t raw_shunt_voltage;
    if (!this->read_byte_16(INA219_REGISTER_SHUNT_VOLTAGE, &raw_shunt_voltage)) {
      this->status_set_warning();
      return;
    }
    float shunt_voltage_mv = int16_t(raw_shunt_voltage) * 0.01f;
    this->shunt_voltage_sensor_->publish_state(shunt_voltage_mv / 1000.0f);
  }

  if (this->current_sensor_ != nullptr) {
    uint16_t raw_current;
    if (!this->read_byte_16(INA219_REGISTER_CURRENT, &raw_current)) {
      this->status_set_warning();
      return;
    }
    float current_ma = int16_t(raw_current) * (this->calibration_lsb_ / 1000.0f);
    this->current_sensor_->publish_state(current_ma / 1000.0f);
  }

  if (this->power_sensor_ != nullptr) {
    uint16_t raw_power;
    if (!this->read_byte_16(INA219_REGISTER_POWER, &raw_power)) {
      this->status_set_warning();
      return;
    }
    float power_mw = int16_t(raw_power) * (this->calibration_lsb_ * 20.0f / 1000.0f);
    this->power_sensor_->publish_state(power_mw / 1000.0f);
  }

  this->status_clear_warning();
}

}  // namespace ina219
}  // namespace esphome
