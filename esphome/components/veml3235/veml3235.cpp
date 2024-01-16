#include "veml3235.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace veml3235 {

static const char *const TAG = "veml3235.sensor";

void VEML3235Sensor::setup() {
  uint8_t device_id[] = {0, 0};

  ESP_LOGCONFIG(TAG, "Setting up VEML3235 '%s'...", this->name_.c_str());

  if (!this->refresh_config_reg()) {
    ESP_LOGE(TAG, "Unable to write configuration");
    this->mark_failed();
    return;
  }
  if ((this->write(&ID_REG, 1, false) != i2c::ERROR_OK) || !this->read_bytes_raw(device_id, 2)) {
    ESP_LOGE(TAG, "Unable to read ID");
    this->mark_failed();
    return;
  } else if (device_id[0] != DEVICE_ID) {
    ESP_LOGE(TAG, "Incorrect device ID - expected 0x%.2x, read 0x%.2x", DEVICE_ID, device_id[0]);
    this->mark_failed();
    return;
  }
}

bool VEML3235Sensor::refresh_config_reg(bool force_on) {
  uint16_t data = this->power_on_ || force_on ? 0 : SHUTDOWN_BITS;

  data |= (uint16_t(this->integration_time_ << CONFIG_REG_IT_BIT));
  data |= (uint16_t(this->digital_gain_ << CONFIG_REG_DG_BIT));
  data |= (uint16_t(this->gain_ << CONFIG_REG_G_BIT));
  data |= 0x1;  // mandatory 1 here per RM

  ESP_LOGVV(TAG, "Writing 0x%.4x to register 0x%.2x", data, CONFIG_REG);
  return this->write_byte_16(CONFIG_REG, data);
}

float VEML3235Sensor::read_lx_() {
  if (!this->power_on_) {  // if off, turn on
    if (!this->refresh_config_reg(true)) {
      ESP_LOGW(TAG, "Turning on failed");
      this->status_set_warning();
      return NAN;
    }
    delay(4);  // from RM: a wait time of 4 ms should be observed before the first measurement is picked up, to allow
               // for a correct start of the signal processor and oscillator
  }

  uint8_t als_regs[] = {0, 0};
  if ((this->write(&ALS_REG, 1, false) != i2c::ERROR_OK) || !this->read_bytes_raw(als_regs, 2)) {
    this->status_set_warning();
    return NAN;
  }

  this->status_clear_warning();

  float als_raw_value_multiplier = LUX_MULTIPLIER_BASE;
  uint16_t als_raw_value = encode_uint16(als_regs[1], als_regs[0]);
  // determine multiplier value based on gains and integration time
  if (this->digital_gain_ == VEML3235_DIGITAL_GAIN_1X) {
    als_raw_value_multiplier *= 2;
  }
  switch (this->gain_) {
    case VEML3235_GAIN_1X:
      als_raw_value_multiplier *= 4;
      break;
    case VEML3235_GAIN_2X:
      als_raw_value_multiplier *= 2;
      break;
    default:
      break;
  }
  switch (this->integration_time_) {
    case VEML3235_INTEGRATION_TIME_50MS:
      als_raw_value_multiplier *= 16;
      break;
    case VEML3235_INTEGRATION_TIME_100MS:
      als_raw_value_multiplier *= 8;
      break;
    case VEML3235_INTEGRATION_TIME_200MS:
      als_raw_value_multiplier *= 4;
      break;
    case VEML3235_INTEGRATION_TIME_400MS:
      als_raw_value_multiplier *= 2;
      break;
    default:
      break;
  }
  // finally, determine and return the actual lux value
  float lx = float(als_raw_value) * als_raw_value_multiplier;
  ESP_LOGVV(TAG, "'%s': ALS raw = %u, multiplier = %.5f", this->get_name().c_str(), als_raw_value,
            als_raw_value_multiplier);
  ESP_LOGD(TAG, "'%s': Illuminance = %.4flx", this->get_name().c_str(), lx);

  if (!this->power_on_) {  // turn off if required
    if (!this->refresh_config_reg()) {
      ESP_LOGW(TAG, "Turning off failed");
      this->status_set_warning();
    }
  }

  if (this->auto_gain_) {
    this->adjust_gain_(als_raw_value);
  }

  return lx;
}

void VEML3235Sensor::adjust_gain_(const uint16_t als_raw_value) {
  if ((als_raw_value > UINT16_MAX * this->auto_gain_threshold_low_) &&
      (als_raw_value < UINT16_MAX * this->auto_gain_threshold_high_)) {
    return;
  }

  if (als_raw_value >= UINT16_MAX * 0.9) {  // over-saturated, reset all gains and start over
    this->digital_gain_ = VEML3235_DIGITAL_GAIN_1X;
    this->gain_ = VEML3235_GAIN_1X;
    this->integration_time_ = VEML3235_INTEGRATION_TIME_50MS;
    this->refresh_config_reg();
    return;
  }

  if (this->gain_ != VEML3235_GAIN_4X) {  // increase gain if possible
    switch (this->gain_) {
      case VEML3235_GAIN_1X:
        this->gain_ = VEML3235_GAIN_2X;
        break;
      case VEML3235_GAIN_2X:
        this->gain_ = VEML3235_GAIN_4X;
        break;
      default:
        break;
    }
    this->refresh_config_reg();
    return;
  }
  // gain is maxed out; reset it and try to increase digital gain
  if (this->digital_gain_ != VEML3235_DIGITAL_GAIN_2X) {  // increase digital gain if possible
    this->digital_gain_ = VEML3235_DIGITAL_GAIN_2X;
    this->gain_ = VEML3235_GAIN_1X;
    this->refresh_config_reg();
    return;
  }
  // digital gain is maxed out; reset it and try to increase integration time
  if (this->integration_time_ != VEML3235_INTEGRATION_TIME_800MS) {  // increase integration time if possible
    switch (this->integration_time_) {
      case VEML3235_INTEGRATION_TIME_50MS:
        this->integration_time_ = VEML3235_INTEGRATION_TIME_100MS;
        break;
      case VEML3235_INTEGRATION_TIME_100MS:
        this->integration_time_ = VEML3235_INTEGRATION_TIME_200MS;
        break;
      case VEML3235_INTEGRATION_TIME_200MS:
        this->integration_time_ = VEML3235_INTEGRATION_TIME_400MS;
        break;
      case VEML3235_INTEGRATION_TIME_400MS:
        this->integration_time_ = VEML3235_INTEGRATION_TIME_800MS;
        break;
      default:
        break;
    }
    this->digital_gain_ = VEML3235_DIGITAL_GAIN_1X;
    this->gain_ = VEML3235_GAIN_1X;
    this->refresh_config_reg();
    return;
  }
}

void VEML3235Sensor::dump_config() {
  uint8_t digital_gain = 1;
  uint8_t gain = 1;
  uint16_t integration_time = 0;

  if (this->digital_gain_ == VEML3235_DIGITAL_GAIN_2X) {
    digital_gain = 2;
  }
  switch (this->gain_) {
    case VEML3235_GAIN_2X:
      gain = 2;
      break;
    case VEML3235_GAIN_4X:
      gain = 4;
      break;
    default:
      break;
  }
  switch (this->integration_time_) {
    case VEML3235_INTEGRATION_TIME_50MS:
      integration_time = 50;
      break;
    case VEML3235_INTEGRATION_TIME_100MS:
      integration_time = 100;
      break;
    case VEML3235_INTEGRATION_TIME_200MS:
      integration_time = 200;
      break;
    case VEML3235_INTEGRATION_TIME_400MS:
      integration_time = 400;
      break;
    case VEML3235_INTEGRATION_TIME_800MS:
      integration_time = 800;
      break;
    default:
      break;
  }

  LOG_SENSOR("", "VEML3235", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication failed");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Auto-gain enabled: %s", YESNO(this->auto_gain_));
  if (this->auto_gain_) {
    ESP_LOGCONFIG(TAG, "  Auto-gain upper threshold: %f%%", this->auto_gain_threshold_high_ * 100.0);
    ESP_LOGCONFIG(TAG, "  Auto-gain lower threshold: %f%%", this->auto_gain_threshold_low_ * 100.0);
    ESP_LOGCONFIG(TAG, "  Values below will be used as initial values only");
  }
  ESP_LOGCONFIG(TAG, "  Digital gain: %uX", digital_gain);
  ESP_LOGCONFIG(TAG, "  Gain: %uX", gain);
  ESP_LOGCONFIG(TAG, "  Integration time: %ums", integration_time);
}

}  // namespace veml3235
}  // namespace esphome
