#include "bh1745.h"
#include <cinttypes>
#include <cmath>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::bh1745 {

ESPHOME_LOG_TAG(TAG, "bh1745");

static constexpr uint8_t REG_SYSTEM_CONTROL = 0x40;
static constexpr uint8_t REG_MODE_CONTROL1 = 0x41;
static constexpr uint8_t REG_MODE_CONTROL2 = 0x42;
static constexpr uint8_t REG_MODE_CONTROL3 = 0x44;
static constexpr uint8_t REG_RED_DATA_LSB = 0x50;
static constexpr uint8_t REG_INTERRUPT = 0x60;
static constexpr uint8_t REG_TH_LSB = 0x62;
static constexpr uint8_t REG_MANUFACTURER_ID = 0x92;

static constexpr uint8_t MANUFACTURER_ID = 0xE0;
static constexpr uint8_t PART_ID = 0b001011;
static constexpr uint8_t SYSTEM_CONTROL_PART_ID_MASK = 0x3F;
static constexpr uint8_t SYSTEM_CONTROL_SW_RESET = 0x80;
static constexpr uint8_t MODE_CONTROL2_RGBC_EN = 0x10;
static constexpr uint8_t MODE_CONTROL2_VALID = 0x80;
static constexpr uint8_t MODE_CONTROL3_VALUE = 0x02;  // the datasheet says always write 0x02
static constexpr uint8_t INTERRUPT_ENABLE = 0x01;

static constexpr uint8_t BH1745_CHANNELS = 4;
static constexpr uint32_t RESET_DELAY_MS = 100;
static constexpr uint32_t BASE_MEAS_TIME_MS = 160;

static uint32_t get_measurement_time_ms(MeasurementTime time) {
  return BASE_MEAS_TIME_MS << static_cast<uint8_t>(time);
}

static uint8_t get_adc_gain(AdcGain gain) {
  return gain == AdcGain::ADC_GAIN_16X ? 16 : 1 + static_cast<uint8_t>(gain);
}

static float calculate_lux(uint16_t red, uint16_t green, bool low_clear_ratio, float scale) {
  if (green == 0)
    return 0.0f;
  float lx = low_clear_ratio ? 0.202f * red + 0.766f * green : 0.159f * red + 0.646f * green;
  return lx * scale;
}

static float calculate_cct(uint16_t red, uint16_t green, uint16_t blue, bool low_clear_ratio) {
  if (green == 0)
    return 0.0f;
  float all = static_cast<float>(static_cast<uint32_t>(red) + green + blue);
  float r_ratio = red / all;
  float b_ratio = blue / all;
  float ct;
  if (low_clear_ratio) {
    float b_eff = std::fmin(b_ratio * 3.13f, 1.0f);
    ct = ((1.0f - b_eff) * 12746.0f * std::exp(-2.911f * r_ratio)) + (b_eff * 1637.0f * std::exp(4.865f * b_ratio));
  } else {
    float b_eff = std::fmin(b_ratio * 10.67f, 1.0f);
    ct = ((1.0f - b_eff) * 16234.0f * std::exp(-2.781f * r_ratio)) + (b_eff * 1882.0f * std::exp(4.448f * b_ratio));
  }
  return clamp_at_most(ct, 10000.0f);
}

void BH1745Component::setup() {
  if (!this->check_chip_id(this, REG_MANUFACTURER_ID, MANUFACTURER_ID))
    return;
  uint8_t system_control;
  if (!this->read_byte(REG_SYSTEM_CONTROL, &system_control) ||
      (system_control & SYSTEM_CONTROL_PART_ID_MASK) != PART_ID) {
    this->mark_failed(LOG_STR("Wrong part ID"));
    return;
  }
  if (!this->write_byte(REG_SYSTEM_CONTROL, SYSTEM_CONTROL_SW_RESET)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Light lost behind glass is scaled back up, as the other light sensors do
  this->lux_scale_ = 160.0f * this->glass_attenuation_factor_ /
                     (get_adc_gain(this->adc_gain_) * get_measurement_time_ms(this->measurement_time_));

  this->set_timeout(RESET_DELAY_MS, [this]() {
    if (this->interrupt_output_) {
      // High threshold 0 and low threshold 0xFFFF (low byte first) make the interrupt condition always
      // true, so the INT pin simply follows the enable bit, which drives the breakout's LEDs.
      static constexpr uint8_t THRESHOLDS[4] = {0x00, 0x00, 0xFF, 0xFF};
      if (!this->write_bytes(REG_TH_LSB, THRESHOLDS, sizeof(THRESHOLDS))) {
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
    }
    // RGBC_EN makes the chip measure continuously; update() reads the latest result.
    if (!this->write_byte(REG_MODE_CONTROL1, static_cast<uint8_t>(this->measurement_time_)) ||
        !this->write_byte(REG_MODE_CONTROL2, static_cast<uint8_t>(this->adc_gain_) | MODE_CONTROL2_RGBC_EN) ||
        !this->write_byte(REG_MODE_CONTROL3, MODE_CONTROL3_VALUE)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    this->busy_ = false;
  });
}

void BH1745Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BH1745:\n"
                "  Gain: %ux\n"
                "  Integration time: %" PRIu32 " ms\n"
                "  Glass attenuation factor: %.2f",
                get_adc_gain(this->adc_gain_), get_measurement_time_ms(this->measurement_time_),
                this->glass_attenuation_factor_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Red Counts", this->red_counts_sensor_);
  LOG_SENSOR("  ", "Green Counts", this->green_counts_sensor_);
  LOG_SENSOR("  ", "Blue Counts", this->blue_counts_sensor_);
  LOG_SENSOR("  ", "Clear Counts", this->clear_counts_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Color Temperature", this->color_temperature_sensor_);
}

void BH1745Component::update() {
  if (this->busy_)
    return;

  // Reading MODE_CONTROL2 clears VALID, so read it only once per poll.
  uint8_t mode_control2;
  if (!this->read_byte(REG_MODE_CONTROL2, &mode_control2)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if ((mode_control2 & MODE_CONTROL2_VALID) == 0)
    return;

  uint16_t buffer[BH1745_CHANNELS];
  if (!this->status_comm_result(this->read_bytes_16_le(REG_RED_DATA_LSB, buffer, BH1745_CHANNELS)))
    return;

  const uint16_t red = buffer[0];
  const uint16_t green = buffer[1];
  const uint16_t blue = buffer[2];
  const uint16_t clear = buffer[3];
  ESP_LOGV(TAG, "Red: %u, Green: %u, Blue: %u, Clear: %u", red, green, blue, clear);

  if (this->red_counts_sensor_ != nullptr)
    this->red_counts_sensor_->publish_state(red);
  if (this->green_counts_sensor_ != nullptr)
    this->green_counts_sensor_->publish_state(green);
  if (this->blue_counts_sensor_ != nullptr)
    this->blue_counts_sensor_->publish_state(blue);
  if (this->clear_counts_sensor_ != nullptr)
    this->clear_counts_sensor_->publish_state(clear);

  // The datasheet picks its coefficients by whether clear / green < 0.160 (= 4 / 25).
  const bool low_clear_ratio = static_cast<uint32_t>(clear) * 25 < static_cast<uint32_t>(green) * 4;
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(calculate_lux(red, green, low_clear_ratio, this->lux_scale_));
  if (this->color_temperature_sensor_ != nullptr)
    this->color_temperature_sensor_->publish_state(calculate_cct(red, green, blue, low_clear_ratio));
}

void BH1745Component::set_interrupt_state(bool on) {
  if (this->is_failed())
    return;
  if (!this->write_byte(REG_INTERRUPT, on ? INTERRUPT_ENABLE : 0x00)) {
    this->status_set_warning();
  }
}

}  // namespace esphome::bh1745
