#include "hlw8032.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <iomanip>

namespace esphome {
namespace hlw8032 {

static const char *const TAG = "hlw8032";

void HLW8032Component::loop() {
  if (!this->available())
    return;

  uint8_t data = this->read();

  if (((data == 0x55) || (data == 0xaa) || (data & 0xf0)) && !this->header_found_) {
    this->header_found_ = true;
    this->raw_data_[0] = data;
  } else if (data == 0x5A && this->header_found_) {
    this->raw_data_[1] = data;
    this->raw_data_index_ = 2;
    this->check_ = 0;
  } else if (this->raw_data_index_ >= 2 && this->raw_data_index_ < 24) {
    this->raw_data_[this->raw_data_index_] = data;
    if (this->raw_data_index_ < 23) {
      this->check_ += data;
    }
    this->raw_data_index_++;
    if (this->raw_data_index_ == 24) {
      if (this->check_ == this->raw_data_[23]) {
        this->parse_data_();
      } else
        ESP_LOGW(TAG, "Invalid checksum: 0x%02X != 0x%02X", this->check_, this->raw_data_[23]);

      this->raw_data_index_ = 0;
      this->header_found_ = false;
      memset(this->raw_data_, 0, 24);
    }
  }
}

void HLW8032Component::parse_data_() {
  // Parse header
  uint8_t state_reg = this->raw_data_[0];

  if (state_reg == 0xAA) {
    ESP_LOGE(TAG, "Function of error correction fails.");
    return;
  }

  // Parse data frame
  uint32_t voltage_parameter = encode_uint24(this->raw_data_[2], this->raw_data_[3], this->raw_data_[4]);
  uint32_t voltage_reg = encode_uint24(this->raw_data_[5], this->raw_data_[6], this->raw_data_[7]);
  uint32_t current_parameter = encode_uint24(this->raw_data_[8], this->raw_data_[9], this->raw_data_[10]);
  uint32_t current_reg = encode_uint24(this->raw_data_[11], this->raw_data_[12], this->raw_data_[13]);
  uint32_t power_parameter = encode_uint24(this->raw_data_[14], this->raw_data_[15], this->raw_data_[16]);
  uint32_t power_reg = encode_uint24(this->raw_data_[17], this->raw_data_[18], this->raw_data_[19]);

  uint8_t data_update_register = this->raw_data_[20];

  bool have_power = data_update_register & (1 << 4);
  bool have_current = data_update_register & (1 << 5);
  bool have_voltage = data_update_register & (1 << 6);

  bool power_cycle_exceeds_range = false;
  bool parameter_regs_usable = true;

  if ((state_reg & 0xF0) == 0xF0) {
    if (state_reg & 0xF) {
      if (state_reg & (1 << 3)) {
        have_voltage = false;
      }
      if (state_reg & (1 << 2)) {
        have_current = false;
      }
      if (state_reg & (1 << 1)) {
        have_power = false;
      }
      if (state_reg & (1 << 0)) {
        parameter_regs_usable = false;       
      }
      
      ESP_LOGW(TAG,
            "Reports: (0x%02X)"
            "%s%s%s%s",
            state_reg,
            !have_voltage ? "\n  Voltage REG overflows." : "",
            !have_current ? "\n  Current REG overflows." : "",
            !have_power ? "\n  Power REG overflows." : "",
            !parameter_regs_usable ? "\n  Voltage/Current/Power Parameter REGs not usable." : "");
      
      if (!parameter_regs_usable) {
        return;
      }
    }
    power_cycle_exceeds_range = state_reg & (1 << 1);
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  ESP_LOGVV(TAG, 
            "Parsed data:\n"
            "  Voltage: Parameter REG 0x%06" PRIX32 ", REG 0x%06" PRIX32 "\n"
            "  Current: Parameter REG 0x%06" PRIX32 ", REG 0x%06" PRIX32 "\n"
            "  Power: Parameter REG 0x%06" PRIX32 ", REG 0x%06" PRIX32 "\n"
            "  Data Update: REG 0x%02" PRIX8 "\n",
            voltage_parameter, voltage_reg, current_parameter, current_reg, power_parameter, power_reg,
            data_update_register);
#endif

  const float current_multiplier = 1 / (this->current_resistor_ * 1000);

  float voltage = 0.0f;
  if (have_voltage) {
    voltage = float(voltage_parameter) * this->voltage_divider_ / float(voltage_reg);
    if (this->voltage_sensor_ != nullptr) {
      this->voltage_sensor_->publish_state(voltage);
    }
  }

  float power = 0.0f;
  if (power_cycle_exceeds_range) {
    // Datasheet: power cycle exceeding range means active power is 0
    if (this->power_sensor_ != nullptr) {
      this->power_sensor_->publish_state(0.0f);
    }
  } else if (have_power) {
    power = (float(power_parameter) / float(power_reg)) * this->voltage_divider_ * current_multiplier;
    if (this->power_sensor_ != nullptr) {
      this->power_sensor_->publish_state(power);
    }
  }

  float current = 0.0f;
  if (have_current) {
    current = float(current_parameter) * current_multiplier / float(current_reg);
    if (this->current_sensor_ != nullptr) {
      this->current_sensor_->publish_state(current);
    }
  }

  if (have_voltage && have_current) {
    const float apparent_power = voltage * current;
    if (this->apparent_power_sensor_ != nullptr) {
      this->apparent_power_sensor_->publish_state(apparent_power);
    }
    if (this->power_factor_sensor_ != nullptr && (have_power || power_cycle_exceeds_range)) {
      float pf = NAN;
      if (apparent_power > 0) {
        pf = power / apparent_power;
        if (pf < 0 || pf > 1) {
          ESP_LOGD(TAG, "Impossible power factor: %.4f not in interval [0, 1]", pf);
          pf = NAN;
        }
      } else if (apparent_power == 0 && power == 0) {
        // No load, report ideal power factor
        pf = 1.0f;
      }
      this->power_factor_sensor_->publish_state(pf);
    }
  }
}

void HLW8032Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Configuration:\n"
                "  Current resistor: %.1f mΩ\n"
                "  Voltage Divider: %.3f",
                this->current_resistor_ * 1000.0f, this->voltage_divider_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_)
  LOG_SENSOR("  ", "Current", this->current_sensor_)
  LOG_SENSOR("  ", "Power", this->power_sensor_)
  LOG_SENSOR("  ", "Apparent Power", this->apparent_power_sensor_)
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_)
}
}  // namespace hlw8032
}  // namespace esphome
