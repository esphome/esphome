#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "as734xbase.h"

namespace esphome {
namespace as734x {

class AS734XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup_model(Model model);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;

  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }
  void set_led_enabled(bool enabled) { this->led_enabled_ = enabled; }

  void enable_led(bool enable);

  // calibration coeffs
  void set_dark_current_calibration(const CalibrationParams &vals);
  void set_channel_correction(const CalibrationParams &vals);
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  // normalization before publication
  void set_normalize_basic_counts(bool enable) { this->normalization_enabled_ = enable; }

 protected:
  Model model_{Model::AS7343};
  AS734xBase *device_{nullptr};

  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    IDLE,
    START_MEASUREMENT,
    CONFIGURE_SMUX,
    WAIT_SMUX,
    READ_DATA,
    DATA_COLLECTED,
    CALCULATE_CIE,
    CALCULATE_SPECTRAL,
    VALIDATE_VALUES,
    READY_TO_PUBLISH_PART_1,
    READY_TO_PUBLISH_PART_2,
    READY_TO_PUBLISH_PART_3
  } state_{State::NOT_INITIALIZED};

#ifdef USE_SENSOR

  // derived values
  SUB_SENSOR(illuminance);
  SUB_SENSOR(irradiance);
  SUB_SENSOR(irradiance_photopic);
  SUB_SENSOR(irradiance_par);
  SUB_SENSOR(ppfd);
  SUB_SENSOR(color_temperature);
  SUB_SENSOR(saturation_level);

 protected:
  SensorArray band_counts_sensors_{};
  SensorArray band_basic_counts_sensors_{};

 public:
  void set_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < this->device_->get_number_of_channels()) {
      band_counts_sensors_[channel] = sensor;
    }
  }
  void set_basic_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < this->device_->get_number_of_channels()) {
      band_basic_counts_sensors_[channel] = sensor;
    }
  }
#endif

#ifdef USE_TEXT_SENSOR
 protected:
  text_sensor::TextSensor *rgb_hex_sensor_{nullptr};
  char rgb_hex_str_[10]{0};

 public:
  void set_rgb_hex_sensor(text_sensor::TextSensor *sensor) { this->rgb_hex_sensor_ = sensor; }
#endif

 protected:
  uint16_t astep_;
  Gain gain_;
  uint8_t atime_;
  uint8_t number_of_channels_{MAX_CHANNELS};
  float glass_attenuation_factor_{1.0f};
  bool normalization_enabled_{false};
  bool led_enabled_{false};

  ChannelValuesFloat dark_current_offset_{};
  ChannelValuesFloat channel_correction_{};

  struct {
    ChannelValuesUint16 raw_counts{};

    bool saturated{false};
    Gain gain;
    uint8_t atime;
    uint16_t astep;

    float gain_x;
    float t_int_us;
    uint32_t millis_start;
    uint8_t smux_step;

    bool first_run{true};
    bool valid{false};
  } readings_;

  struct {
    ChannelValuesFloat basic_counts{};
    float max_basic_count;

    float lux_from_irradiance;
    float irradiance;
    float irradiance_photopic;
    float irradiance_par;
    float ppfd;
    float cct;
    float lux_from_xyz;
    float lux;
    float saturation_level;
  } calculated_values_;

  void calculate_();
  void calculate_basic_counts_();
  void calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance, float &irradiance_photopic);
  void calculate_color_(float &cct, float &lux);

  void publish_channel_readings_();
  void publish_basic_counts_();
  void publish_derived_readings_();

  float get_gain_multiplier_(Gain gain);
  inline float get_t_int_us_(uint16_t atime, uint16_t astep);

  template<typename T, size_t N> T get_highest_value_(std::array<T, N> &data);
  uint16_t get_maximum_spectral_adc_(uint16_t atime, uint16_t astep);

#ifdef USE_SENSOR
  inline void publish_sensor_(sensor::Sensor *sensor, float value);
#endif
};

}  // namespace as734x
}  // namespace esphome
