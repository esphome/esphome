#include "max31865.h"

#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace max31865 {

static const char *const TAG = "max31865";

void MAX31865Sensor::update() {
  // Check new faults since last measurement
  if (!has_fault_) {
    const uint8_t faults = this->read_register_(FAULT_STATUS_REG);
    if (faults & 0b11111100) {
      if (faults & (1 << 2)) {
        ESP_LOGW(TAG, "Overvoltage/undervoltage fault between measurements");
      }
      if (faults & (1 << 3)) {
        ESP_LOGW(TAG, "RTDIN- < 0.85 x V_BIAS (FORCE- open) between measurements");
      }
      if (faults & (1 << 4)) {
        ESP_LOGW(TAG, "REFIN- < 0.85 x V_BIAS (FORCE- open) between measurements");
      }
      if (faults & (1 << 5)) {
        ESP_LOGW(TAG, "REFIN- > 0.85 x V_BIAS between measurements");
      }
      if (!has_warn_) {
        if (faults & (1 << 6)) {
          ESP_LOGW(TAG, "RTD Low Threshold between measurements");
        }
        if (faults & (1 << 7)) {
          ESP_LOGW(TAG, "RTD High Threshold between measurements");
        }
      }
    }
  }

  // Run fault detection
  this->write_config_(0b11101110, 0b10000110);
  const uint32_t start_time = micros();
  uint8_t config;
  uint32_t fault_detect_time;
  do {
    config = this->read_register_(CONFIGURATION_REG);
    fault_detect_time = micros() - start_time;
    if ((fault_detect_time >= 6000) && (config & 0b00001100)) {
      ESP_LOGE(TAG, "Fault detection incomplete (0x%02X) after %uμs (datasheet spec is 600μs max)! Aborting read.",
               config, fault_detect_time);
      this->publish_state(NAN);
      this->status_set_error();
      return;
    }
  } while (config & 0b00001100);
  ESP_LOGV(TAG, "Fault detection completed in %uμs.", fault_detect_time);

  // Start 1-shot conversion
  this->write_config_(0b11100000, 0b10100000);

  // Datasheet max conversion time is 55ms for 60Hz / 66ms for 50Hz
  auto f = std::bind(&MAX31865Sensor::read_data_, this);
  this->set_timeout("value", filter_ == FILTER_60HZ ? 55 : 66, f);
}

void MAX31865Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX31865Sensor '%s'...", this->name_.c_str());
  this->spi_setup();

  // Build base configuration
  base_config_ = 0b00000000;
  base_config_ |= (filter_ & 1) << 0;
  if (rtd_wires_ == 3) {
    base_config_ |= 1 << 4;
  }

  // Clear any existing faults & set base config
  this->write_config_(0b00000010, 0b00000010);
}

void MAX31865Sensor::dump_config() {
  LOG_SENSOR("", "MAX31865", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Reference Resistance: %.2fΩ", reference_resistance_);
  ESP_LOGCONFIG(TAG, "  RTD: %u-wire %.2fΩ", rtd_wires_, rtd_nominal_resistance_);
  ESP_LOGCONFIG(TAG, "  Mains Filter: %s",
                (filter_ == FILTER_60HZ ? "60 Hz" : (filter_ == FILTER_50HZ ? "50 Hz" : "Unknown!")));
}

float MAX31865Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MAX31865Sensor::read_data_() {
  // Read temperature, disable V_BIAS (save power)
  const uint16_t rtd_resistance_register = this->read_register_16_(RTD_RESISTANCE_MSB_REG);
  this->write_config_(0b11000000, 0b00000000);

  // Check for bad connection
  if (rtd_resistance_register == 0b0000000000000000 || rtd_resistance_register == 0b1111111111111111) {
    ESP_LOGE(TAG, "SPI bus read all 0 or all 1 (0x%04X), check MAX31865 wiring & power.", rtd_resistance_register);
    this->publish_state(NAN);
    this->status_set_error();
    return;
  }

  // Check faults
  const uint8_t faults = this->read_register_(FAULT_STATUS_REG);
  if ((has_fault_ = faults & 0b00111100)) {
    if (faults & (1 << 2)) {
      ESP_LOGE(TAG, "Overvoltage/undervoltage fault");
    }
    if (faults & (1 << 3)) {
      ESP_LOGE(TAG, "RTDIN- < 0.85 x V_BIAS (FORCE- open)");
    }
    if (faults & (1 << 4)) {
      ESP_LOGE(TAG, "REFIN- < 0.85 x V_BIAS (FORCE- open)");
    }
    if (faults & (1 << 5)) {
      ESP_LOGE(TAG, "REFIN- > 0.85 x V_BIAS");
    }
    this->publish_state(NAN);
    this->status_set_error();
    return;
  } else {
    this->status_clear_error();
  }
  if ((has_warn_ = faults & 0b11000000)) {
    if (faults & (1 << 6)) {
      ESP_LOGW(TAG, "RTD Low Threshold");
    }
    if (faults & (1 << 7)) {
      ESP_LOGW(TAG, "RTD High Threshold");
    }
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }

  // Process temperature
  if (rtd_resistance_register & 0x0001) {
    ESP_LOGW(TAG, "RTD Resistance Registers fault bit set! (0x%04X)", rtd_resistance_register);
    this->status_set_warning();
  }
  const float rtd_ratio = static_cast<float>(rtd_resistance_register >> 1) / static_cast<float>((1 << 15) - 1);
  const float temperature = this->calc_temperature_(rtd_ratio);
  ESP_LOGV(TAG, "RTD read complete. %.5f (ratio) * %.1fΩ (reference) = %.2fΩ --> %.2f°C", rtd_ratio,
           reference_resistance_, reference_resistance_ * rtd_ratio, temperature);
  this->publish_state(temperature);
}

void MAX31865Sensor::write_config_(uint8_t mask, uint8_t bits, uint8_t start_position) {
  uint8_t value = base_config_;

  value &= (~mask);
  value |= (bits << start_position);

  this->write_register_(CONFIGURATION_REG, value);
}

void MAX31865Sensor::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg |= SPI_WRITE_M);
  this->write_byte(value);
  this->disable();
  ESP_LOGVV(TAG, "write_register_ 0x%02X: 0x%02X", reg, value);
}

uint8_t MAX31865Sensor::read_register_(uint8_t reg) {
  this->enable();
  this->write_byte(reg);
  const uint8_t value(this->read_byte());
  this->disable();
  ESP_LOGVV(TAG, "read_register_ 0x%02X: 0x%02X", reg, value);
  return value;
}

uint16_t MAX31865Sensor::read_register_16_(uint8_t reg) {
  this->enable();
  this->write_byte(reg);
  const uint8_t msb(this->read_byte());
  const uint8_t lsb(this->read_byte());
  this->disable();
  const uint16_t value((msb << 8) | lsb);
  ESP_LOGVV(TAG, "read_register_16_ 0x%02X: 0x%04X", reg, value);
  return value;
}

float MAX31865Sensor::calc_temperature_(float rtd_ratio) {
  // Based loosely on Adafruit's library: https://github.com/adafruit/Adafruit_MAX31865
  // Mainly based on formulas provided by Analog:
  // http://www.analog.com/media/en/technical-documentation/application-notes/AN709_0.pdf

  const float a = 3.9083e-3;
  const float b = -5.775e-7;
  const float z1 = -a;
  const float z2 = a * a - 4 * b;
  const float z3 = 4 * b / rtd_nominal_resistance_;
  const float z4 = 2 * b;

  float rtd_resistance = rtd_ratio * reference_resistance_;

  // ≥ 0°C Formula
  const float pos_temp = (z1 + std::sqrt(z2 + (z3 * rtd_resistance))) / z4;
  if (pos_temp >= 0) {
    return pos_temp;
  }

  // < 0°C Formula
  if (rtd_nominal_resistance_ != 100) {
    // Normalize RTD to 100Ω
    rtd_resistance /= rtd_nominal_resistance_;
    rtd_resistance *= 100;
  }
  float rpoly = rtd_resistance;
  float neg_temp = -242.02f;
  neg_temp += 2.2228f * rpoly;
  rpoly *= rtd_resistance;  // square
  neg_temp += 2.5859e-3f * rpoly;
  rpoly *= rtd_resistance;  // ^3
  neg_temp -= 4.8260e-6f * rpoly;
  rpoly *= rtd_resistance;  // ^4
  neg_temp -= 2.8183e-8f * rpoly;
  rpoly *= rtd_resistance;  // ^5
  neg_temp += 1.5243e-10f * rpoly;
  return neg_temp;
}

}  // namespace max31865
}  // namespace esphome
