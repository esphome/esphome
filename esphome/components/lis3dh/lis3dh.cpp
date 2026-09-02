#include "lis3dh.h"
#include <algorithm>
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::lis3dh {

static const char *const TAG = "lis3dh";

// Acceleration sensitivity in mg per digit, indexed by [operating_mode][range].
// The "digit" is the right-justified sample, so the raw register value must be
// shifted down by RESOLUTION_SHIFT first. Values are taken from the datasheet
// (note the ±16g column does not follow the doubling pattern).
static constexpr float SENSITIVITY_MG[3][4] = {
    {16.0f, 32.0f, 64.0f, 192.0f},  // low power (8-bit)
    {4.0f, 8.0f, 16.0f, 48.0f},     // normal (10-bit)
    {1.0f, 2.0f, 4.0f, 12.0f},      // high resolution (12-bit)
};

// Number of unused low bits in the left-justified 16-bit sample, per mode.
static constexpr uint8_t RESOLUTION_SHIFT[3] = {8, 6, 4};

// Motion interrupt threshold LSB in mg, indexed by range.
static constexpr float THRESHOLD_LSB_MG[4] = {16.0f, 32.0f, 62.0f, 186.0f};

void LIS3DHComponent::setup() {
  MotionComponent::setup();

  uint8_t who_am_i = 0;
  if (!this->read_byte(LIS3DH_REG_WHO_AM_I, &who_am_i)) {
    ESP_LOGE(TAG, "Failed to read chip ID - check wiring / address");
    this->mark_failed();
    return;
  }
  if (who_am_i != LIS3DH_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "Wrong chip ID: 0x%02X (expected 0x%02X)", who_am_i, LIS3DH_WHO_AM_I_VALUE);
    this->mark_failed();
    return;
  }

  if (!this->setup_accelerometer_() || !this->setup_temperature_()) {
    ESP_LOGE(TAG, "Failed to configure sensor");
    this->mark_failed();
    return;
  }
  if (this->interrupt_enabled_ && !this->setup_interrupt_()) {
    ESP_LOGE(TAG, "Failed to configure motion interrupt");
    this->mark_failed();
    return;
  }
}

bool LIS3DHComponent::setup_accelerometer_() {
  // CTRL_REG1: output data rate, low-power flag and axis enable.
  uint8_t ctrl_reg1 = ((uint8_t) this->data_rate_ << 4) | LIS3DH_CTRL_REG1_AXES_EN;
  if (this->operating_mode_ == LIS3DH_MODE_LOW_POWER)
    ctrl_reg1 |= LIS3DH_CTRL_REG1_LPEN;
  if (!this->write_byte(LIS3DH_REG_CTRL_REG1, ctrl_reg1))
    return false;

  // CTRL_REG4: block data update, full-scale range and high-resolution flag.
  uint8_t ctrl_reg4 = LIS3DH_CTRL_REG4_BDU | ((uint8_t) this->range_ << 4);
  if (this->operating_mode_ == LIS3DH_MODE_HIGH_RESOLUTION)
    ctrl_reg4 |= LIS3DH_CTRL_REG4_HR;
  return this->write_byte(LIS3DH_REG_CTRL_REG4, ctrl_reg4);
}

bool LIS3DHComponent::setup_temperature_() {
  // Only power the ADC / temperature sensor when a temperature sensor is used.
  uint8_t temp_cfg =
      this->temperature_callback_.empty() ? 0x00 : (uint8_t) (LIS3DH_TEMP_CFG_ADC_EN | LIS3DH_TEMP_CFG_TEMP_EN);
  return this->write_byte(LIS3DH_REG_TEMP_CFG, temp_cfg);
}

bool LIS3DHComponent::setup_interrupt_() {
  // High-pass filter the interrupt (IA1) input so the ~1 g of gravity is removed
  // from the threshold comparison. The output data registers stay unfiltered
  // (FDS = 0), so acceleration / pitch / roll still report the full gravity vector.
  uint8_t ctrl_reg2 = this->interrupt_high_pass_ ? LIS3DH_CTRL_REG2_HP_IA1 : 0x00;
  if (!this->write_byte(LIS3DH_REG_CTRL_REG2, ctrl_reg2))
    return false;

  // Convert the threshold from g to the 7-bit INT1_THS value for the active range.
  // A threshold of zero is treated as always exceeded, which would hold the pad
  // asserted, so keep the smallest step the range can express.
  int32_t raw = lroundf(this->interrupt_threshold_g_ * 1000.0f / THRESHOLD_LSB_MG[this->range_]);
  uint8_t threshold = (uint8_t) std::clamp<int32_t>(raw, 1, 0x7F);
  this->interrupt_threshold_raw_ = threshold;

  if (!this->write_byte(LIS3DH_REG_INT1_CFG, this->interrupt_axes_cfg_) ||
      !this->write_byte(LIS3DH_REG_INT1_THS, threshold) ||
      !this->write_byte(LIS3DH_REG_INT1_DURATION, this->interrupt_duration_))
    return false;

  // Latch the request so a brief movement keeps the pad asserted until INT1_SRC is read.
  uint8_t ctrl_reg5 = this->interrupt_latched_ ? LIS3DH_CTRL_REG5_LIR_INT1 : 0;
  if (!this->write_byte(LIS3DH_REG_CTRL_REG5, ctrl_reg5))
    return false;

  // Route the IA1 engine to the chosen physical pad and set the pad polarity.
  uint8_t ctrl_reg3 = 0;
  uint8_t ctrl_reg6 = 0;
  if (this->interrupt_pin_ == LIS3DH_INT_PIN_INT1) {
    ctrl_reg3 |= LIS3DH_CTRL_REG3_I1_IA1;
  } else {
    ctrl_reg6 |= LIS3DH_CTRL_REG6_I2_IA1;
  }
  if (!this->interrupt_active_high_)
    ctrl_reg6 |= LIS3DH_CTRL_REG6_INT_POLARITY;
  if (!this->write_byte(LIS3DH_REG_CTRL_REG3, ctrl_reg3) || !this->write_byte(LIS3DH_REG_CTRL_REG6, ctrl_reg6))
    return false;

  // With the high-pass filter enabled, reading REFERENCE captures the current
  // acceleration as the filter's zero point (removing the standing gravity bias).
  if (this->interrupt_high_pass_) {
    uint8_t reference = 0;
    this->read_byte(LIS3DH_REG_REFERENCE, &reference);
  }

  // Clear any latched request left over from the motion that woke the device.
  uint8_t src = 0;
  this->read_byte(LIS3DH_REG_INT1_SRC, &src);
  return true;
}

void LIS3DHComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LIS3DH:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  static constexpr const char *const RANGE_STRS[] = {"±2g", "±4g", "±8g", "±16g"};
  static constexpr const char *const MODE_STRS[] = {"low power (8-bit)", "normal (10-bit)", "high resolution (12-bit)"};
  static constexpr const char *const RATE_STRS[] = {"powered down", "1Hz",   "10Hz",  "25Hz",   "50Hz",
                                                    "100Hz",        "200Hz", "400Hz", "1620Hz", "1344Hz"};
  ESP_LOGCONFIG(TAG,
                "  Range: %s\n"
                "  Operating mode: %s\n"
                "  Data rate: %s",
                RANGE_STRS[this->range_], MODE_STRS[this->operating_mode_], RATE_STRS[this->data_rate_]);
  if (this->interrupt_enabled_) {
    // The threshold is reported as written, since converting it from g depends
    // on the range and rounds.
    ESP_LOGCONFIG(TAG,
                  "  Motion interrupt: %s (%s%s%s)\n"
                  "    Threshold: %.3fg (%u)\n"
                  "    Duration: %u samples",
                  this->interrupt_pin_ == LIS3DH_INT_PIN_INT1 ? LOG_STR_LITERAL("INT1") : LOG_STR_LITERAL("INT2"),
                  this->interrupt_active_high_ ? LOG_STR_LITERAL("active high") : LOG_STR_LITERAL("active low"),
                  this->interrupt_high_pass_ ? LOG_STR_LITERAL(", high-pass") : LOG_STR_LITERAL(""),
                  this->interrupt_latched_ ? LOG_STR_LITERAL(", latched") : LOG_STR_LITERAL(""),
                  this->interrupt_threshold_g_, this->interrupt_threshold_raw_, this->interrupt_duration_);
  }
  MotionComponent::dump_config();
}

bool LIS3DHComponent::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  uint8_t raw[6];
  if (!this->read_bytes(LIS3DH_REG_OUT_X_L | LIS3DH_AUTO_INCREMENT, raw, 6)) {
    ESP_LOGW(TAG, "Failed to read acceleration data");
    return false;
  }

  const uint8_t shift = RESOLUTION_SHIFT[this->operating_mode_];
  const float scale = SENSITIVITY_MG[this->operating_mode_][this->range_] / 1000.0f;  // g per digit

  // Data is little-endian and left-justified in a signed 16-bit register.
  int16_t raw_x = (int16_t) encode_uint16(raw[1], raw[0]);
  int16_t raw_y = (int16_t) encode_uint16(raw[3], raw[2]);
  int16_t raw_z = (int16_t) encode_uint16(raw[5], raw[4]);
  ESP_LOGV(TAG, "Read raw accel data: %d, %d, %d", raw_x, raw_y, raw_z);
  data.acceleration[motion::X_AXIS] = (raw_x >> shift) * scale;
  data.acceleration[motion::Y_AXIS] = (raw_y >> shift) * scale;
  data.acceleration[motion::Z_AXIS] = (raw_z >> shift) * scale;

  if (!this->temperature_callback_.empty())
    this->publish_temperature_();
  return true;
}

void LIS3DHComponent::publish_temperature_() {
  uint8_t raw[2];
  if (!this->read_bytes(LIS3DH_REG_OUT_ADC3_L | LIS3DH_AUTO_INCREMENT, raw, 2)) {
    ESP_LOGW(TAG, "Failed to read temperature data");
    return;
  }
  // The temperature occupies the high byte as a signed, relative value (~1 °C per count).
  float temperature = (int8_t) raw[1] + LIS3DH_TEMP_REFERENCE_C;
  this->temperature_callback_.call(temperature);
}

}  // namespace esphome::lis3dh
