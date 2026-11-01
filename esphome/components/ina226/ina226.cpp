#include "ina226.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome::ina226 {

ESPHOME_LOG_TAG(TAG, "ina226");

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
static const uint8_t INA226_REGISTER_ALERT_LIMIT = 0x07;

static const uint16_t INA226_ADC_TIMES[] = {140, 204, 332, 588, 1100, 2116, 4156, 8244};
static const uint16_t INA226_ADC_AVG_SAMPLES[] = {1, 4, 16, 64, 128, 256, 512, 1024};

void INA226Component::setup() {
  ConfigurationRegister config;

  config.reset = 1;
  if (!this->write_byte_16(INA226_REGISTER_CONFIG, config.raw)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  delay(1);

  config.raw = 0;
  config.reserved = 0b100;  // as per datasheet

  // Averaging Mode AVG Bit Settings[11:9] (000 -> 1 sample, 001 -> 4 sample, 111 -> 1024 samples)
  config.avg_samples = this->adc_avg_samples_;

  // Bus Voltage Conversion Time VBUSCT Bit Settings [8:6] (100 -> 1.1ms, 111 -> 8.244 ms)
  config.bus_voltage_conversion_time = this->adc_time_voltage_;

  // Shunt Voltage Conversion Time VSHCT Bit Settings [5:3] (100 -> 1.1ms, 111 -> 8.244 ms)
  config.shunt_voltage_conversion_time = this->adc_time_current_;

  // Mode Settings [2:0] Combinations (111 -> Shunt and Bus, Continuous)
  config.mode = 0b111;

  if (!this->write_byte_16(INA226_REGISTER_CONFIG, config.raw)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // lsb is multiplied by 1000000 to store it as an integer value
  uint32_t lsb = static_cast<uint32_t>(ceilf(this->max_current_a_ * 1000000.0f / 32768));

  this->calibration_lsb_ = lsb;

  auto calibration = uint32_t(0.00512f / (lsb * this->shunt_resistance_ohm_ / 1000000.0f));

  ESP_LOGV(TAG, "    Using LSB=%" PRIu32 " calibration=%" PRIu32, lsb, calibration);

  if (!this->write_byte_16(INA226_REGISTER_CALIBRATION, calibration)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Configure Alert/Conversion Ready pin when requested
  if (this->alert_function_ != ALERT_FUNCTION_NONE || this->alert_conversion_ready_) {
    int16_t alert_limit_raw = 0;

    auto safe_round_to_int16 = [this](float value, const char *function_name, const char *unit,
                                      float scale_factor) -> int16_t {
      float rounded = value + (value >= 0.0f ? 0.5f : -0.5f);
      if (rounded < -32768.0f || rounded > 32767.0f) {
        float min_limit = -32768.0f * scale_factor;
        float max_limit = 32767.0f * scale_factor;
        ESP_LOGE(
            TAG,
            "Alert limit %.6f %s (%s) converts to register value %.2f, out of range [-32768, 32767] (%.6f to %.6f %s)",
            this->alert_limit_, unit, function_name, rounded, min_limit, max_limit, unit);
        this->mark_failed(LOG_STR("Alert limit out of range"));
        return 0;
      }
      return static_cast<int16_t>(rounded);
    };

    switch (this->alert_function_) {
      case ALERT_FUNCTION_SHUNT_OVER:
      case ALERT_FUNCTION_SHUNT_UNDER:
        alert_limit_raw = safe_round_to_int16(
            this->alert_limit_ / 0.0000025f,
            this->alert_function_ == ALERT_FUNCTION_SHUNT_OVER ? "shunt_over" : "shunt_under", "V", 0.0000025f);
        break;
      case ALERT_FUNCTION_BUS_OVER:
      case ALERT_FUNCTION_BUS_UNDER:
        alert_limit_raw = safe_round_to_int16(
            this->alert_limit_ / 0.00125f, this->alert_function_ == ALERT_FUNCTION_BUS_OVER ? "bus_over" : "bus_under",
            "V", 0.00125f);
        break;
      case ALERT_FUNCTION_POWER_OVER: {
        float current_lsb_a = this->calibration_lsb_ / 1000000.0f;
        float denom = current_lsb_a * 25.0f;
        if (denom > 0.0f) {
          alert_limit_raw = safe_round_to_int16(this->alert_limit_ / denom, "power_over", "W", denom);
        }
        break;
      }
      case ALERT_FUNCTION_NONE:
      default:
        break;
    }

    if (this->is_failed()) {
      return;
    }

    if (this->alert_function_ != ALERT_FUNCTION_NONE) {
      if (!this->write_byte_16(INA226_REGISTER_ALERT_LIMIT, static_cast<uint16_t>(alert_limit_raw))) {
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
    }

    uint16_t mask = static_cast<uint16_t>(this->alert_function_);
    if (this->alert_conversion_ready_) {
      mask |= 1 << 10;
    }
    if (this->alert_polarity_high_) {
      mask |= 1 << 1;
    }
    if (this->alert_latch_) {
      mask |= 1 << 0;
    }

    if (!this->write_byte_16(INA226_REGISTER_MASK_ENABLE, mask)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
  }
}

void INA226Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA226:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed())
    return;
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG,
                "  ADC Conversion Time Bus Voltage: %d\n"
                "  ADC Conversion Time Shunt Voltage: %d\n"
                "  ADC Averaging Samples: %d",
                INA226_ADC_TIMES[this->adc_time_voltage_ & 0b111], INA226_ADC_TIMES[this->adc_time_current_ & 0b111],
                INA226_ADC_AVG_SAMPLES[this->adc_avg_samples_ & 0b111]);

  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);

  if (this->alert_function_ != ALERT_FUNCTION_NONE || this->alert_conversion_ready_) {
    const char *alert_function_label = "None";
    switch (this->alert_function_) {
      case ALERT_FUNCTION_SHUNT_OVER:
        alert_function_label = "Shunt Over";
        break;
      case ALERT_FUNCTION_SHUNT_UNDER:
        alert_function_label = "Shunt Under";
        break;
      case ALERT_FUNCTION_BUS_OVER:
        alert_function_label = "Bus Over";
        break;
      case ALERT_FUNCTION_BUS_UNDER:
        alert_function_label = "Bus Under";
        break;
      case ALERT_FUNCTION_POWER_OVER:
        alert_function_label = "Power Over";
        break;
      case ALERT_FUNCTION_NONE:
      default:
        break;
    }

    ESP_LOGCONFIG(TAG, "  Alert Function: %s", alert_function_label);
    if (this->alert_function_ != ALERT_FUNCTION_NONE) {
      ESP_LOGCONFIG(TAG, "  Alert Limit: %.6f", this->alert_limit_);
    }
    ESP_LOGCONFIG(TAG, "  Alert Conversion Ready: %s", ONOFF(this->alert_conversion_ready_));
    ESP_LOGCONFIG(TAG, "  Alert Polarity High: %s", ONOFF(this->alert_polarity_high_));
    ESP_LOGCONFIG(TAG, "  Alert Latch Enabled: %s", ONOFF(this->alert_latch_));
  } else {
    ESP_LOGCONFIG(TAG, "  Alert: Disabled");
  }
}

void INA226Component::update() {
  if (this->bus_voltage_sensor_ != nullptr) {
    uint16_t raw_bus_voltage;
    if (!this->read_byte_16(INA226_REGISTER_BUS_VOLTAGE, &raw_bus_voltage)) {
      this->status_set_warning();
      return;
    }
    // Convert for 2's compliment and signed value (though always positive)
    float bus_voltage_v = sign_extend<16>(raw_bus_voltage);
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
    float shunt_voltage_v = sign_extend<16>(raw_shunt_voltage);
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
    float current_ma = sign_extend<16>(raw_current);
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

void INA226Component::clear_alert_flag() {
  uint16_t mask_reg;
  if (this->read_byte_16(INA226_REGISTER_MASK_ENABLE, &mask_reg)) {
    ESP_LOGI(TAG, "Alert flag cleared");
  } else {
    ESP_LOGW(TAG, "Failed to clear alert flag");
  }
}

}  // namespace esphome::ina226
