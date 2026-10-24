#include "mq_gas_sensor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mq_gas_sensors {

ESPHOME_LOG_TAG(TAG, "mq_gas_sensors");

/// Version tag mixed into the preference key of the persisted R0 value.
static constexpr uint32_t R0_PREFERENCE_VERSION = 0x00000001;

/// A sensor whose resistance collapses drives its analog output to its supply, and
/// the supply rail can sit this much above the configured `vcc:` (a 5.1 V USB rail
/// against the default 5.0 V). A reading within the tolerance is the top of the
/// measuring range; above it the reading cannot come from the configured sensor.
static constexpr float SATURATION_TOLERANCE = 0.05f;

/// A linked temperature/humidity source that has not delivered a state for this
/// long is treated as missing. The component polls every 60 s by default, so a
/// source that is alive normally updates several times inside the window; the
/// floor avoids flagging a slower source during its first interval.
static constexpr uint32_t SOURCE_STALE_MS = 5 * 60 * 1000;

/// Human readable name of a `mqmath::RegressionMethod` value.
static const LogString *regression_method_name(mqmath::RegressionMethod method) {
  switch (method) {
    case mqmath::REGRESSION_LINEAR:
      return LOG_STR("linear");
    case mqmath::REGRESSION_INVERSE:
      return LOG_STR("inverse");
    default:
      return LOG_STR("exponential");
  }
}

#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
void MQGasSensor::publish_log_(LogSeverity level, const char *message) {
  if (this->log_sensor_ == nullptr)
    return;
  if (level == LOG_SEVERITY_DEBUG) {
    // The per-update line: at a 1 s update interval a text state per second would
    // flood the Home Assistant recorder, so it is mirrored at most every
    // LOG_SENSOR_DEBUG_INTERVAL_MS (the console log keeps every line).
    const uint32_t now = App.get_loop_component_start_time();
    if (this->last_log_sensor_debug_ != 0 && now - this->last_log_sensor_debug_ < LOG_SENSOR_DEBUG_INTERVAL_MS)
      return;
    this->last_log_sensor_debug_ = now;
  }
  this->log_sensor_->publish_state(message);
}
#endif

void MQGasSensor::log_message_(LogSeverity level, const LogString *format, ...) {
  char message[LOG_BUFFER_SIZE];
  // A failed `snprintf` leaves the buffer untouched, and the message below is both
  // logged and published as a string, so it must never be uninitialized.
  message[0] = '\0';
  const int prefix = std::snprintf(message, sizeof(message), "'%s %s': ", this->type_.c_str(), this->gas_.c_str());
  // A failed or truncated prefix must not take the message with it: the body is what
  // carries the fault, so it is formatted from the start of the buffer when the
  // prefix does not leave room for it.
  size_t used = 0;
  if (prefix > 0 && static_cast<size_t>(prefix) < sizeof(message) - 1) {
    used = static_cast<size_t>(prefix);
  } else {
    message[0] = '\0';
  }
  va_list args;
  va_start(args, format);
#ifdef USE_ESP8266
  // The format is in flash (see LOG_STR), so it is read from there with the
  // `_P` variant - the same call the logger itself makes.
  vsnprintf_P(message + used, sizeof(message) - used, LOG_STR_ARG(format), args);
#else
  std::vsnprintf(message + used, sizeof(message) - used, LOG_STR_ARG(format), args);
#endif
  va_end(args);

  switch (level) {
    case LOG_SEVERITY_ERROR:
      ESP_LOGE(TAG, "%s", message);
      break;
    case LOG_SEVERITY_WARN:
      ESP_LOGW(TAG, "%s", message);
      break;
    case LOG_SEVERITY_INFO:
      ESP_LOGI(TAG, "%s", message);
      break;
    default:
      ESP_LOGD(TAG, "%s", message);
      break;
  }

#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  this->publish_log_(level, message);
#endif
}

void MQGasSensor::log_reading_(float ppm) {
  if (!this->log_ppm_ || !std::isfinite(ppm))
    return;
  // Console only: the detailed chain is what reaches `log_sensor_` (rate
  // limited there), and a text state per second would flood the recorder.
  ESP_LOGI(TAG, "'%s %s': %.1f ppm", this->type_.c_str(), this->gas_.c_str(), ppm);
}

void MQGasSensor::setup() {
  // The wiring hash is folded into the key so that the stored R0 of a different
  // supply, scaling or load resistor is never read back.
  this->r0_pref_ = this->make_entity_preference<float>(R0_PREFERENCE_VERSION ^ this->preference_key_,
                                                       /*in_flash=*/true);

  if (this->r0_configured_) {
    this->log_message_(LOG_SEVERITY_INFO, LOG_STR("using the R0 from the configuration (%.4f kOhm)"), this->r0_);
  } else if (this->persist_ && this->load_r0_()) {
    this->log_message_(LOG_SEVERITY_INFO, LOG_STR("restored R0 from preferences (%.4f kOhm)"), this->r0_);
  }
  this->publish_r0_();

  if (this->warmup_time_ > 0) {
    this->warmup_start_ = App.get_loop_component_start_time();
    this->log_message_(LOG_SEVERITY_INFO,
                       LOG_STR("warm-up/burn-in of %" PRIu32 " s - no readings published before that"),
                       this->warmup_time_ / 1000);
  }

  if (this->calibration_enabled_) {
    if (this->has_r0()) {
      this->log_message_(LOG_SEVERITY_INFO,
                         LOG_STR("R0 already known (%.4f kOhm) - automatic calibration skipped (clear the stored R0 to "
                                 "recalibrate)"),
                         this->r0_);
    } else {
      // The warm-up window starts here, so all of it still lies ahead of the calibration.
      const uint32_t delay = std::max(this->calibration_delay_, this->warmup_time_);
      this->calibration_pending_ = true;
      this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
      this->log_message_(LOG_SEVERITY_INFO,
                         LOG_STR("R0 calibration scheduled in %" PRIu32 " s - the sensor must be in clean air"),
                         delay / 1000);
    }
  } else if (!this->has_r0()) {
    // No R0 will ever appear in this configuration, so the component is degraded for
    // its whole life: keep the status warning up, not only the log line.
    this->r0_warning_ = LOG_STR("no R0 available - set 'r0:' or 'calibration:' in the configuration");
    this->update_status_();
    this->log_message_(
        LOG_SEVERITY_WARN,
        LOG_STR("no R0 available, set 'r0:' or 'calibration:' - the PPM value stays unknown until then"));
  }

  // Track when the correction sources last delivered a finite state. A sensor
  // keeps `has_state()` after it stops updating, so the timestamp is what tells
  // a live source from a frozen one.
  const uint32_t now = App.get_loop_component_start_time();
  if (this->temperature_source_ != nullptr) {
    this->temperature_source_->add_on_raw_state_callback([this](float value) {
      this->temperature_last_ms_ = App.get_loop_component_start_time();
      this->temperature_seen_ = std::isfinite(value);
    });
    if (this->temperature_source_->has_state()) {
      this->temperature_last_ms_ = now;
      this->temperature_seen_ = std::isfinite(this->temperature_source_->state);
    }
  }
  if (this->humidity_source_ != nullptr) {
    this->humidity_source_->add_on_raw_state_callback([this](float value) {
      this->humidity_last_ms_ = App.get_loop_component_start_time();
      this->humidity_seen_ = std::isfinite(value);
    });
    if (this->humidity_source_->has_state()) {
      this->humidity_last_ms_ = now;
      this->humidity_seen_ = std::isfinite(this->humidity_source_->state);
    }
  }

  this->disable_loop();
}

void MQGasSensor::log_config_() {
  ESP_LOGCONFIG(TAG, "  Type: %s", this->type_.c_str());
  ESP_LOGCONFIG(TAG, "  Target gas: %s", this->gas_.c_str());
  ESP_LOGCONFIG(TAG, "  Curve: %s (a=%.6g, b=%.6g), ratio: %s",
                LOG_STR_ARG(regression_method_name(this->regression_method_)), this->a_, this->b_,
                this->ratio_mode_ == mqmath::RATIO_R0_RS ? LOG_STR_LITERAL("R0/RS (MQUnifiedsensor)")
                                                         : LOG_STR_LITERAL("RS/R0 (datasheet)"));
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, RL: %.2f kOhm, AO multiplier: x%.3f", this->vcc_, this->rl_,
                this->voltage_multiplier_);
  ESP_LOGCONFIG(TAG, "  Range: %.1f - %.1f ppm, AO samples per reading: %u, calibration spacing: %" PRIu32 " ms",
                this->min_ppm_, this->max_ppm_, static_cast<unsigned>(this->samples_), this->sample_interval_);
  if (this->has_r0()) {
    ESP_LOGCONFIG(TAG, "  R0: %.4f kOhm (%s)", this->r0_,
                  this->r0_configured_ ? LOG_STR_LITERAL("configured") : LOG_STR_LITERAL("calibrated/restored"));
  } else {
    ESP_LOGCONFIG(TAG, "  R0: not available yet");
  }
  ESP_LOGCONFIG(TAG, "  RS/R0 in clean air: %.2f, correction factor: %.4f", this->ratio_in_clean_air_,
                this->correction_factor_);
  ESP_LOGCONFIG(TAG, "  Warm-up: %" PRIu32 " s, auto calibration: %s", this->warmup_time_ / 1000,
                this->calibration_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));

  const bool tc_enabled = this->correction_mode_ == mqmath::CORRECTION_MQDATASCIENCE;
  ESP_LOGCONFIG(TAG, "  Temperature/humidity correction: %s",
                tc_enabled ? LOG_STR_LITERAL("mqdatascience (ratio / (a + c * exp(b * T)))") : LOG_STR_LITERAL("none"));
  if (tc_enabled) {
    ESP_LOGCONFIG(TAG, "  Correction constants (RH 33%%/85%%): a %.4f/%.4f, b %.4f/%.4f, c %.4f/%.4f", this->tc_.a33,
                  this->tc_.a85, this->tc_.b33, this->tc_.b85, this->tc_.c33, this->tc_.c85);
    ESP_LOGCONFIG(TAG, "  Correction sources: temperature %s, humidity %s, clamp: %s",
                  this->temperature_source_ != nullptr ? LOG_STR_LITERAL("wired") : LOG_STR_LITERAL("MISSING"),
                  this->humidity_source_ != nullptr ? LOG_STR_LITERAL("wired") : LOG_STR_LITERAL("MISSING"),
                  this->correction_clamp_ == mqmath::CLAMP_SCALED ? LOG_STR_LITERAL("max_ppm * correction")
                                                                  : LOG_STR_LITERAL("absolute max_ppm"));
  }
}

void MQGasSensor::dump_config() {
  LOG_SENSOR("", "MQ gas sensor", this);
  LOG_UPDATE_INTERVAL(this);
  this->log_config_();
}

float MQGasSensor::sample_voltage_() {
  float sum = 0.0f;
  this->sample_count_ = 0;
  for (uint8_t i = 0; i < this->samples_; i++) {
    const float sample = this->source_->sample();
    // A conversion the sampler could not complete is not a measurement: drop it
    // instead of letting one NaN poison the average of the whole window.
    if (!std::isfinite(sample))
      continue;
    sum += sample;
    this->sample_count_++;
  }
  if (this->sample_count_ == 0)
    return NAN;
  return (sum / static_cast<float>(this->sample_count_)) * this->voltage_multiplier_;
}

float MQGasSensor::current_rs_() const { return mqmath::rs_from_voltage(this->sensor_voltage_, this->vcc_, this->rl_); }

float MQGasSensor::current_ratio_() const {
  return mqmath::ratio_from_rs(this->rs_, this->r0_, this->correction_factor_, this->ratio_mode_);
}

float MQGasSensor::compute_correction_() {
#ifdef USE_MQ_GAS_SENSORS_CORRECTION
  if (this->correction_mode_ != mqmath::CORRECTION_MQDATASCIENCE)
    return 1.0f;

  if (this->temperature_source_ == nullptr || this->humidity_source_ == nullptr) {
    this->correction_degraded_ = true;
    if (!this->warned_correction_) {
      this->warned_correction_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("temperature/humidity correction is enabled but a source is missing - "
                                 "publishing the uncorrected value"));
    }
    this->update_status_();
    return 1.0f;
  }

  // A source that has no state yet, holds a non-finite value, or stopped
  // updating cannot contribute; the last good value must not silently keep the
  // correction alive.
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t max_age = std::max(SOURCE_STALE_MS, this->get_update_interval() * 3u);
  const bool temperature_ok =
      mqmath::source_is_fresh(this->temperature_seen_, this->temperature_last_ms_, now, max_age) &&
      std::isfinite(this->temperature_source_->state);
  const bool humidity_ok = mqmath::source_is_fresh(this->humidity_seen_, this->humidity_last_ms_, now, max_age) &&
                           std::isfinite(this->humidity_source_->state);
  if (!temperature_ok || !humidity_ok) {
    this->correction_degraded_ = true;
    if (!this->warned_correction_) {
      this->warned_correction_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("no recent temperature/humidity reading (T=%.1f, RH=%.1f) - "
                                 "publishing the uncorrected value"),
                         this->temperature_source_->state, this->humidity_source_->state);
    }
    this->update_status_();
    return 1.0f;
  }

  const float temperature = this->temperature_source_->state;
  const float humidity = this->humidity_source_->state;
  this->warned_correction_ = false;
  if (this->correction_degraded_) {
    this->correction_degraded_ = false;
    this->update_status_();
  }
  return mqmath::correction_coefficient(humidity, temperature, this->tc_);
#else
  // No configuration in this build enables a correction model, so the ratio is
  // used as it is read (`USE_MQ_GAS_SENSORS_CORRECTION` is emitted by `sensor.py`).
  return 1.0f;
#endif
}

float MQGasSensor::read_ppm_() {
  const float ratio = this->ratio_mode_ == mqmath::RATIO_R0_RS
                          ? mqmath::apply_correction_inverse(this->ratio_, this->correction_)
                          : mqmath::apply_correction(this->ratio_, this->correction_);
  const float ppm = mqmath::ppm_from_ratio(this->a_, this->b_, ratio, this->regression_method_);
  if (!std::isfinite(ppm))
    return NAN;
  return mqmath::clamp_ppm_corrected(ppm, this->min_ppm_, this->max_ppm_, this->correction_, this->correction_clamp_);
}

void MQGasSensor::publish_diagnostics_() {
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(this->sensor_voltage_);
  if (this->rs_sensor_ != nullptr)
    this->rs_sensor_->publish_state(this->rs_);
  if (this->ratio_sensor_ != nullptr)
    this->ratio_sensor_->publish_state(this->ratio_);
  if (this->correction_sensor_ != nullptr)
    this->correction_sensor_->publish_state(this->correction_);
}

void MQGasSensor::publish_r0_() {
  if (this->r0_sensor_ == nullptr || !this->has_r0())
    return;
  this->r0_sensor_->publish_state(this->r0_);
}

void MQGasSensor::publish_fault_diagnostics_() {
  // The voltage is a real reading even when no resistance can be derived from
  // it; RS, the ratio and the correction are not, and 0 kOhm would look like a
  // measurement.
  this->rs_ = NAN;
  this->ratio_ = NAN;
  this->correction_ = NAN;
  this->publish_diagnostics_();
}

void MQGasSensor::update() {
  if (this->is_failed())
    return;

  this->sensor_voltage_ = this->sample_voltage_();
  if (!std::isfinite(this->sensor_voltage_)) {
    // The sampler itself could not produce a conversion: this is not the same
    // fault as a wiring that reads 0 V, and pointing at the AO wiring would hide
    // a failed ADC or I2C device.
    if (!this->warned_sampler_failure_) {
      this->warned_sampler_failure_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("the voltage sampler returned no reading - check the sampler (I2C bus, ADC)"));
    }
    this->wiring_warning_ = LOG_STR("the voltage sampler returned no reading");
    this->update_status_();
    this->publish_fault_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  if (this->sample_count_ < this->samples_) {
    // A sampler that answers only part of the window is degrading without failing
    // outright; the average still uses what came back, so say so once.
    if (!this->warned_partial_window_) {
      this->warned_partial_window_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("only %u of %u conversions returned a reading - check the sampler (I2C bus, ADC)"),
                         static_cast<unsigned>(this->sample_count_), static_cast<unsigned>(this->samples_));
    }
  } else {
    this->warned_partial_window_ = false;
  }
  if (!this->voltage_usable_(this->sensor_voltage_)) {
    if (!this->warned_low_voltage_) {
      this->warned_low_voltage_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("analog output reads %.4f V - open circuit, missing supply or bad "
                                 "divider? check the AO wiring (RS would be invalid)"),
                         this->sensor_voltage_);
    }
    // Raised on every update while the fault lasts, not only when it appears: any
    // other warning that cleared the status must not hide it.
    this->wiring_warning_ = LOG_STR("analog output reads at or below 0.01 V - check the AO wiring");
    this->update_status_();
    this->publish_fault_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  bool saturated = false;
  if (this->sensor_voltage_ >= this->vcc_) {
    // A sensor whose resistance collapses drives its output to the supply, and the
    // supply itself can sit a few percent above the configured `vcc:`. Up to the
    // tolerance that is the top of the measuring range; beyond it the reading cannot
    // come from the sensor the configuration describes.
    saturated = this->sensor_voltage_ <= this->vcc_ * (1.0f + SATURATION_TOLERANCE);
    if (!saturated) {
      if (!this->warned_high_voltage_) {
        this->warned_high_voltage_ = true;
        this->log_message_(LOG_SEVERITY_WARN,
                           LOG_STR("analog output reads %.4f V, above the configured vcc (%.2f V) by more than "
                                   "%.0f %% - check 'vcc:' and the divider"),
                           this->sensor_voltage_, this->vcc_, SATURATION_TOLERANCE * 100.0f);
      }
      // See the low-voltage branch: the status warning is a state, not an event.
      this->wiring_warning_ = LOG_STR("analog output is far above vcc - check 'vcc:' and the divider");
      this->update_status_();
      this->publish_fault_diagnostics_();
      this->publish_state(NAN);
      return;
    }
  }
  // The wiring reads as expected again, so both faults are over and the status
  // warning raised for them goes away with them. A calibration warning is not
  // this component's to clear, so only a wiring warning is cleared here.
  this->warned_sampler_failure_ = false;
  this->warned_low_voltage_ = false;
  this->warned_high_voltage_ = false;
  if (this->wiring_warning_ != nullptr) {
    this->wiring_warning_ = nullptr;
    this->update_status_();
  }

  // These states publish no measurement, but the wiring checks above still ran:
  // they are what tells a dead input from a misconfigured one. Only the ratio and
  // the ppm below wait for R0.
  if (this->calibrating_) {
    // The persisted R0 is about to change: never keep a value that was computed
    // with the previous one.  The message also reaches `log_sensor:`.
    this->publish_state(NAN);
    this->log_message_(LOG_SEVERITY_DEBUG,
                       LOG_STR("update skipped, a calibration is running - the sensor must be in clean air"));
    return;
  }

  if (this->calibration_pending_) {
    if (!this->has_r0()) {
      // There is no reference to publish with: wait for the calibration.
      this->publish_state(NAN);
      return;
    }
    // A retry is pending, but the R0 of this boot (or from preferences) is still
    // a valid reference: keep publishing instead of blanking the sensor until the
    // retry runs.
  }

  if (this->in_warmup_()) {
    // Published on every pass, so a value computed before a restart never survives
    // the warm-up window. `setup()` logged the window, so nothing repeats here.
    this->publish_state(NAN);
    return;
  }

  if (!this->has_r0()) {
    if (!this->warned_no_r0_) {
      this->warned_no_r0_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("R0 unknown - publish 'calibration:' or 'r0:' in the configuration"));
    }
    // The voltage and RS are real readings even without R0, so the diagnostics
    // that show the wiring keep working; the ratio and the ppm need R0 and stay
    // unknown until it is known. At the supply rail RS has collapsed, so the same 0
    // a saturated reading with R0 reports is used instead of the negative RS that
    // `current_rs_()` would derive from the voltage.
    this->rs_ = saturated ? 0.0f : this->current_rs_();
    this->ratio_ = NAN;
    this->correction_ = NAN;
    this->publish_diagnostics_();
    this->publish_state(NAN);
    return;
  }

  if (saturated) {
    // RS has collapsed to 0, so the ratio is 0 and the curve reports the top of its
    // range: a real high concentration must not look like a broken sensor. Go
    // through the same clamp as a normal reading so `correction_clamp: scaled`
    // moves the ceiling the same way in both paths.
    this->rs_ = 0.0f;
    this->ratio_ = 0.0f;
    this->correction_ = this->compute_correction_();
    const float ppm = mqmath::clamp_ppm_corrected(FLT_MAX, this->min_ppm_, this->max_ppm_, this->correction_,
                                                  this->correction_clamp_);
    // A ratio fault from an earlier update is over: this is a valid reading.
    this->clear_reading_warnings_();
    this->log_reading_(ppm);
    this->warn_ceiling_(ppm);
    this->log_message_(LOG_SEVERITY_DEBUG,
                       LOG_STR("V=%.4f V is at the supply - the sensor is saturated, reporting the top of the range"),
                       this->sensor_voltage_);
    this->publish_diagnostics_();
    this->publish_state(ppm);
    return;
  }

  this->rs_ = this->current_rs_();
  this->ratio_ = this->current_ratio_();
  this->correction_ = this->compute_correction_();
  const float ppm = this->read_ppm_();

  if (!std::isfinite(ppm)) {
    // The wiring reads fine but the ratio is unusable: the curve cannot evaluate it
    // for the coefficients, mode and correction in use. Publishing unknown in
    // silence would look like a sensor that simply reads nothing.
    if (!this->warned_unusable_ratio_) {
      this->warned_unusable_ratio_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("V=%.4f V, RS=%.4f kOhm gives ratio %.4f (correction %.4f), which the curve cannot "
                                 "evaluate - check 'correction_factor:', the coefficients and the correction sources"),
                         this->sensor_voltage_, this->rs_, this->ratio_, this->correction_);
    }
    // Raised on every update, not only the first one: every reading is unknown for
    // as long as the ratio stays unusable, and nothing else may report the
    // component as healthy while that is the case.
    this->update_status_();
    this->publish_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  this->clear_reading_warnings_();
  this->warn_ceiling_(ppm);

  this->log_reading_(ppm);
  this->log_message_(LOG_SEVERITY_DEBUG, LOG_STR("V=%.4f V, RS=%.4f kOhm, ratio=%.4f (correction=%.4f) -> %.1f ppm"),
                     this->sensor_voltage_, this->rs_, this->ratio_, this->correction_, ppm);

  this->publish_diagnostics_();
  this->publish_state(ppm);
}

void MQGasSensor::update_status_() {
  // The status slot holds one warning. Restate the most important one still
  // current so that clearing a fault never hides another active fault.
  if (this->wiring_warning_ != nullptr) {
    this->status_set_warning(this->wiring_warning_);
  } else if (this->r0_warning_ != nullptr) {
    this->status_set_warning(this->r0_warning_);
  } else if (this->warned_unusable_ratio_) {
    this->status_set_warning(
        LOG_STR("the ratio is not a measurement - check 'correction_factor:' and the coefficients"));
  } else if (this->correction_degraded_) {
    this->status_set_warning(LOG_STR("T/RH compensation degraded - using the uncorrected reading"));
  } else {
    this->status_clear_warning();
  }
}

void MQGasSensor::clear_reading_warnings_() {
  // A valid reading ends the faults an earlier update reported. The saturated
  // branch publishes the top of the range without going through the curve, so it
  // has to clear the ratio fault itself. A failed manual recalibration only
  // concerned that attempt: the R0 still in use is valid, so a published reading
  // ends its warning too.
  bool changed = false;
  if (this->warned_unusable_ratio_) {
    this->warned_unusable_ratio_ = false;
    changed = true;
  }
  if (this->r0_warning_clears_on_reading_) {
    this->r0_warning_clears_on_reading_ = false;
    this->r0_warning_ = nullptr;
    changed = true;
  }
  if (changed)
    this->update_status_();
}

void MQGasSensor::warn_ceiling_(float ppm) {
  // The value is clamped to the range and a saturated sensor reports its ceiling, so
  // say so once: neither may pass for a measurement at that value.
  const bool at_ceiling = ppm >= this->max_ppm_;
  if (at_ceiling == this->warned_over_range_)
    return;
  this->warned_over_range_ = at_ceiling;
  if (at_ceiling)
    this->log_message_(LOG_SEVERITY_WARN, LOG_STR("V=%.4f V is beyond %.1f ppm - reporting the top of the range"),
                       this->sensor_voltage_, this->max_ppm_);
}

void MQGasSensor::set_calibration(bool enabled, float ratio_in_clean_air, uint32_t delay, uint32_t duration,
                                  uint32_t samples, bool persist) {
  this->calibration_enabled_ = enabled;
  this->ratio_in_clean_air_ = ratio_in_clean_air;
  this->calibration_delay_ = delay;
  this->calibration_duration_ = duration;
  this->calibration_samples_ = samples;
  this->persist_ = persist;
}

void MQGasSensor::start_calibration() {
  if (this->calibrating_) {
    this->log_message_(LOG_SEVERITY_WARN, LOG_STR("a calibration is already running"));
    return;
  }
  if (this->calibration_pending_) {
    // A retry is waiting for its timer: this request replaces it.  The sampling
    // window itself is the only state a new request may not interrupt.
    this->cancel_timeout(CALIBRATION_START_ID);
    this->calibration_pending_ = false;
  }
  if (this->ratio_in_clean_air_ <= 0.0f) {
    this->log_message_(LOG_SEVERITY_ERROR,
                       LOG_STR("cannot calibrate without an RS/R0 in clean air - set 'ratio_in_clean_air:'"));
    return;
  }

  // A sensor that is still warming up has not settled: sample after the rest of
  // the window instead of turning an unsettled run into the new R0.
  if (this->in_warmup_()) {
    const uint32_t delay = this->warmup_start_ + this->warmup_time_ - App.get_loop_component_start_time();
    this->calibration_pending_ = true;
    this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
    this->log_message_(LOG_SEVERITY_INFO, LOG_STR("calibration requested, starting in %" PRIu32 " s (warm-up)"),
                       delay / 1000);
    return;
  }

  this->begin_calibration_();
}

void MQGasSensor::loop() {
  if (this->is_failed() || !this->calibrating_)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->calibration_last_sample_ < this->sample_interval_)
    return;

  this->calibration_last_sample_ = now;
  this->process_calibration_();
}

void MQGasSensor::begin_calibration_() {
  this->calibration_pending_ = false;
  this->calibrating_ = true;
  this->calibration_start_ = App.get_loop_component_start_time();
  this->calibration_last_sample_ = this->calibration_start_;
  this->calibration_count_ = 0;
  this->calibration_attempts_ = 0;
  this->calibration_rejected_low_ = 0;
  this->calibration_rejected_high_ = 0;
  this->calibration_rejected_nonfinite_ = 0;
  this->calibration_rejected_rs_ = 0;
  this->calibration_partial_ = 0;
  this->calibration_sum_ = 0.0f;
  this->calibration_min_ = 0.0f;
  this->calibration_max_ = 0.0f;
  this->calibration_rejection_logged_ = false;
  // A new attempt supersedes the warning of an earlier failed one.
  this->r0_warning_clears_on_reading_ = false;
  this->enable_loop();

  this->log_message_(
      LOG_SEVERITY_INFO,
      LOG_STR("calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the sensor in clean air"),
      this->ratio_in_clean_air_, this->calibration_samples_);
}

void MQGasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  // A window the sampler answered only in part is a second, quieter shape of the
  // same fault the rejection counts name: count it so the failure message can
  // point at the sampler instead of only at the wiring.
  if (this->sample_count_ < this->samples_)
    this->calibration_partial_++;
  bool rejected = false;
  if (!std::isfinite(voltage)) {
    this->calibration_rejected_nonfinite_++;
    rejected = true;
  } else if (!this->voltage_usable_(voltage)) {
    this->calibration_rejected_low_++;
    rejected = true;
  } else if (voltage >= this->vcc_) {
    this->calibration_rejected_high_++;
    rejected = true;
  } else {
    this->sensor_voltage_ = voltage;
    this->rs_ = this->current_rs_();
    const float r0 = mqmath::r0_from_clean_air(this->rs_, this->ratio_in_clean_air_);
    if (r0 > 0.0f) {
      if (this->calibration_count_ == 0) {
        this->calibration_min_ = r0;
        this->calibration_max_ = r0;
      } else {
        this->calibration_min_ = std::min(this->calibration_min_, r0);
        this->calibration_max_ = std::max(this->calibration_max_, r0);
      }
      this->calibration_sum_ += r0;
      this->calibration_count_++;
    } else {
      this->calibration_rejected_rs_++;
      rejected = true;
    }
  }

  if (rejected && !this->calibration_rejection_logged_) {
    // The final message names the causes; this line keeps the actual value of the
    // first rejected sample available for a wiring check without repeating it for
    // every sample.
    this->calibration_rejection_logged_ = true;
    this->log_message_(LOG_SEVERITY_DEBUG, LOG_STR("first rejected calibration sample reads %.4f V"), voltage);
  }

  const bool samples_done = this->calibration_attempts_ >= this->calibration_samples_;
  const bool timed_out = this->calibration_duration_ > 0 && (App.get_loop_component_start_time() -
                                                             this->calibration_start_) >= this->calibration_duration_;
  if (samples_done || timed_out)
    this->finish_calibration_();
}

void MQGasSensor::finish_calibration_() {
  this->calibrating_ = false;
  this->disable_loop();

  const uint32_t attempts = this->calibration_attempts_;
  const uint32_t valid = this->calibration_count_;
  const uint32_t required = std::min<uint32_t>(std::max<uint32_t>((attempts + 1) / 2, 2), attempts);

  if (valid < required) {
    // Name the reason each rejected sample failed, so a wrong `vcc:` is not
    // mistaken for a wiring fault just because the generic message says so.
    this->log_message_(LOG_SEVERITY_ERROR,
                       LOG_STR("calibration failed, %" PRIu32 " of %" PRIu32 " valid RS: %" PRIu32
                               " below %.2f V, %" PRIu32 " at/above vcc, %" PRIu32 " non-finite, %" PRIu32
                               " bad RS, %" PRIu32 " partial windows - check AO wiring and supply"),
                       valid, attempts, this->calibration_rejected_low_, MIN_VALID_VOLTAGE,
                       this->calibration_rejected_high_, this->calibration_rejected_nonfinite_,
                       this->calibration_rejected_rs_, this->calibration_partial_);
    if (!this->has_r0()) {
      this->schedule_calibration_retry_();
    } else {
      // A manual run failed but the R0 of this boot is still a usable reference:
      // keep publishing with it and do not retry forever. The warning stays up
      // until the next valid reading, which proves the old R0 still works; the
      // ERROR line above is the record of the failed attempt.
      this->r0_warning_ = LOG_STR("clean-air calibration failed - keeping the current R0");
      this->r0_warning_clears_on_reading_ = true;
      this->update_status_();
    }
    return;
  }

  // Every counted sample is finite and positive (`process_calibration_()`), so their
  // average is usable without another check.
  const float r0 = this->calibration_sum_ / static_cast<float>(valid);

  // A sensor that is still settling, or one that is not in clean air, varies too
  // much for its average to be the R0 of every later reading.
  if (!mqmath::calibration_is_stable(this->calibration_min_, this->calibration_max_, r0)) {
    this->log_message_(LOG_SEVERITY_WARN,
                       LOG_STR("clean-air samples spread %.4f kOhm around %.4f kOhm - the sensor was not settled in "
                               "clean air"),
                       this->calibration_max_ - this->calibration_min_, r0);
    if (!this->has_r0()) {
      this->schedule_calibration_retry_();
    } else {
      this->r0_warning_ = LOG_STR("clean-air samples were unstable - keeping the current R0");
      this->r0_warning_clears_on_reading_ = true;
      this->update_status_();
    }
    return;
  }

  this->r0_ = r0;
  this->publish_r0_();
  const bool stored = this->persist_ && this->save_r0_();
  if (!this->persist_ || stored) {
    // The calibration warning is over; a failed store keeps its own warning
    // (set in `save_r0_()`) so it stays visible until the next boot.
    this->r0_warning_ = nullptr;
  }
  this->update_status_();

  this->log_message_(LOG_SEVERITY_INFO, LOG_STR("R0 = %.4f kOhm (%" PRIu32 "/%" PRIu32 " samples valid)%s"), this->r0_,
                     valid, attempts,
                     stored           ? LOG_STR_LITERAL(", stored persistently")
                     : this->persist_ ? LOG_STR_LITERAL(", not stored")
                                      : LOG_STR_LITERAL(", kept until reboot"));
  if (!this->persist_) {
    this->log_message_(LOG_SEVERITY_INFO, LOG_STR("hard-code 'r0: %.4f' in the YAML to skip the calibration at boot"),
                       this->r0_);
  }
}

void MQGasSensor::schedule_calibration_retry_() {
  this->r0_warning_ = LOG_STR("clean-air calibration failed - retrying");
  this->update_status_();
  this->calibration_pending_ = true;
  // A delay of zero restarts the run straight away and logs the same failure again
  // about five times per second, so anything below the floor is raised to it.
  const uint32_t delay = std::max(this->calibration_delay_, CALIBRATION_RETRY_MIN_MS);
  this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
  this->log_message_(LOG_SEVERITY_WARN, LOG_STR("retrying the calibration in %" PRIu32 " s"), delay / 1000);
}

bool MQGasSensor::in_warmup_() {
  if (this->warmup_ended_)
    return false;
  if (App.get_loop_component_start_time() - this->warmup_start_ < this->warmup_time_)
    return true;
  // A window that has passed is over: latching it keeps the subtraction above from
  // reopening it once it overflows, ~49.7 days after the start.
  this->warmup_ended_ = true;
  return false;
}

bool MQGasSensor::save_r0_() {
  if (!this->r0_pref_.save(&this->r0_)) {
    this->r0_warning_ = LOG_STR("could not store R0");
    this->log_message_(LOG_SEVERITY_WARN, LOG_STR("could not store R0"));
    return false;
  }
  if (!global_preferences->sync()) {
    this->r0_warning_ = LOG_STR("R0 saved but the preference sync failed - it may not survive a reboot");
    this->log_message_(LOG_SEVERITY_WARN,
                       LOG_STR("R0 saved but the preference sync failed - it may not survive a reboot"));
    return false;
  }
  this->log_message_(LOG_SEVERITY_DEBUG, LOG_STR("R0 %.4f kOhm saved"), this->r0_);
  return true;
}

bool MQGasSensor::load_r0_() {
  float stored = 0.0f;
  if (!this->r0_pref_.load(&stored))
    return false;
  if (!std::isfinite(stored) || stored <= 0.0f) {
    this->log_message_(LOG_SEVERITY_WARN, LOG_STR("ignoring the stored R0 %.4f, calibrating again"), stored);
    return false;
  }
  this->r0_ = stored;
  return true;
}

}  // namespace esphome::mq_gas_sensors
