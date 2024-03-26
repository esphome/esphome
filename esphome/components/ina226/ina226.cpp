#include "ina226.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace ina226 {

static const char *const TAG = "ina226";

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

static const uint8_t INA226_REGISTER_CONFIG = 0x00;
static const uint8_t INA226_REGISTER_SHUNT_VOLTAGE = 0x01;
static const uint8_t INA226_REGISTER_BUS_VOLTAGE = 0x02;
static const uint8_t INA226_REGISTER_POWER = 0x03;
static const uint8_t INA226_REGISTER_CURRENT = 0x04;
static const uint8_t INA226_REGISTER_CALIBRATION = 0x05;
static const uint8_t INA226_REGISTER_MASK_ENABLE = 0x06;

static const uint16_t INA226_MASK_ENABLE_CONVERSION_READY = 0b1000;

static const uint16_t INA226_ADC_TIMES[] = {140, 204, 332, 588, 1100, 2116, 4156, 8244};
static const uint16_t INA226_ADC_AVG_SAMPLES[] = {1, 4, 16, 64, 128, 256, 512, 1024};

static const uint32_t INA226_TIMEOUT_MS = 10 * 1000;

void INA226Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA226...");

  this->config_reg_.reset = 1;
  if (!this->write_byte_16(INA226_REGISTER_CONFIG, this->config_reg_.raw)) {
    this->mark_failed();
    return;
  }
  delay(1);

  this->config_reg_.raw = 0;
  this->config_reg_.reserved = 0b100;  // as per datasheet

  // Averaging Mode AVG Bit Settings[11:9] (000 -> 1 sample, 001 -> 4 sample, 111 -> 1024 samples)
  this->config_reg_.avg_samples = this->adc_avg_samples_;

  // Bus Voltage Conversion Time VBUSCT Bit Settings [8:6] (100 -> 1.1ms, 111 -> 8.244 ms)
  this->config_reg_.bus_voltage_conversion_time = this->adc_time_voltage_;

  // Shunt Voltage Conversion Time VSHCT Bit Settings [5:3] (100 -> 1.1ms, 111 -> 8.244 ms)
  this->config_reg_.shunt_voltage_conversion_time = this->adc_time_current_;

  // Mode Settings [2:0] Combinations (111 -> Shunt and Bus, Continuous)
  this->config_reg_.mode =
      this->low_power_mode_ ? AdcMode::ADC_MODE_POWER_DOWN : AdcMode::ADC_MODE_SHUNT_AND_BUS_CONTINUOUS;

  if (!this->write_byte_16(INA226_REGISTER_CONFIG, this->config_reg_.raw)) {
    this->mark_failed();
    return;
  }

  // lsb is multiplied by 1000000 to store it as an integer value
  uint32_t lsb = static_cast<uint32_t>(ceilf(this->max_current_a_ * 1000000.0f / 32768));

  this->calibration_lsb_ = lsb;

  auto calibration = uint32_t(0.00512 / (lsb * this->shunt_resistance_ohm_ / 1000000.0f));

  ESP_LOGV(TAG, "    Using LSB=%" PRIu32 " calibration=%" PRIu32, lsb, calibration);

  if (!this->write_byte_16(INA226_REGISTER_CALIBRATION, calibration)) {
    this->mark_failed();
    return;
  }
}

void INA226Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA226:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA226 failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  ADC Conversion Time Bus Voltage: %d", INA226_ADC_TIMES[this->adc_time_voltage_ & 0b111]);
  ESP_LOGCONFIG(TAG, "  ADC Conversion Time Shunt Voltage: %d", INA226_ADC_TIMES[this->adc_time_current_ & 0b111]);
  ESP_LOGCONFIG(TAG, "  ADC Averaging Samples: %d", INA226_ADC_AVG_SAMPLES[this->adc_avg_samples_ & 0b111]);

  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
}

float INA226Component::get_setup_priority() const { return setup_priority::DATA; }

void INA226Component::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    this->start_measurements_();
    this->last_start_time_ = millis();
    this->state_ = State::WAITING_FOR_DATA;
  }
}

void INA226Component::loop() {
  if (!this->is_ready())
    return;

  switch (this->state_) {
    case State::NOT_INITIALIZED:
    case State::IDLE:
      break;
    case State::WAITING_FOR_DATA:
      if (this->is_conversion_ready_()) {
        this->state_ = State::READY_TO_PUBLISH;
      } else if (millis() - this->last_start_time_ > INA226_TIMEOUT_MS) {
        ESP_LOGW(TAG, "INA226 Data collection timeout");
        this->status_set_warning();
        this->state_ = State::IDLE;
      } else {
        ESP_LOGD(TAG, "INA226 Data not ready yet, waiting...");
      }
      break;
    case State::READY_TO_PUBLISH:
      this->stop_measurements_();
      this->get_and_publish_data_();
      this->state_ = State::IDLE;
      break;
    default:
      // shall never be here
      break;
  }
}

void INA226Component::get_and_publish_data_() {
  if (this->bus_voltage_sensor_ != nullptr) {
    uint16_t raw_bus_voltage;
    if (!this->read_byte_16(INA226_REGISTER_BUS_VOLTAGE, &raw_bus_voltage)) {
      this->status_set_warning();
      return;
    }
    // Convert for 2's compliment and signed value (though always positive)
    float bus_voltage_v = this->twos_complement_(raw_bus_voltage, 16);
    bus_voltage_v *= 0.00125f;
    this->bus_voltage_sensor_->publish_state(bus_voltage_v);
  }

  if (this->shunt_voltage_sensor_ != nullptr) {
    uint16_t raw_shunt_voltage;
    if (!this->read_byte_16(INA226_REGISTER_SHUNT_VOLTAGE, &raw_shunt_voltage)) {
      this->status_set_warning();
      return;
    }
    // Convert for 2's compliment and signed value
    float shunt_voltage_v = this->twos_complement_(raw_shunt_voltage, 16);
    shunt_voltage_v *= 0.0000025f;
    this->shunt_voltage_sensor_->publish_state(shunt_voltage_v);
  }

  if (this->current_sensor_ != nullptr) {
    uint16_t raw_current;
    if (!this->read_byte_16(INA226_REGISTER_CURRENT, &raw_current)) {
      this->status_set_warning();
      return;
    }
    // Convert for 2's compliment and signed value
    float current_ma = this->twos_complement_(raw_current, 16);
    current_ma *= (this->calibration_lsb_ / 1000.0f);
    this->current_sensor_->publish_state(current_ma / 1000.0f);
  }

  if (this->power_sensor_ != nullptr) {
    uint16_t raw_power;
    if (!this->read_byte_16(INA226_REGISTER_POWER, &raw_power)) {
      this->status_set_warning();
      return;
    }
    float power_mw = int16_t(raw_power) * (this->calibration_lsb_ * 25.0f / 1000.0f);
    this->power_sensor_->publish_state(power_mw / 1000.0f);
  }

  this->status_clear_warning();
}

int32_t INA226Component::twos_complement_(int32_t val, uint8_t bits) {
  if (val & ((uint32_t) 1 << (bits - 1))) {
    val -= (uint32_t) 1 << bits;
  }
  return val;
}

void INA226Component::stop_measurements_() {
  if (!this->low_power_mode_)
    return;

  this->config_reg_.mode = AdcMode::ADC_MODE_POWER_DOWN;
  if (!this->write_byte_16(INA226_REGISTER_CONFIG, this->config_reg_.raw)) {
    this->mark_failed();
  }
}

void INA226Component::start_measurements_() {
  if (!this->low_power_mode_)
    return;
  this->config_reg_.mode = AdcMode::ADC_MODE_SHUNT_AND_BUS_TRIGGERED;
  if (!this->write_byte_16(INA226_REGISTER_CONFIG, this->config_reg_.raw)) {
    this->mark_failed();
  }
}

bool INA226Component::is_conversion_ready_() {
  uint16_t me_reg;
  if (!this->read_byte_16(INA226_REGISTER_MASK_ENABLE, &me_reg)) {
    this->mark_failed();
    return false;
  }
  return (me_reg & INA226_MASK_ENABLE_CONVERSION_READY);
}

}  // namespace ina226
}  // namespace esphome
