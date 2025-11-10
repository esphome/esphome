#include "hlw8032.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::hlw8032 {

static const char *const TAG = "hlw8032";

static constexpr uint8_t STATE_REG_OFFSET = 0;
static constexpr uint8_t VOLTAGE_PARAM_OFFSET = 2;
static constexpr uint8_t VOLTAGE_REG_OFFSET = 5;
static constexpr uint8_t CURRENT_PARAM_OFFSET = 8;
static constexpr uint8_t CURRENT_REG_OFFSET = 11;
static constexpr uint8_t POWER_PARAM_OFFSET = 14;
static constexpr uint8_t POWER_REG_OFFSET = 17;
static constexpr uint8_t DATA_UPDATE_REG_OFFSET = 20;
static constexpr uint8_t CHECKSUM_REG_OFFSET = 23;
static constexpr uint8_t PARAM_REG_USABLE_BIT = (1 << 0);
static constexpr uint8_t POWER_OVERFLOW_BIT = (1 << 1);
static constexpr uint8_t CURRENT_OVERFLOW_BIT = (1 << 2);
static constexpr uint8_t VOLTAGE_OVERFLOW_BIT = (1 << 3);
static constexpr uint8_t HAVE_POWER_BIT = (1 << 4);
static constexpr uint8_t HAVE_CURRENT_BIT = (1 << 5);
static constexpr uint8_t HAVE_VOLTAGE_BIT = (1 << 6);
static constexpr uint8_t CHECK_REG = 0x5A;
static constexpr uint8_t STATE_REG_CORRECTION_FUNC_NORMAL = 0x55;
static constexpr uint8_t STATE_REG_CORRECTION_FUNC_FAIL = 0xAA;
static constexpr uint8_t STATE_REG_CORRECTION_MASK = 0xF0;
static constexpr uint8_t STATE_REG_OVERFLOW_MASK = 0xF;
static constexpr uint8_t PACKET_LENGTH = 24;

void HLW8032Component::loop() {
  while (this->available()) {
    uint8_t data = this->read();
    if (!this->header_found_) {
      if ((data == STATE_REG_CORRECTION_FUNC_NORMAL) || (data == STATE_REG_CORRECTION_FUNC_FAIL) ||
          (data & STATE_REG_CORRECTION_MASK) == STATE_REG_CORRECTION_MASK) {
        this->header_found_ = true;
        this->raw_data_[0] = data;
      }
    } else if (data == CHECK_REG) {
      this->raw_data_[1] = data;
      this->raw_data_index_ = 2;
      this->check_ = 0;
    } else if (this->raw_data_index_ >= 2 && this->raw_data_index_ < PACKET_LENGTH) {
      this->raw_data_[this->raw_data_index_++] = data;
      if (this->raw_data_index_ < PACKET_LENGTH) {
        this->check_ += data;
      } else if (this->raw_data_index_ == PACKET_LENGTH) {
        if (this->check_ == this->raw_data_[CHECKSUM_REG_OFFSET]) {
          this->parse_data_();
        } else {
          ESP_LOGW(TAG, "Invalid checksum: 0x%02X != 0x%02X", this->check_, this->raw_data_[CHECKSUM_REG_OFFSET]);
        }
        this->raw_data_index_ = 0;
        this->header_found_ = false;
        memset(this->raw_data_, 0, PACKET_LENGTH);
      }
    }
  }
}

uint32_t HLW8032Component::read_uint24_(uint8_t offset) {
  return encode_uint24(this->raw_data_[offset], this->raw_data_[offset + 1], this->raw_data_[offset + 2]);
}

void HLW8032Component::parse_data_() {
  // Parse header
  uint8_t state_reg = this->raw_data_[STATE_REG_OFFSET];

  if (state_reg == STATE_REG_CORRECTION_FUNC_FAIL) {
    ESP_LOGE(TAG, "The chip's function of error correction fails.");
    return;
  }

  // Parse data frame
  uint32_t voltage_parameter = this->read_uint24_(VOLTAGE_PARAM_OFFSET);
  uint32_t voltage_reg = this->read_uint24_(VOLTAGE_REG_OFFSET);
  uint32_t current_parameter = this->read_uint24_(CURRENT_PARAM_OFFSET);
  uint32_t current_reg = this->read_uint24_(CURRENT_REG_OFFSET);
  uint32_t power_parameter = this->read_uint24_(POWER_PARAM_OFFSET);
  uint32_t power_reg = this->read_uint24_(POWER_REG_OFFSET);
  uint8_t data_update_register = this->raw_data_[DATA_UPDATE_REG_OFFSET];

  bool have_power = data_update_register & HAVE_POWER_BIT;
  bool have_current = data_update_register & HAVE_CURRENT_BIT;
  bool have_voltage = data_update_register & HAVE_VOLTAGE_BIT;

  bool power_cycle_exceeds_range = false;
  bool parameter_regs_usable = true;

  if ((state_reg & STATE_REG_CORRECTION_MASK) == STATE_REG_CORRECTION_MASK) {
    if (state_reg & STATE_REG_OVERFLOW_MASK) {
      if (state_reg & VOLTAGE_OVERFLOW_BIT) {
        have_voltage = false;
      }
      if (state_reg & CURRENT_OVERFLOW_BIT) {
        have_current = false;
      }
      if (state_reg & POWER_OVERFLOW_BIT) {
        have_power = false;
      }
      if (state_reg & PARAM_REG_USABLE_BIT) {
        parameter_regs_usable = false;
      }

      ESP_LOGW(TAG,
               "Reports: (0x%02X)\n"
               "  Voltage REG overflows: %s\n"
               "  Current REG overflows: %s\n"
               "  Power REG overflows: %s\n"
               "  Voltage/Current/Power Parameter REGs not usable: %s\n",
               state_reg, YESNO(!have_voltage), YESNO(!have_current), YESNO(!have_power),
               YESNO(!parameter_regs_usable));

      if (!parameter_regs_usable) {
        return;
      }
    }
    power_cycle_exceeds_range = have_power;
  }

  ESP_LOGVV(TAG,
            "Parsed data:\n"
            "  Voltage: Parameter REG 0x%06" PRIX32 ", REG 0x%06" PRIX32 "\n"
            "  Current: Parameter REG 0x%06" PRIX32 ", REG 0x%06" PRIX32 "\n"
            "  Power: Parameter REG 0x%06" PRIX32 ", REG 0x%06" PRIX32 "\n"
            "  Data Update: REG 0x%02" PRIX8 "\n",
            voltage_parameter, voltage_reg, current_parameter, current_reg, power_parameter, power_reg,
            data_update_register);

  const float current_multiplier = 1 / (this->current_resistor_ * 1000);

  float voltage = 0.0f;
  if (have_voltage && voltage_reg) {
    voltage = float(voltage_parameter) * this->voltage_divider_ / float(voltage_reg);
  }
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(voltage);
  }

  float power = 0.0f;
  if (have_power && power_reg && !power_cycle_exceeds_range) {
    power = (float(power_parameter) / float(power_reg)) * this->voltage_divider_ * current_multiplier;
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(power);
  }

  float current = 0.0f;
  if (have_current && current_reg) {
    current = float(current_parameter) * current_multiplier / float(current_reg);
  }
  if (this->current_sensor_ != nullptr) {
    this->current_sensor_->publish_state(current);
  }

  float pf = NAN;
  const float apparent_power = voltage * current;
  if (have_voltage && have_current) {
    if (have_power || power_cycle_exceeds_range) {
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
    }
  }
  if (this->apparent_power_sensor_ != nullptr) {
    this->apparent_power_sensor_->publish_state(apparent_power);
  }
  if (this->power_factor_sensor_ != nullptr) {
    this->power_factor_sensor_->publish_state(pf);
  }
}

void HLW8032Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Configuration:\n"
                "  Current resistor: %.1f mΩ\n"
                "  Voltage Divider: %.3f",
                this->current_resistor_ * 1000.0f, this->voltage_divider_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_);
}
}  // namespace esphome::hlw8032
