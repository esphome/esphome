#include "opt4048.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::opt4048 {

static const char *const TAG = "opt4048";

static const uint8_t OPT4048_REG_CH0_MSB = 0x00;
static const uint8_t OPT4048_REG_THRESHOLD_LOW = 0x08;
static const uint8_t OPT4048_REG_THRESHOLD_HIGH = 0x09;
static const uint8_t OPT4048_REG_CONFIG = 0x0A;
static const uint8_t OPT4048_REG_THRESHOLD_CFG = 0x0B;
static const uint8_t OPT4048_REG_STATUS = 0x0C;
static const uint8_t OPT4048_REG_DEVICE_ID = 0x11;

static const uint16_t OPT4048_DEVICE_ID = 0x0821;

static const uint8_t OPT4048_FLAG_LOW = 0x01;
static const uint8_t OPT4048_FLAG_HIGH = 0x02;
static const uint8_t OPT4048_FLAG_CONVERSION_READY = 0x04;
static const uint8_t OPT4048_FLAG_OVERLOAD = 0x08;

// TI datasheet conversion matrix: [ch0..ch3] · M = [X Y Z lux]
static const double M0X = 2.34892992e-04;
static const double M0Y = -1.89652390e-05;
static const double M0Z = 1.20811684e-05;
static const double M1X = 4.07467441e-05;
static const double M1Y = 1.98958202e-04;
static const double M1Z = -1.58848115e-05;
static const double M1L = 2.15e-3;
static const double M2X = 9.28619404e-05;
static const double M2Y = -1.69739553e-05;
static const double M2Z = 6.74021520e-04;

static const uint32_t CONVERSION_TIME_US[] = {600,   1000,  1800,   3400,   6500,   12700,
                                              25000, 50000, 100000, 200000, 400000, 800000};

float OPT4048Component::get_setup_priority() const { return setup_priority::DATA; }

bool OPT4048Component::read_u16_(uint8_t a_register, uint16_t &value) {
  uint8_t buffer[2];
  if (this->read_register(a_register, buffer, 2) != i2c::ERROR_OK)
    return false;
  value = (uint16_t(buffer[0]) << 8) | buffer[1];
  return true;
}

bool OPT4048Component::write_u16_(uint8_t a_register, uint16_t value) {
  uint8_t buffer[2] = {uint8_t(value >> 8), uint8_t(value)};
  return this->write_register(a_register, buffer, 2) == i2c::ERROR_OK;
}

uint32_t OPT4048Component::conversion_timeout_ms_() const {
  const uint8_t index = static_cast<uint8_t>(this->conversion_time_);
  const uint32_t per_channel_us = CONVERSION_TIME_US[index];
  return (4 * per_channel_us) / 1000 + 25;
}

bool OPT4048Component::write_config_() {
  uint16_t config = 0;
  if (this->quick_wake_)
    config |= 1 << 15;
  config |= (static_cast<uint16_t>(this->range_) & 0x0F) << 10;
  config |= (static_cast<uint16_t>(this->conversion_time_) & 0x0F) << 6;
  config |= (static_cast<uint16_t>(this->mode_) & 0x03) << 4;
  if (this->interrupt_latch_)
    config |= 1 << 3;
  if (this->interrupt_polarity_high_)
    config |= 1 << 2;
  config |= static_cast<uint16_t>(this->fault_count_) & 0x03;
  return this->write_u16_(OPT4048_REG_CONFIG, config);
}

bool OPT4048Component::write_threshold_cfg_() {
  uint16_t value = 0;
  value |= (uint16_t(this->threshold_channel_) & 0x03) << 5;
  if (this->interrupt_direction_high_)
    value |= 1 << 4;
  value |= (static_cast<uint16_t>(this->interrupt_config_) & 0x03) << 2;
  return this->write_u16_(OPT4048_REG_THRESHOLD_CFG, value);
}

bool OPT4048Component::write_threshold_(uint8_t a_register, uint32_t adc_code) {
  // Datasheet: ADC_CODES = THRESHOLD_RESULT << (8 + THRESHOLD_EXPONENT)
  uint8_t exponent = 0;
  uint32_t mantissa = adc_code >> 8;
  while (mantissa > 0x0FFF && exponent < 15) {
    mantissa >>= 1;
    exponent++;
  }
  if (mantissa > 0x0FFF)
    mantissa = 0x0FFF;
  const uint16_t encoded = (uint16_t(exponent) << 12) | uint16_t(mantissa);
  return this->write_u16_(a_register, encoded);
}

bool OPT4048Component::crc_ok_(uint8_t exp, uint32_t mantissa, uint8_t counter, uint8_t crc) const {
  uint8_t x0 = 0;
  for (uint8_t i = 0; i < 4; i++)
    x0 ^= (exp >> i) & 1;
  for (uint8_t i = 0; i < 20; i++)
    x0 ^= (mantissa >> i) & 1;
  for (uint8_t i = 0; i < 4; i++)
    x0 ^= (counter >> i) & 1;

  uint8_t x1 = ((counter >> 1) & 1) ^ ((counter >> 3) & 1);
  for (uint8_t i = 1; i < 20; i += 2)
    x1 ^= (mantissa >> i) & 1;
  x1 ^= (exp >> 1) & 1;
  x1 ^= (exp >> 3) & 1;

  uint8_t x2 = (counter >> 3) & 1;
  for (uint8_t i = 3; i < 20; i += 4)
    x2 ^= (mantissa >> i) & 1;
  x2 ^= (exp >> 3) & 1;

  const uint8_t x3 = ((mantissa >> 3) & 1) ^ ((mantissa >> 11) & 1) ^ ((mantissa >> 19) & 1);
  const uint8_t calculated = (x3 << 3) | (x2 << 2) | (x1 << 1) | x0;
  return calculated == crc;
}

bool OPT4048Component::read_channels_(uint32_t channels[4]) {
  uint8_t buffer[16];
  if (this->read_register(OPT4048_REG_CH0_MSB, buffer, sizeof(buffer)) != i2c::ERROR_OK)
    return false;

  for (uint8_t ch = 0; ch < 4; ch++) {
    const uint8_t *frame = &buffer[4 * ch];
    const uint8_t exp = frame[0] >> 4;
    const uint32_t mantissa = (uint32_t(frame[0] & 0x0F) << 16) | (uint32_t(frame[1]) << 8) | frame[2];
    const uint8_t counter = frame[3] >> 4;
    const uint8_t crc = frame[3] & 0x0F;
    if (!this->crc_ok_(exp, mantissa, counter, crc)) {
      ESP_LOGW(TAG, "CRC check failed on channel %u", ch);
      return false;
    }
    channels[ch] = mantissa << exp;
  }
  return true;
}

void OPT4048Component::setup() {
  uint16_t device_id = 0;
  if (!this->read_u16_(OPT4048_REG_DEVICE_ID, device_id)) {
    ESP_LOGE(TAG, "Failed to read device ID");
    this->mark_failed();
    return;
  }
  if (device_id != OPT4048_DEVICE_ID) {
    ESP_LOGE(TAG, "Unexpected device ID 0x%04X (expected 0x%04X)", device_id, OPT4048_DEVICE_ID);
    this->mark_failed();
    return;
  }

  if (this->interrupt_pin_ != nullptr)
    this->interrupt_pin_->setup();

  if (this->has_threshold_low_ && !this->write_threshold_(OPT4048_REG_THRESHOLD_LOW, this->threshold_low_)) {
    this->mark_failed();
    return;
  }
  if (this->has_threshold_high_ && !this->write_threshold_(OPT4048_REG_THRESHOLD_HIGH, this->threshold_high_)) {
    this->mark_failed();
    return;
  }
  if (!this->write_threshold_cfg_() || !this->write_config_()) {
    this->mark_failed();
    return;
  }
}

void OPT4048Component::dump_config() {
  ESP_LOGCONFIG(TAG, "OPT4048:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Range: %u\n"
                "  Conversion time: %u\n"
                "  Mode: %u\n"
                "  Quick wake: %s\n"
                "  Fault count: %u\n"
                "  Threshold channel: %u",
                static_cast<uint8_t>(this->range_), static_cast<uint8_t>(this->conversion_time_),
                static_cast<uint8_t>(this->mode_), YESNO(this->quick_wake_), static_cast<uint8_t>(this->fault_count_),
                this->threshold_channel_);
  LOG_PIN("  Interrupt pin: ", this->interrupt_pin_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "CIE x", this->x_sensor_);
  LOG_SENSOR("  ", "CIE y", this->y_sensor_);
  LOG_SENSOR("  ", "Color Temperature", this->color_temperature_sensor_);
  LOG_SENSOR("  ", "Channel X", this->channel_x_sensor_);
  LOG_SENSOR("  ", "Channel Y", this->channel_y_sensor_);
  LOG_SENSOR("  ", "Channel Z", this->channel_z_sensor_);
  LOG_SENSOR("  ", "Channel W", this->channel_w_sensor_);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Conversion Ready", this->conversion_ready_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Overload", this->overload_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Threshold Low", this->threshold_low_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Threshold High", this->threshold_high_binary_sensor_);
#endif
}

void OPT4048Component::update() {
  if (this->updating_)
    return;
  this->updating_ = true;

  if (this->mode_ == OPT4048Mode::OPT4048_MODE_CONTINUOUS) {
    this->read_and_publish_();
    return;
  }

  if (!this->write_config_()) {
    ESP_LOGW(TAG, "Failed to trigger conversion");
    this->status_set_warning();
    this->updating_ = false;
    return;
  }

  this->set_timeout(this->conversion_timeout_ms_(), [this]() { this->read_and_publish_(); });
}

void OPT4048Component::read_and_publish_() {
  this->updating_ = false;

  uint32_t channels[4];
  if (!this->read_channels_(channels)) {
    this->status_set_warning();
    return;
  }

  uint16_t status = 0;
  if (!this->read_u16_(OPT4048_REG_STATUS, status)) {
    this->status_set_warning();
    return;
  }
  const uint8_t flags = uint8_t(status & 0x0F);

  const double x_tristimulus = channels[0] * M0X + channels[1] * M1X + channels[2] * M2X;
  const double y_tristimulus = channels[0] * M0Y + channels[1] * M1Y + channels[2] * M2Y;
  const double z_tristimulus = channels[0] * M0Z + channels[1] * M1Z + channels[2] * M2Z;
  const double lux = channels[1] * M1L;
  const double sum = x_tristimulus + y_tristimulus + z_tristimulus;

  float cie_x = NAN;
  float cie_y = NAN;
  float cct = NAN;
  if (sum > 0.0) {
    cie_x = float(x_tristimulus / sum);
    cie_y = float(y_tristimulus / sum);
    const double denominator = 0.1858 - cie_y;
    if (std::abs(denominator) > 1e-6) {
      const double n = (cie_x - 0.3320) / denominator;
      cct = float((((437.0 * n + 3601.0) * n + 6861.0) * n) + 5517.0);
    }
  }

  this->status_clear_warning();

  if (this->channel_x_sensor_ != nullptr)
    this->channel_x_sensor_->publish_state(channels[0]);
  if (this->channel_y_sensor_ != nullptr)
    this->channel_y_sensor_->publish_state(channels[1]);
  if (this->channel_z_sensor_ != nullptr)
    this->channel_z_sensor_->publish_state(channels[2]);
  if (this->channel_w_sensor_ != nullptr)
    this->channel_w_sensor_->publish_state(channels[3]);
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(float(lux));
  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(cie_x);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(cie_y);
  if (this->color_temperature_sensor_ != nullptr)
    this->color_temperature_sensor_->publish_state(cct);

#ifdef USE_BINARY_SENSOR
  if (this->conversion_ready_binary_sensor_ != nullptr)
    this->conversion_ready_binary_sensor_->publish_state(flags & OPT4048_FLAG_CONVERSION_READY);
  if (this->overload_binary_sensor_ != nullptr)
    this->overload_binary_sensor_->publish_state(flags & OPT4048_FLAG_OVERLOAD);
  if (this->threshold_low_binary_sensor_ != nullptr)
    this->threshold_low_binary_sensor_->publish_state(flags & OPT4048_FLAG_LOW);
  if (this->threshold_high_binary_sensor_ != nullptr)
    this->threshold_high_binary_sensor_->publish_state(flags & OPT4048_FLAG_HIGH);
#endif

  ESP_LOGD(TAG, "X=%u Y=%u Z=%u W=%u lux=%.2f x=%.4f y=%.4f CCT=%.0f K flags=0x%02X", channels[0], channels[1],
           channels[2], channels[3], lux, cie_x, cie_y, cct, flags);
}

}  // namespace esphome::opt4048
