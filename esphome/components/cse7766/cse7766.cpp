#include "cse7766.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::cse7766 {

ESPHOME_LOG_TAG(TAG, "cse7766");

// The chip only sends the time since the last pulse once a cycle is longer than 1 s (in us)
static constexpr uint32_t MIN_HELD_POWER_CYCLE = 1000000;
// A gap this long between bytes starts a new frame
static constexpr uint32_t RX_RESET_TIMEOUT_MS = 500;
// Datasheet: the smallest current the chip measures
static constexpr float MIN_CURRENT_A = 0.05f;
// Energy per CF pulse is the power coefficient in uW·s; this turns pulses × coefficient into Wh
static constexpr float PULSE_COEFF_TO_WH = 1.0f / (1000000.0f * 3600.0f);

static constexpr uint8_t HEADER_OK = 0x55;
static constexpr uint8_t HEADER_NOT_CALIBRATED = 0xAA;
static constexpr uint8_t HEADER_VOLTAGE_CYCLE_EXCEEDS = 1 << 3;
static constexpr uint8_t HEADER_CURRENT_CYCLE_EXCEEDS = 1 << 2;
static constexpr uint8_t HEADER_POWER_CYCLE_EXCEEDS = 1 << 1;
static constexpr uint8_t HEADER_COEFF_STORAGE_ABNORMAL = 1 << 0;
static constexpr uint8_t HEADER2 = 0x5A;

// Adjustment byte: which registers hold a new measurement
static constexpr uint8_t ADJ_POWER = 0x10;
static constexpr uint8_t ADJ_CURRENT = 0x20;
static constexpr uint8_t ADJ_VOLTAGE = 0x40;

void CSE7766Component::loop() {
  size_t avail = this->available();
  if (avail == 0)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_transmission_ >= RX_RESET_TIMEOUT_MS)
    this->raw_data_index_ = 0;
  this->last_transmission_ = now;

  // At 4800 baud (~480 bytes/sec) with ~122 Hz loop rate, typically ~4 bytes per call
  this->read_available<RAW_DATA_SIZE>(
      [this](uint8_t byte) {
        const uint8_t index = this->raw_data_index_;
        this->raw_data_[index] = byte;
        if (!this->check_byte_(index, byte)) {
          this->raw_data_index_ = 0;
          this->status_set_warning();
          return;
        }
        if (index == RAW_DATA_SIZE - 1) {
          this->raw_data_index_ = 0;
          this->status_comm_result(this->parse_data_());
          return;
        }
        this->raw_data_index_ = index + 1;
      },
      avail);
}

bool CSE7766Component::check_byte_(uint8_t index, uint8_t byte) const {
  if (index == 0)
    return byte == HEADER_OK || high_nibble(byte) == 0xF || byte == HEADER_NOT_CALIBRATED;

  if (index == 1) {
    if (byte == HEADER2)
      return true;
    ESP_LOGV(TAG, "Invalid Header 2 Start: 0x%02X!", byte);
    return false;
  }

  if (index == RAW_DATA_SIZE - 1) {
    const uint8_t checksum = sum_bytes(this->raw_data_ + 2, RAW_DATA_SIZE - 3);
    if (checksum != byte) {
      ESP_LOGW(TAG, "Invalid checksum from CSE7766: 0x%02X != 0x%02X", checksum, byte);
      return false;
    }
  }
  return true;
}

bool CSE7766Component::parse_data_() {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  {
    char hex_buf[format_hex_pretty_size(RAW_DATA_SIZE)];
    ESP_LOGVV(TAG, "Raw data: %s", format_hex_pretty_to(hex_buf, this->raw_data_, RAW_DATA_SIZE));
  }
#endif

  const uint8_t header1 = this->raw_data_[0];
  if (header1 == HEADER_NOT_CALIBRATED) {
    ESP_LOGE(TAG, "CSE7766 not calibrated!");
    return false;
  }

  bool power_cycle_exceeds_range = false;
  // 0xFx: the low nibble holds error bits
  if (high_nibble(header1) == 0xF) {
    if (header1 & (HEADER_VOLTAGE_CYCLE_EXCEEDS | HEADER_CURRENT_CYCLE_EXCEEDS | HEADER_COEFF_STORAGE_ABNORMAL)) {
      ESP_LOGE(TAG, "CSE7766 reports abnormal external circuit or chip damage: (0x%02X)", header1);
      if (header1 & HEADER_VOLTAGE_CYCLE_EXCEEDS) {
        ESP_LOGE(TAG, "  Voltage cycle exceeds range.");
      }
      if (header1 & HEADER_CURRENT_CYCLE_EXCEEDS) {
        ESP_LOGE(TAG, "  Current cycle exceeds range.");
      }
      if (header1 & HEADER_COEFF_STORAGE_ABNORMAL) {
        ESP_LOGE(TAG, "  Coefficient storage area is abnormal.");
      }
      // Datasheet: voltage or current cycle exceeding range means invalid values
      return false;
    }
    power_cycle_exceeds_range = header1 & HEADER_POWER_CYCLE_EXCEEDS;
  }

  const uint8_t *d = this->raw_data_;
  uint32_t voltage_coeff = encode_uint24(d[2], d[3], d[4]);
  uint32_t voltage_cycle = encode_uint24(d[5], d[6], d[7]);
  uint32_t current_coeff = encode_uint24(d[8], d[9], d[10]);
  uint32_t current_cycle = encode_uint24(d[11], d[12], d[13]);
  uint32_t power_coeff = encode_uint24(d[14], d[15], d[16]);
  uint32_t power_cycle = encode_uint24(d[17], d[18], d[19]);
  uint8_t adj = d[20];
  uint16_t cf_pulses = encode_uint16(d[21], d[22]);

  bool have_power = adj & ADJ_POWER;
  bool have_current = adj & ADJ_CURRENT;
  bool have_voltage = adj & ADJ_VOLTAGE;

  float voltage = 0.0f;
  if (have_voltage) {
    voltage = voltage_coeff / float(voltage_cycle);
    PUBLISH_STATE(this->voltage_sensor_, voltage);
  }

  float energy = 0.0f;
  if (this->energy_sensor_ != nullptr) {
    if (this->cf_pulses_last_ == 0 && !this->energy_sensor_->has_state()) {
      this->cf_pulses_last_ = cf_pulses;
    }
    uint16_t cf_diff = cf_pulses - this->cf_pulses_last_;
    this->cf_pulses_total_ += cf_diff;
    this->cf_pulses_last_ = cf_pulses;
    energy = this->cf_pulses_total_ * float(power_coeff) * PULSE_COEFF_TO_WH;
    this->energy_sensor_->publish_state(energy);
  }

  float power = 0.0f;
  if (power_cycle_exceeds_range) {
    // Datasheet: power cycle exceeding range means active power is 0
    have_power = true;
    this->last_power_cycle_ = 0;
  } else if (have_power) {
    power = power_coeff / float(power_cycle);
    this->last_power_cycle_ = power_cycle;
  } else if (this->last_power_cycle_ > MIN_HELD_POWER_CYCLE && power_cycle <= 2 * this->last_power_cycle_) {
    // No new measurement in this frame. With cycles over 1 s the register holds the last
    // cycle or the time elapsed since the last pulse. While that time is within twice the
    // last cycle, the next pulse is not overdue, so the last measurement still applies.
    // Shorter cycles never get here in normal operation.
    have_power = true;
    power = power_coeff / float(this->last_power_cycle_);
  } else {
    // The next pulse is overdue: the load has dropped, report 0 W. A load that drops to less
    // than half also reads 0 W here until its first new pulse arrives.
    this->last_power_cycle_ = 0;
  }
  PUBLISH_STATE(this->power_sensor_, power);

  // Without a power measurement the current is likely below the 50 mA the chip can measure;
  // P > I_min·U is P/U > I_min without the division
  const bool current_measurable = have_power && voltage > 1.0f && power > MIN_CURRENT_A * voltage;
  float current = 0.0f;
  if (have_current) {
    if (current_measurable)
      current = current_coeff / float(current_cycle);
    PUBLISH_STATE(this->current_sensor_, current);
  }

  if (have_voltage && have_current) {
    const float apparent_power = voltage * current;
    PUBLISH_STATE(this->apparent_power_sensor_, apparent_power);
    if (have_power && this->reactive_power_sensor_ != nullptr) {
      const float q_squared = apparent_power * apparent_power - power * power;
      if (q_squared < 0.0f) {
        // From the same noise as a power factor above 1 (below)
        ESP_LOGV(TAG, "Impossible reactive power: S^2-P^2 is negative (%.4f)", q_squared);
        this->reactive_power_sensor_->publish_state(0.0f);
      } else {
        this->reactive_power_sensor_->publish_state(std::sqrt(q_squared));
      }
    }
    if (this->power_factor_sensor_ != nullptr && have_power) {
      float pf = NAN;
      if (apparent_power > 0) {
        pf = power / apparent_power;
        if (pf > 1) {
          // Noise near the chip's resolution limit pushes the ratio past unity: during inrush, and on
          // nearly every frame of a steady resistive load, so it is logged only at verbose level
          ESP_LOGV(TAG, "Power factor %.4f above 1, clamped", pf);
          pf = 1.0f;
        }
      } else if (apparent_power == 0 && power == 0) {
        // No load, report ideal power factor
        pf = 1.0f;
      } else if (current == 0 && !current_measurable) {
        ESP_LOGV(TAG, "Can't calculate power factor (current below minimum for CSE7766)");
      } else {
        ESP_LOGW(TAG, "Can't calculate power factor from P = %.4f W, S = %.4f VA", power, apparent_power);
      }
      this->power_factor_sensor_->publish_state(pf);
    }
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  {
    const float calculated_current = have_power && voltage > 1.0f ? power / voltage : 0.0f;
    char buf[128];  // five fields at %.4f fit with room to spare
    size_t pos = buf_append_str(buf, sizeof(buf), 0, "Parsed:");
    if (have_voltage) {
      pos = buf_append_printf(buf, sizeof(buf), pos, " V=%.4fV", voltage);
    }
    if (have_current) {
      pos = buf_append_printf(buf, sizeof(buf), pos, " I=%.4fmA (~%.4fmA)", current * 1000.0f,
                              calculated_current * 1000.0f);
    }
    if (have_power) {
      pos = buf_append_printf(buf, sizeof(buf), pos, " P=%.4fW", power);
    }
    if (energy != 0.0f) {
      buf_append_printf(buf, sizeof(buf), pos, " E=%.4fkWh (%u)", energy, cf_pulses);
    }
    ESP_LOGVV(TAG, "%s", buf);
  }
#endif
  return true;
}

void CSE7766Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CSE7766:");
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Energy", this->energy_sensor_);
  LOG_SENSOR("  ", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("  ", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_);
}

}  // namespace esphome::cse7766
