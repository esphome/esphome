#include "veml3235.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::veml3235 {

static const char *const TAG = "veml3235.sensor";

// ADC counts at or above this value (98% of full scale) are treated as clipped: the true light level cannot
// be estimated from such a reading, so auto-gain restarts from minimum sensitivity instead
static const uint16_t CLIPPED_COUNTS = 64224;

// Maximum sensitivity multiplier: integration time 800 ms (16x) * gain 4x * digital gain 2x
static const uint16_t MAX_SENSITIVITY_FACTOR = 128;

// At most one restart from clipping plus one proportional adjustment per update cycle
static const uint8_t MAX_ADJUSTMENTS_PER_UPDATE = 2;

void VEML3235Sensor::setup() {
  uint8_t device_id[] = {0, 0};
  if (!this->refresh_config_reg()) {
    ESP_LOGE(TAG, "Unable to write configuration");
    this->mark_failed();
    return;
  }
  if ((this->read_register(ID_REG, device_id, sizeof device_id) != i2c::ERROR_OK)) {
    ESP_LOGE(TAG, "Unable to read ID");
    this->mark_failed();
  } else if (device_id[0] != DEVICE_ID) {
    ESP_LOGE(TAG, "Incorrect device ID - expected 0x%.2x, read 0x%.2x", DEVICE_ID, device_id[0]);
    this->mark_failed();
  }
}

bool VEML3235Sensor::refresh_config_reg() {
  uint16_t data = 0x1;  // mandatory 1 per RM; shutdown bits cleared (device powered on)

  data |= (uint16_t(this->integration_time_) << CONFIG_REG_IT_BIT);
  data |= (uint16_t(this->digital_gain_) << CONFIG_REG_DG_BIT);
  data |= (uint16_t(this->gain_) << CONFIG_REG_G_BIT);

  ESP_LOGVV(TAG, "Writing 0x%.4x to register 0x%.2x", data, CONFIG_REG);
  return this->write_byte_16(CONFIG_REG, data);
}

void VEML3235Sensor::update() {
  if (this->measurement_in_progress_) {
    ESP_LOGV(TAG, "'%s': Previous measurement still in progress; skipping update", this->get_name().c_str());
    return;
  }
  this->measurement_in_progress_ = true;
  this->read_and_publish_(MAX_ADJUSTMENTS_PER_UPDATE);
}

void VEML3235Sensor::read_and_publish_(uint8_t adjustments_left) {
  uint8_t als_regs[] = {0, 0};
  if ((this->read_register(ALS_REG, als_regs, sizeof als_regs) != i2c::ERROR_OK)) {
    this->status_set_warning();
    this->publish_state(NAN);
    this->measurement_in_progress_ = false;
    return;
  }

  this->status_clear_warning();

  uint16_t als_counts = encode_uint16(als_regs[1], als_regs[0]);

  if (this->auto_gain_ && adjustments_left > 0) {
    // A sample integrated with the previous settings may still be in the data register after the
    // configuration changes, so wait out the old integration period plus two new ones before re-reading
    const uint32_t old_integration_time_ms = this->integration_time_ms_();
    if (this->adjust_sensitivity_(als_counts)) {
      const uint32_t wait_ms = old_integration_time_ms + 2 * this->integration_time_ms_();
      this->set_timeout("reread", wait_ms,
                        [this, adjustments_left]() { this->read_and_publish_(adjustments_left - 1); });
      return;
    }
  }

  float lux = this->counts_to_lux_(als_counts);
  ESP_LOGVV(TAG, "'%s': ALS counts = %u, sensitivity = %ux", this->get_name().c_str(), als_counts,
            this->sensitivity_factor_());
  ESP_LOGV(TAG, "'%s': Illuminance = %.4flx", this->get_name().c_str(), lux);
  this->publish_state(lux);
  this->measurement_in_progress_ = false;
}

float VEML3235Sensor::counts_to_lux_(uint16_t counts) const {
  float resolution = LUX_MULTIPLIER_BASE * (float(MAX_SENSITIVITY_FACTOR) / float(this->sensitivity_factor_()));
  return float(counts) * resolution;
}

uint8_t VEML3235Sensor::gain_factor_() const {
  switch (this->gain_) {
    case VEML3235_GAIN_4X:
      return 4;
    case VEML3235_GAIN_2X:
      return 2;
    default:
      return 1;
  }
}

uint16_t VEML3235Sensor::sensitivity_factor_() const {
  const uint8_t digital_gain_factor = this->digital_gain_ == VEML3235_DIGITAL_GAIN_2X ? 2 : 1;
  return (1 << this->integration_time_) * this->gain_factor_() * digital_gain_factor;
}

void VEML3235Sensor::set_sensitivity_factor_(uint16_t factor) {
  // The factor is a power of two in [1, 128]. Prefer integration time (improves the signal-to-noise ratio),
  // then analog gain; digital gain is a plain doubling of the output and is used only as a last resort.
  uint8_t it_exponent = 0;  // integration time is 2^n * 50 ms
  while (it_exponent < VEML3235_INTEGRATION_TIME_800MS && (1u << it_exponent) < factor) {
    it_exponent++;
  }
  this->integration_time_ = static_cast<VEML3235ComponentIntegrationTime>(it_exponent);
  factor >>= it_exponent;

  if (factor >= 4) {
    this->gain_ = VEML3235_GAIN_4X;
    factor >>= 2;
  } else if (factor == 2) {
    this->gain_ = VEML3235_GAIN_2X;
    factor >>= 1;
  } else {
    this->gain_ = VEML3235_GAIN_1X;
  }

  this->digital_gain_ = factor >= 2 ? VEML3235_DIGITAL_GAIN_2X : VEML3235_DIGITAL_GAIN_1X;
}

bool VEML3235Sensor::adjust_sensitivity_(uint16_t counts) {
  // Test for clipping before the window test: with an upper threshold configured at or above the clip
  // point, a saturated reading would otherwise count as "in window" and sensitivity would never recover
  const bool clipped = counts >= CLIPPED_COUNTS;
  const uint16_t low = uint16_t(UINT16_MAX * this->auto_gain_threshold_low_);
  const uint16_t high = uint16_t(UINT16_MAX * this->auto_gain_threshold_high_);
  if (!clipped && counts >= low && counts <= high) {
    return false;
  }

  const uint16_t current_factor = this->sensitivity_factor_();
  uint16_t new_factor;
  if (clipped) {
    new_factor = 1;
  } else if (counts == 0) {
    new_factor = MAX_SENSITIVITY_FACTOR;
  } else {
    // Counts scale linearly with the sensitivity factor: in one step, pick the power of two that puts the
    // next reading closest below the middle of the configured window. Rounding down means the target is
    // never overshot, which also keeps the sensitivity stable when the window is narrower than one step.
    float desired = float(current_factor) * ((float(low) + float(high)) * 0.5f / float(counts));
    desired = clamp(desired, 1.0f, float(MAX_SENSITIVITY_FACTOR));
    new_factor = 1;
    while (new_factor * 2 <= uint16_t(desired)) {
      new_factor *= 2;
    }
  }

  if (new_factor == current_factor) {
    return false;
  }

  const VEML3235ComponentIntegrationTime old_integration_time = this->integration_time_;
  const VEML3235ComponentGain old_gain = this->gain_;
  const VEML3235ComponentDigitalGain old_digital_gain = this->digital_gain_;

  this->set_sensitivity_factor_(new_factor);
  if (!this->refresh_config_reg()) {
    // Keep our state consistent with the device, which still has the old configuration
    this->integration_time_ = old_integration_time;
    this->gain_ = old_gain;
    this->digital_gain_ = old_digital_gain;
    this->status_set_warning();
    return false;
  }

  ESP_LOGV(TAG, "'%s': Sensitivity adjusted from %ux to %ux (ALS counts = %u)", this->get_name().c_str(),
           current_factor, new_factor, counts);
  return true;
}

void VEML3235Sensor::dump_config() {
  const uint8_t digital_gain = this->digital_gain_ == VEML3235_DIGITAL_GAIN_2X ? 2 : 1;

  LOG_SENSOR("", "VEML3235", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Auto-gain enabled: %s", YESNO(this->auto_gain_));
  if (this->auto_gain_) {
    ESP_LOGCONFIG(TAG,
                  "  Auto-gain thresholds:\n"
                  "    Upper: %.0f%%\n"
                  "    Lower: %.0f%%\n"
                  "  Values below will be used as initial values only",
                  this->auto_gain_threshold_high_ * 100.0f, this->auto_gain_threshold_low_ * 100.0f);
  }
  ESP_LOGCONFIG(TAG,
                "  Digital gain: %uX\n"
                "  Gain: %uX\n"
                "  Integration time: %ums",
                digital_gain, this->gain_factor_(), this->integration_time_ms_());
}

}  // namespace esphome::veml3235
