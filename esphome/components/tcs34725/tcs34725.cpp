#include "tcs34725.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <algorithm>

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
    if (this->integration_time_auto_) {
      ESP_LOGI(TAG, "Saturation too high, sample discarded, autogain ongoing");
    } else {
      ESP_LOGW(
          TAG,
          "Saturation too high, sample with saturation %.1f and clear %d treat values carefully or use grey filter",
          sat, c);
    }
  }

  /* AMS RGB sensors have no IR channel, so the IR content must be */
  /* calculated indirectly. */
  ir = ((r + g + b) > c) ? (r + g + b - c) / 2 : 0;

  /* Remove the IR component from the raw RGB values */
  r2 = r - ir;
  g2 = g - ir;
  b2 = b - ir;

  // discarding super low values? not recemmonded, and avoided by using auto gain.
  if (r2 == 0) {
    // legacy code
    if (!this->integration_time_auto_) {
      ESP_LOGW(TAG,
               "No light detected on red channel, switch to auto gain or adjust timing, values will be unreliable");
      return;
    }
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

  if (this->read_data_register_(TCS34725_REGISTER_CDATAL, raw_c) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  if (this->read_data_register_(TCS34725_REGISTER_RDATAL, raw_r) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  if (this->read_data_register_(TCS34725_REGISTER_GDATAL, raw_g) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  if (this->read_data_register_(TCS34725_REGISTER_BDATAL, raw_b) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  ESP_LOGV(TAG, "Raw values clear=%d red=%d green=%d blue=%d", raw_c, raw_r, raw_g, raw_b);

  float channel_c;
  float channel_r;
  float channel_g;
  float channel_b;
  // avoid division by 0 and return black if clear is 0
  if (raw_c == 0) {
    channel_c = channel_r = channel_g = channel_b = 0.0f;
  } else {
    float max_count = this->integration_time_ <= 153.6f ? this->integration_time_ * 1024.0f / 2.4f : 65535.0f;
    float sum = raw_c;
    channel_r = raw_r / sum * 100.0f;
    channel_g = raw_g / sum * 100.0f;
    channel_b = raw_b / sum * 100.0f;
    channel_c = raw_c / max_count * 100.0f;
  }

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

  // do not publish values if auto gain finding ongoing, and oversaturated
  // so: publish when:
  // - not auto mode
  // - clear not oversaturated
  // - clear oversaturated but gain and timing cannot go lower
  if (!this->integration_time_auto_ || raw_c < 65530 || (this->gain_reg_ == 0 && this->integration_time_ < 200)) {
    if (this->illuminance_sensor_ != nullptr)
      this->illuminance_sensor_->publish_state(this->illuminance_);

    if (this->color_temperature_sensor_ != nullptr)
      this->color_temperature_sensor_->publish_state(this->color_temperature_);
  }

  ESP_LOGD(TAG,
           "Got Red=%.1f%%,Green=%.1f%%,Blue=%.1f%%,Clear=%.1f%% Illuminance=%.1flx Color "
           "Temperature=%.1fK",
           channel_r, channel_g, channel_b, channel_c, this->illuminance_, this->color_temperature_);

  if (this->integration_time_auto_) {
    // change integration time an gain to achieve maximum resolution an dynamic range
    // calculate optimal integration time to achieve 70% satuaration
    float integration_time_ideal;

    integration_time_ideal = 60 / ((float) std::max((uint16_t) 1, raw_c) / 655.35f) * this->integration_time_;

    uint8_t gain_reg_val_new = this->gain_reg_;
    // increase gain if less than 20% of white channel used and high integration time
    // increase only if not already maximum
    // do not use max gain, as ist will not get better
    if (this->gain_reg_ < 3) {
      if (((float) raw_c / 655.35 < 20.f) && (this->integration_time_ > 600.f)) {
        gain_reg_val_new = this->gain_reg_ + 1;
        // update integration time to new situation
        integration_time_ideal = integration_time_ideal / 4;
      }
    }

    // decrease gain, if very high clear values and integration times alreadey low
    if (this->gain_reg_ > 0) {
      if (70 < ((float) raw_c / 655.35) && (this->integration_time_ < 200)) {
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
    ESP_LOGD(TAG, "Integration time: %.1fms, ideal: %.1fms regval_new %d Gain: %.f Clear channel raw: %d  gain reg: %d",
             this->integration_time_, integration_time_next, regval_atime, this->gain_, raw_c, this->gain_reg_);

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
