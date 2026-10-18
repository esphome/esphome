#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "mq_math.h"

namespace esphome::mq_gas_sensors {

/// One MQ gas sensor: samples the analog output, converts it into the RS/R0
/// ratio and evaluates the built-in regression curve (`PPM = a * ratio^b`).
/// R0 comes from the configuration or from a clean air calibration at boot.
class MQGasSensor : public sensor::Sensor, public PollingComponent {
 public:
  explicit MQGasSensor(voltage_sampler::VoltageSampler *source) : source_(source) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_type(const char *type) { this->type_ = StringRef(type); }
  void set_gas(const char *gas) { this->gas_ = StringRef(gas); }
  void set_a(float a) { this->a_ = a; }
  void set_b(float b) { this->b_ = b; }
  void set_regression_method(uint8_t method) { this->regression_method_ = method; }
  void set_rl(float rl) { this->rl_ = rl; }
  void set_r0(float r0) {
    this->r0_ = r0;
    this->r0_configured_ = true;
  }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_ratio_in_clean_air(float ratio) { this->ratio_in_clean_air_ = ratio; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_min_ppm(float ppm) { this->min_ppm_ = ppm; }
  void set_max_ppm(float ppm) { this->max_ppm_ = ppm; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  /// Enable the clean-air calibration at boot: it starts `delay` ms from now and
  /// averages `samples` readings spaced `sample_interval` ms apart.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval);

  bool has_r0() const { return this->r0_ > 0.0f; }
  float get_r0() const { return this->r0_; }

 protected:
  /// Averaged, scaled analog output voltage (V); sampled back to back.
  float sample_voltage_();
  /// RS in kOhm for `voltage`.
  float current_rs_(float voltage) const;
  /// RS/R0 ratio for `rs`.
  float current_ratio_(float rs) const;
  /// Full MQUnifiedsensor pipeline for `ratio`; NAN when invalid.
  float read_ppm_(float ratio) const;

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();

  StringRef type_{StringRef::from_lit("MQ")};
  StringRef gas_{StringRef::from_lit("CUSTOM")};
  voltage_sampler::VoltageSampler *source_{nullptr};

  float a_{0.0f};
  float b_{0.0f};
  uint8_t regression_method_{mqmath::REGRESSION_EXPONENTIAL};
  float rl_{10.0f};  ///< load resistor of the module (kOhm)
  float r0_{0.0f};   ///< sensor resistance in clean air (kOhm)
  float vcc_{5.0f};  ///< sensor supply (V)
  float ratio_in_clean_air_{0.0f};
  float voltage_multiplier_{1.0f};
  float min_ppm_{0.0f};
  float max_ppm_{10000.0f};
  uint8_t samples_{2};
  bool r0_configured_{false};

  bool calibration_enabled_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_samples_{10};
  /// Minimum spacing between two clean-air calibration samples (ms); a normal reading
  /// never waits for it.  The ~16 ms main loop tick is the practical floor.
  uint32_t sample_interval_{20};

  /// Scheduler id of the deferred calibration start.
  static constexpr uint32_t CALIBRATION_START_ID = 0;

  bool calibrating_{false};
  bool calibration_pending_{false};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  bool warned_no_r0_{false};
  bool warned_voltage_{false};
};

}  // namespace esphome::mq_gas_sensors
