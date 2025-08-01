#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"  // This defines ADC_CHANNEL_MAX
#endif                      // USE_ESP32

#ifdef USE_ZEPHYR
#include <zephyr/drivers/adc.h>
#endif

namespace esphome {
namespace adc {

#ifdef USE_ESP32
// clang-format off
#if (ESP_IDF_VERSION_MAJOR == 5 && \
     ((ESP_IDF_VERSION_MINOR == 0 && ESP_IDF_VERSION_PATCH >= 5) || \
      (ESP_IDF_VERSION_MINOR == 1 && ESP_IDF_VERSION_PATCH >= 3) || \
      (ESP_IDF_VERSION_MINOR >= 2)) \
    )
// clang-format on
static const adc_atten_t ADC_ATTEN_DB_12_COMPAT = ADC_ATTEN_DB_12;
#else
static const adc_atten_t ADC_ATTEN_DB_12_COMPAT = ADC_ATTEN_DB_11;
#endif
#endif  // USE_ESP32

enum class SamplingMode : uint8_t {
  AVG = 0,
  MIN = 1,
  MAX = 2,
};

const LogString *sampling_mode_to_str(SamplingMode mode);

template<typename T> class Aggregator {
 public:
  Aggregator(SamplingMode mode);
  void add_sample(T value);
  T aggregate();

 protected:
  T aggr_{0};
  uint8_t samples_{0};
  SamplingMode mode_{SamplingMode::AVG};
};

class ADCSensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
 public:
  /// Update the sensor's state by reading the current ADC value.
  /// This method is called periodically based on the update interval.
  void update() override;

  /// Set up the ADC sensor by initializing hardware and calibration parameters.
  /// This method is called once during device initialization.
  void setup() override;

  /// Output the configuration details of the ADC sensor for debugging purposes.
  /// This method is called during the ESPHome setup process to log the configuration.
  void dump_config() override;

  /// Return the setup priority for this component.
  /// Components with higher priority are initialized earlier during setup.
  /// @return A float representing the setup priority.
  float get_setup_priority() const override;

#ifdef USE_ZEPHYR
  /// Set the ADC channel to be used by the ADC sensor.
  /// @param channel Pointer to an adc_dt_spec structure representing the ADC channel.
  void set_adc_channel(const adc_dt_spec *channel) { this->channel_ = channel; }
#endif
  /// Set the GPIO pin to be used by the ADC sensor.
  /// @param pin Pointer to an InternalGPIOPin representing the ADC input pin.
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }

  /// Enable or disable the output of raw ADC values (unprocessed data).
  /// @param output_raw Boolean indicating whether to output raw ADC values (true) or processed values (false).
  void set_output_raw(bool output_raw) { this->output_raw_ = output_raw; }

  /// Set the number of samples to be taken for ADC readings to improve accuracy.
  /// A higher sample count reduces noise but increases the reading time.
  /// @param sample_count The number of samples (e.g., 1, 4, 8).
  void set_sample_count(uint8_t sample_count);

  /// Set the sampling mode for how multiple ADC samples are combined into a single measurement.
  ///
  /// When multiple samples are taken (controlled by set_sample_count), they can be combined
  /// in one of three ways:
  ///   - SamplingMode::AVG: Compute the average (default)
  ///   - SamplingMode::MIN: Use the lowest sample value
  ///   - SamplingMode::MAX: Use the highest sample value
  /// @param sampling_mode The desired sampling mode to use for aggregating ADC samples.
  void set_sampling_mode(SamplingMode sampling_mode);

  /// Perform a single ADC sampling operation and return the measured value.
  /// This function handles raw readings, calibration, and averaging as needed.
  /// @return The sampled value as a float.
  float sample() override;

#ifdef USE_ESP32
  /// Set the ADC attenuation level to adjust the input voltage range.
  /// This determines how the ADC interprets input voltages, allowing for greater precision
  /// or the ability to measure higher voltages depending on the chosen attenuation level.
  /// @param attenuation The desired ADC attenuation level (e.g., ADC_ATTEN_DB_0, ADC_ATTEN_DB_11).
  void set_attenuation(adc_atten_t attenuation) { this->attenuation_ = attenuation; }

  /// Configure the ADC to use a specific channel on a specific ADC unit.
  /// This sets the channel for single-shot or continuous ADC measurements.
  /// @param unit The ADC unit to use (ADC_UNIT_1 or ADC_UNIT_2).
  /// @param channel The ADC channel to configure, such as ADC_CHANNEL_0, ADC_CHANNEL_3, etc.
  void set_channel(adc_unit_t unit, adc_channel_t channel) {
    this->adc_unit_ = unit;
    this->channel_ = channel;
  }

  /// Set whether autoranging should be enabled for the ADC.
  /// Autoranging automatically adjusts the attenuation level to handle a wide range of input voltages.
  /// @param autorange Boolean indicating whether to enable autoranging.
  void set_autorange(bool autorange) { this->autorange_ = autorange; }
#endif  // USE_ESP32

#ifdef USE_RP2040
  void set_is_temperature() { this->is_temperature_ = true; }
#endif  // USE_RP2040

 protected:
  uint8_t sample_count_{1};
  bool output_raw_{false};
  InternalGPIOPin *pin_;
  SamplingMode sampling_mode_{SamplingMode::AVG};

#ifdef USE_ESP32
  float sample_autorange_();
  float sample_fixed_attenuation_();
  bool autorange_{false};
  adc_oneshot_unit_handle_t adc_handle_{nullptr};
  adc_cali_handle_t calibration_handle_{nullptr};
  adc_atten_t attenuation_{ADC_ATTEN_DB_0};
  adc_channel_t channel_{};
  adc_unit_t adc_unit_{};
  struct SetupFlags {
    uint8_t init_complete : 1;
    uint8_t config_complete : 1;
    uint8_t handle_init_complete : 1;
    uint8_t calibration_complete : 1;
    uint8_t reserved : 4;
  } setup_flags_{};
  static adc_oneshot_unit_handle_t shared_adc_handles[2];
#endif  // USE_ESP32

#ifdef USE_RP2040
  bool is_temperature_{false};
#endif  // USE_RP2040

#ifdef USE_ZEPHYR
  const struct adc_dt_spec *channel_ = nullptr;
#endif
};

}  // namespace adc
}  // namespace esphome
