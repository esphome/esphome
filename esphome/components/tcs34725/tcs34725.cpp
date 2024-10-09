#include "tcs34725.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace tcs34725 {

static const char *const TAG = "tcs34725";

static const uint8_t TCS34725_ADDRESS = 0x29;
static const uint8_t TCS34725_COMMAND_BIT = 0x80;
static const uint8_t TCS34725_REGISTER_ID = TCS34725_COMMAND_BIT | 0x12;
static const uint8_t TCS34725_REGISTER_ATIME = TCS34725_COMMAND_BIT | 0x01;
static const uint8_t TCS34725_REGISTER_CONTROL = TCS34725_COMMAND_BIT | 0x0F;
static const uint8_t TCS34725_REGISTER_ENABLE = TCS34725_COMMAND_BIT | 0x00;
static const uint8_t TCS34725_REGISTER_CRGBDATAL = TCS34725_COMMAND_BIT | 0x14;
static const float RED_CHANNEL_COUNTS_TO_IRRADIANCE = 0.030895152730118627f;    // counts/µW/cm²
static const float GREEN_CHANNEL_COUNTS_TO_IRRADIANCE = 0.032402966993759885f;  // counts/µW/cm²
static const float BLUE_CHANNEL_COUNTS_TO_IRRADIANCE = 0.03695911040578352f;    // counts/µW/cm²

void TCS34725Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCS34725...");
  uint8_t id;
  if (this->read_register(TCS34725_REGISTER_ID, &id, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  if (this->write_config_register_(TCS34725_REGISTER_ATIME, this->integration_reg_) != i2c::ERROR_OK ||
      this->write_config_register_(TCS34725_REGISTER_CONTROL, this->gain_reg_) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  if (this->write_config_register_(TCS34725_REGISTER_ENABLE, 0x01) !=
      i2c::ERROR_OK) {  // Power on (internal oscillator on)
    this->mark_failed();
    return;
  }
  delay(3);
  if (this->write_config_register_(TCS34725_REGISTER_ENABLE, 0x03) !=
      i2c::ERROR_OK) {  // Power on (internal oscillator on) + RGBC ADC Enable
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

  LOG_SENSOR("  ", "Sensor Saturation", this->sensor_saturation_);
  LOG_SENSOR("  ", "Red Channel Irradiance", this->red_irradiance_sensor_);
  LOG_SENSOR("  ", "Green Channel Irradiance", this->green_irradiance_sensor_);
  LOG_SENSOR("  ", "Blue Channel Irradiance", this->blue_irradiance_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Color Temperature", this->color_temperature_sensor_);
}
float TCS34725Component::get_setup_priority() const { return setup_priority::DATA; }

/*!
 *  @brief  Determines the minimum raw value threshold for color channels at which the sensor is considered underexposed
 *
 *  @return Minimum raw value threshold
 */
uint16_t TCS34725Component::get_min_raw_limit_() const {
  // Minimum raw value below 1 is considered too low, return NaN
  return 1;
}

/*!
 *  @brief  Determines the saturation threshold at which the sensor is considered overexposed
 *
 *  @return The saturation limit as a percentage (between 0.0 and 100.0).
 */
float TCS34725Component::get_saturation_limit_() const {
  // Return 99.99f if integration time is below 153.6ms, else return 75.0f
  if ((256 - this->integration_reg_) < TCS34725_INTEGRATION_TIME_154MS) {
    return 99.99f;
  } else {
    /* Adjust sat limit to 75% to avoid analog saturation if atime < 153.6ms */
    return 75.0f;
  }
}

/*!
 *  @brief  Converts the raw R/G/B values to color temperature in degrees
 *          Kelvin using the algorithm described in DN40 from Taos (now AMS).
 *  @param  r
 *          Red value
 *  @param  g
 *          Green value
 *  @param  b
 *          Blue value
 *  @param  current_saturation
 *          Sensor saturation in percent
 *  @param  min_raw_value
 *          lowest raw value reported by the sensor
 *  @return Color temperature in degrees Kelvin
 */
void TCS34725Component::calculate_temperature_and_lux_(uint16_t r, uint16_t g, uint16_t b, float current_saturation,
                                                       uint16_t min_raw_value) {
  this->illuminance_ = NAN;
  this->color_temperature_ = NAN;

  const float ga = this->glass_attenuation_;            // Glass Attenuation Factor
  static const float DF = 310.f;                        // Device Factor
  static const float R_COEF = 0.136f;                   //
  static const float G_COEF = 1.f;                      // used in lux computation
  static const float B_COEF = -0.444f;                  //
  static const float CT_COEF = 3810.f;                  // Color Temperature Coefficient
  static const float CT_OFFSET = 1391.f;                // Color Temperatuer Offset
  static const float MAX_ILLUMINANCE = 100000.0f;       // Cap illuminance at 100,000 lux
  static const float MAX_COLOR_TEMPERATURE = 15000.0f;  // Maximum expected color temperature in Kelvin
  static const float MIN_COLOR_TEMPERATURE = 1000.0f;   // Maximum reasonable color temperature in Kelvin

  uint16_t min_raw_limit = get_min_raw_limit_();

  if (min_raw_value < min_raw_limit) {
    ESP_LOGW(TAG,
             "Saturation too low, sample with saturation %d (raw value) below limit (%d). Lux/color"
             "temperature cannot reliably calculated.",
             min_raw_value, min_raw_limit);
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

  float sat_limit = get_saturation_limit_();

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

  /* Check for saturation and mark the sample as invalid if true */
  if (current_saturation >= sat_limit) {
    if (this->integration_time_auto_) {
      ESP_LOGI(TAG, "Saturation too high, skip Lux/color temperature calculation, autogain ongoing");
      return;
    } else {
      ESP_LOGW(TAG,
               "Saturation too high, sample with saturation %.1f above limit (%.1f). Lux/color temperature cannot be "
               "reliably calculated, reduce integration/gain or use a grey filter.",
               current_saturation, sat_limit);
      return;
    }
  }

  // Lux Calculation (DN40 3.2)

  float g1 = R_COEF * (float) r + G_COEF * (float) g + B_COEF * (float) b;
  float cpl = (this->integration_time_ * this->gain_) / (ga * DF);

  this->illuminance_ = std::max(g1 / cpl, 0.0f);

  if (this->illuminance_ > MAX_ILLUMINANCE) {
    if (ga < 1.1f || this->illuminance_ > MAX_ILLUMINANCE * ga) {
      ESP_LOGW(TAG, "Calculated illuminance greater than limit (%f), setting to NAN", this->illuminance_);
      this->illuminance_ = NAN;
      return;
    }
  }

  if (r == 0) {
    ESP_LOGW(TAG, "Red channel is zero, cannot compute color temperature");
    return;
  }

  // Color Temperature Calculation (DN40)
  /* A simple method of measuring color temp is to use the ratio of blue */
  /* to red light. */

  this->color_temperature_ = (CT_COEF * (float) b) / (float) r + CT_OFFSET;

  // Ensure the color temperature stays within reasonable bounds
  if (this->color_temperature_ < MIN_COLOR_TEMPERATURE) {
    ESP_LOGW(TAG, "Calculated color temperature value too low (%f), setting to NAN", this->color_temperature_);
    this->color_temperature_ = NAN;
  } else if (this->color_temperature_ > MAX_COLOR_TEMPERATURE) {
    ESP_LOGW(TAG, "Calculated color temperature value too high (%f), setting to NAN", this->color_temperature_);
    this->color_temperature_ = NAN;
  }
}

/*!
 *  @brief  Calculates the irradiance per channel (R/G/B) using fixed conversion factors.
 *  @param  r
 *          Red raw value
 *  @param  g
 *          Green raw value
 *  @param  b
 *          Blue raw value
 *  @param  current_saturation
 *          Sensor saturation in percent
 *  @param  min_raw_value
 *          Lowest raw value reported by the sensor
 */
void TCS34725Component::calculate_irradiance_(uint16_t r, uint16_t g, uint16_t b, float current_saturation,
                                              uint16_t min_raw_value) {
  this->irradiance_r_ = NAN;
  this->irradiance_g_ = NAN;
  this->irradiance_b_ = NAN;

  uint16_t min_raw_limit = get_min_raw_limit_();
  float sat_limit = get_saturation_limit_();

  if (min_raw_value < min_raw_limit) {
    ESP_LOGW(TAG,
             "Saturation too low, sample with saturation %d (raw value) below limit (%d). Irradiance cannot be "
             "reliably calculated.",
             min_raw_value, min_raw_limit);
    return;
  }

  /* Check for saturation and mark the sample as invalid if true */
  if (current_saturation >= sat_limit) {
    if (this->integration_time_auto_) {
      ESP_LOGI(TAG, "Saturation too high, skip irradiance calculation, autogain ongoing");
      return;
    } else {
      ESP_LOGW(TAG,
               "Saturation too high, sample with saturation %.1f above limit (%.1f). Irradiance cannot be reliably "
               "calculated, reduce integration/gain or use a grey filter.",
               current_saturation, sat_limit);
      return;
    }
  }

  // Calculate the scaling factor for integration time
  float integration_time_scaling = this->integration_time_ / 2.4f;

  // Calculate irradiance for each channel using predefined conversion factors
  this->irradiance_r_ = std::max(r / (RED_CHANNEL_COUNTS_TO_IRRADIANCE * integration_time_scaling * this->gain_), 0.0f);
  this->irradiance_g_ =
      std::max(g / (GREEN_CHANNEL_COUNTS_TO_IRRADIANCE * integration_time_scaling * this->gain_), 0.0f);
  this->irradiance_b_ =
      std::max(b / (BLUE_CHANNEL_COUNTS_TO_IRRADIANCE * integration_time_scaling * this->gain_), 0.0f);

  ESP_LOGD(TAG, "Calculated irradiance - R: %.2f µW/cm2, G: %.2f µW/cm2, B: %.2f µW/cm2", this->irradiance_r_,
           this->irradiance_g_, this->irradiance_b_);
}

void TCS34725Component::update() {
  uint8_t data[8];  // Buffer to hold the 8 bytes (2 bytes for each of the 4 channels)

  // Perform burst
  if (this->read_register(TCS34725_REGISTER_CRGBDATAL, data, 8) != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Error reading TCS34725 sensor data");
    return;
  }

  // Extract the data
  uint16_t raw_c = encode_uint16(data[1], data[0]);  // Clear channel
  uint16_t raw_r = encode_uint16(data[3], data[2]);  // Red channel
  uint16_t raw_g = encode_uint16(data[5], data[4]);  // Green channel
  uint16_t raw_b = encode_uint16(data[7], data[6]);  // Blue channel

  ESP_LOGV(TAG, "Raw values clear=%d red=%d green=%d blue=%d", raw_c, raw_r, raw_g, raw_b);

  float current_saturation;
  uint16_t peak_raw_value = std::max({raw_r, raw_g, raw_b});
  uint16_t min_raw_value = std::min({raw_r, raw_g, raw_b});
  uint16_t max_count;

  /* sensor counts up to 1024 for each 2.4 ms of integration time, until 65535 is hit, which is the
   * maximum which can be stored in the counter. This happens at 153.6 ms integration time. */
  max_count = (this->integration_reg_ > 192)
                  ? 65535
                  : (uint16_t) std::min(std::round(this->integration_time_ * 1024.0f / 2.4f), 65535.0f);

  current_saturation = ((float) peak_raw_value / (float) max_count) * 100.0f;

  current_saturation = clamp(current_saturation, 0.0f, 100.0f);

  if (this->red_irradiance_sensor_ || this->green_irradiance_sensor_ || this->blue_irradiance_sensor_) {
    calculate_irradiance_(raw_r, raw_g, raw_b, current_saturation, min_raw_value);
  }

  if (this->illuminance_sensor_ || this->color_temperature_sensor_) {
    calculate_temperature_and_lux_(raw_r, raw_g, raw_b, current_saturation, min_raw_value);
  }

  // do not publish values if auto gain finding ongoing, and oversaturated
  // so: publish when:
  // - not auto mode
  // - sensor not oversaturated
  // - sensor oversaturated but gain and timing cannot go lower
  if (!this->integration_time_auto_ || current_saturation < 99.99f ||
      (this->gain_reg_ == 0 && this->integration_time_ < 200)) {
    if (this->illuminance_sensor_ != nullptr)
      this->illuminance_sensor_->publish_state(this->illuminance_);
    if (this->color_temperature_sensor_ != nullptr)
      this->color_temperature_sensor_->publish_state(this->color_temperature_);
    if (this->sensor_saturation_ != nullptr)
      this->sensor_saturation_->publish_state(current_saturation);
    if (this->red_irradiance_sensor_ != nullptr)
      this->red_irradiance_sensor_->publish_state(this->irradiance_r_);
    if (this->green_irradiance_sensor_ != nullptr)
      this->green_irradiance_sensor_->publish_state(this->irradiance_g_);
    if (this->blue_irradiance_sensor_ != nullptr)
      this->blue_irradiance_sensor_->publish_state(this->irradiance_b_);
  }

  ESP_LOGD(TAG,
           "Calculated: Red Irad=%.2f µW/cm², Green Irad=%.2f µW/cm², Blue Irad=%.2f µW/cm², Sensor Sat=%.2f%%, "
           "Illum=%.1f lx, Color Temp=%.1f K",
           this->irradiance_r_, this->irradiance_g_, this->irradiance_b_, current_saturation, this->illuminance_,
           this->color_temperature_);

  if (this->integration_time_auto_) {
    // change integration time an gain to achieve maximum resolution an dynamic range
    // calculate optimal integration time to achieve 60% saturation
    float integration_time_ideal;

    integration_time_ideal = 60 / ((float) std::max((uint16_t) 1, peak_raw_value) / 655.35f) * this->integration_time_;

    uint8_t gain_reg_val_new = this->gain_reg_;
    // increase gain if peak value is less 20% of maximum and we're already using the highest
    // integration time
    if (this->gain_reg_ < 3) {
      if (((float) peak_raw_value / 655.35 < 20.f) && (this->integration_time_ > 600.f)) {
        gain_reg_val_new = this->gain_reg_ + 1;
        // update integration time to new situation
        integration_time_ideal = integration_time_ideal / 4;
      }
    }

    // decrease gain, if very high sensor values and integration times already low
    if (this->gain_reg_ > 0) {
      if (70 < ((float) peak_raw_value / 655.35) && (this->integration_time_ < 200)) {
        gain_reg_val_new = this->gain_reg_ - 1;
        // update integration time to new situation
        integration_time_ideal = integration_time_ideal * 4;
      }
    }

    // saturate integration times
    float integration_time_next = integration_time_ideal;
    if (integration_time_ideal > 2.4f * 256) {
      integration_time_next = 2.4f * 256;
    }
    if (integration_time_ideal < 154) {
      integration_time_next = 154;
    }

    // calculate register value from timing
    uint8_t regval_atime = (uint8_t) (256.f - integration_time_next / 2.4f);
    ESP_LOGD(TAG, "Integration time: %.1fms, ideal: %.1fms regval_new %d Gain: %.f Peak raw: %d  gain reg: %d",
             this->integration_time_, integration_time_next, regval_atime, this->gain_, peak_raw_value,
             this->gain_reg_);

    if (this->integration_reg_ != regval_atime || gain_reg_val_new != this->gain_reg_) {
      this->integration_reg_ = regval_atime;
      this->gain_reg_ = gain_reg_val_new;
      set_gain((TCS34725Gain) gain_reg_val_new);
      if (this->write_config_register_(TCS34725_REGISTER_ATIME, this->integration_reg_) != i2c::ERROR_OK ||
          this->write_config_register_(TCS34725_REGISTER_CONTROL, this->gain_reg_) != i2c::ERROR_OK) {
        this->mark_failed();
        ESP_LOGW(TAG, "TCS34725I update timing failed!");
      } else {
        this->integration_time_ = integration_time_next;
      }
    }
  }
  this->status_clear_warning();
}
void TCS34725Component::set_integration_time(TCS34725IntegrationTime integration_time) {
  // if an integration time is 0x100, this is auto start with 154ms as this gives best starting point
  TCS34725IntegrationTime my_integration_time_regval;

  if (integration_time == TCS34725_INTEGRATION_TIME_AUTO) {
    this->integration_time_auto_ = true;
    this->integration_reg_ = TCS34725_INTEGRATION_TIME_154MS;
    my_integration_time_regval = TCS34725_INTEGRATION_TIME_154MS;
  } else {
    this->integration_reg_ = integration_time;
    my_integration_time_regval = integration_time;
    this->integration_time_auto_ = false;
  }
  this->integration_time_ = (256.f - my_integration_time_regval) * 2.4f;
  ESP_LOGI(TAG, "TCS34725I Integration time set to: %.1fms", this->integration_time_);
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
