#include "tcs34725.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tcs34725 {

static const char *const TAG = "tcs34725";

static const uint8_t TCS34725_ADDRESS = 0x29;
static const uint8_t TCS34725_COMMAND_BIT = 0x80;
static const uint8_t TCS34725_REGISTER_ID = TCS34725_COMMAND_BIT | 0x12;
static const uint8_t TCS34725_REGISTER_ATIME = TCS34725_COMMAND_BIT | 0x01;
static const uint8_t TCS34725_REGISTER_CONTROL = TCS34725_COMMAND_BIT | 0x0F;
static const uint8_t TCS34725_REGISTER_ENABLE = TCS34725_COMMAND_BIT | 0x00;
static const uint8_t TCS34725_REGISTER_CDATAL = TCS34725_COMMAND_BIT | 0x14;
static const uint8_t TCS34725_REGISTER_RDATAL = TCS34725_COMMAND_BIT | 0x16;
static const uint8_t TCS34725_REGISTER_GDATAL = TCS34725_COMMAND_BIT | 0x18;
static const uint8_t TCS34725_REGISTER_BDATAL = TCS34725_COMMAND_BIT | 0x1A;

void TCS34725Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCS34725...");
  uint8_t id;
  if (!this->read_byte(TCS34725_REGISTER_ID, &id)) {
    this->mark_failed();
    return;
  }

  if (!this->write_byte(TCS34725_REGISTER_ATIME, this->integration_reg_) ||
      !this->write_byte(TCS34725_REGISTER_CONTROL, this->gain_reg_)) {
    this->mark_failed();
    return;
  }

  if (!this->write_byte(TCS34725_REGISTER_ENABLE, 0x01)) {  // Power on (internal oscillator on)
    this->mark_failed();
    return;
  }
  delay(3);
  if (!this->write_byte(TCS34725_REGISTER_ENABLE, 0x03)) {  // Power on (internal oscillator on) + RGBC ADC Enable
    this->mark_failed();
    return;
  }
}

void TCS34725Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TCS34725:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TCS34725 failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Clear Channel", this->clear_sensor_);
  LOG_SENSOR("  ", "Red Channel", this->red_sensor_);
  LOG_SENSOR("  ", "Green Channel", this->green_sensor_);
  LOG_SENSOR("  ", "Blue Channel", this->blue_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Color Temperature", this->color_temperature_sensor_);
}
float TCS34725Component::get_setup_priority() const { return setup_priority::DATA; }

/*!
 *  @brief  Converts the raw R/G/B values to color temperature in degrees
 *          Kelvin using the algorithm described in DN40 from Taos (now AMS).
 *  @param  r
 *          Red value
 *  @param  g
 *          Green value
 *  @param  b
 *          Blue value
 *  @param  c
 *          Clear channel value
 *  @return Color temperature in degrees Kelvin
 */
void TCS34725Component::calculate_temperature_and_lux_(uint16_t r, uint16_t g, uint16_t b, uint16_t c) {
  float r2, g2, b2; /* RGB values minus IR component */
  float sat;        /* Digital saturation level */
  float ir;         /* Inferred IR content */

  this->illuminance_ = 0;  // Assign 0 value before calculation
  this->color_temperature_ = 0;

  const float ga = this->glass_attenuation_;  // Glass Attenuation Factor
  static const float DF = 310.f;              // Device Factor
  static const float R_COEF = 0.136f;         //
  static const float G_COEF = 1.f;            // used in lux computation
  static const float B_COEF = -0.444f;        //
  static const float CT_COEF = 3810.f;        // Color Temperature Coefficient
  static const float CT_OFFSET = 1391.f;      // Color Temperatuer Offset

  if (c == 0) {
    return;
  }

  /* Analog/Digital saturation:
   *
   * (a) As light becomes brighter, the clear channel will tend to
   *     saturate first since R+G+B is approximately equal to C.
   * (b) The TCS34725 accumulates 1024 counts per 2.4ms of integration
   *     time, up to a maximum values of 65535. This means analog
   *     saturation can occur up to an integration time of 153.6ms
   *     (64*2.4ms=153.6ms).
   * (c) If the integration time is > 153.6ms, digital saturation will
   *     occur before analog saturation. Digital saturation occurs when
   *     the count reaches 65535.
   */
  if ((256 - this->integration_reg_) > 63) {
    /* Track digital saturation */
    sat = 65535.f;
  } else {
    /* Track analog saturation */
    sat = 1024.f * (256.f - this->integration_reg_);
  }

  /* Ripple rejection:
   *
   * (a) An integration time of 50ms or multiples of 50ms are required to
   *     reject both 50Hz and 60Hz ripple.
   * (b) If an integration time faster than 50ms is required, you may need
   *     to average a number of samples over a 50ms period to reject ripple
   *     from fluorescent and incandescent light sources.
   *
   * Ripple saturation notes:
   *
   * (a) If there is ripple in the received signal, the value read from C
   *     will be less than the max, but still have some effects of being
   *     saturated. This means that you can be below the 'sat' value, but
   *     still be saturating. At integration times >150ms this can be
   *     ignored, but <= 150ms you should calculate the 75% saturation
   *     level to avoid this problem.
   */
  if (this->integration_time_ < 150) {
    /* Adjust sat to 75% to avoid analog saturation if atime < 153.6ms */
    sat -= sat / 4.f;
  }

  /* Check for saturation and mark the sample as invalid if true */
  if (c >= sat) {
    return;
  }

  /* AMS RGB sensors have no IR channel, so the IR content must be */
  /* calculated indirectly. */
  ir = ((r + g + b) > c) ? (r + g + b - c) / 2 : 0;

  /* Remove the IR component from the raw RGB values */
  r2 = r - ir;
  g2 = g - ir;
  b2 = b - ir;

  if (r2 == 0) {
    return;
  }

  // Lux Calculation (DN40 3.2)

  float g1 = R_COEF * r2 + G_COEF * g2 + B_COEF * b2;
  float cpl = (this->integration_time_ * this->gain_) / (ga * DF);
  this->illuminance_ = g1 / cpl;

  // Color Temperature Calculation (DN40)
  /* A simple method of measuring color temp is to use the ratio of blue */
  /* to red light, taking IR cancellation into account. */
  this->color_temperature_ = (CT_COEF * b2) / /** Color temp coefficient. */
                                 r2 +
                             CT_OFFSET; /** Color temp offset. */
}

void TCS34725Component::update() {
  uint16_t raw_c;
  uint16_t raw_r;
  uint16_t raw_g;
  uint16_t raw_b;

  if (!this->read_byte_16(TCS34725_REGISTER_CDATAL, &raw_c) || !this->read_byte_16(TCS34725_REGISTER_RDATAL, &raw_r) ||
      !this->read_byte_16(TCS34725_REGISTER_GDATAL, &raw_g) || !this->read_byte_16(TCS34725_REGISTER_BDATAL, &raw_b)) {
    ESP_LOGW(TAG, "Reading data from TCS34725 failed!");
    this->status_set_warning();
    return;
  }

  // May need to fix endianness as the data read over I2C is big-endian, but most ESP platforms are little-endian
  raw_c = i2c::i2ctohs(raw_c);
  raw_r = i2c::i2ctohs(raw_r);
  raw_g = i2c::i2ctohs(raw_g);
  raw_b = i2c::i2ctohs(raw_b);

  const float channel_c = raw_c / 655.35f;
  const float channel_r = raw_r / 655.35f;
  const float channel_g = raw_g / 655.35f;
  const float channel_b = raw_b / 655.35f;
  if (this->clear_sensor_ != nullptr)
    this->clear_sensor_->publish_state(channel_c);
  if (this->red_sensor_ != nullptr)
    this->red_sensor_->publish_state(channel_r);
  if (this->green_sensor_ != nullptr)
    this->green_sensor_->publish_state(channel_g);
  if (this->blue_sensor_ != nullptr)
    this->blue_sensor_->publish_state(channel_b);

  if (this->illuminance_sensor_ || this->color_temperature_sensor_) {
    calculate_temperature_and_lux_(raw_r, raw_g, raw_b, raw_c);
  }

  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(this->illuminance_);

  if (this->color_temperature_sensor_ != nullptr)
    this->color_temperature_sensor_->publish_state(this->color_temperature_);

  ESP_LOGD(TAG, "Got R=%.1f%%,G=%.1f%%,B=%.1f%%,C=%.1f%% Illuminance=%.1flx Color Temperature=%.1fK", channel_r,
           channel_g, channel_b, channel_c, this->illuminance_, this->color_temperature_);

  this->status_clear_warning();
}
void TCS34725Component::set_integration_time(TCS34725IntegrationTime integration_time) {
  this->integration_reg_ = integration_time;
  this->integration_time_ = (256.f - integration_time) * 2.4f;
}
void TCS34725Component::set_gain(TCS34725Gain gain) {
  this->gain_reg_ = gain;
  switch (gain) {
    case TCS34725Gain::TCS34725_GAIN_1X:
      this->gain_ = 1.f;
      break;
    case TCS34725Gain::TCS34725_GAIN_4X:
      this->gain_ = 4.f;
      break;
    case TCS34725Gain::TCS34725_GAIN_16X:
      this->gain_ = 16.f;
      break;
    case TCS34725Gain::TCS34725_GAIN_60X:
      this->gain_ = 60.f;
      break;
    default:
      this->gain_ = 1.f;
      break;
  }
}

void TCS34725Component::set_glass_attenuation_factor(float ga) {
  // The Glass Attenuation (FA) factor used to compensate for lower light
  // levels at the device due to the possible presence of glass. The GA is
  // the inverse of the glass transmissivity (T), so GA = 1/T. A transmissivity
  // of 50% gives GA = 1 / 0.50 = 2. If no glass is present, use GA = 1.
  // See Application Note: DN40-Rev 1.0
  this->glass_attenuation_ = ga;
}

}  // namespace tcs34725
}  // namespace esphome
