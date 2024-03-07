#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include "as7343_registers.h"

namespace esphome {
namespace as7343 {

class AS7343Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_f1_sensor(sensor::Sensor *f1_sensor) { f1_ = f1_sensor; }
  void set_f2_sensor(sensor::Sensor *f2_sensor) { f2_ = f2_sensor; }
  void set_fz_sensor(sensor::Sensor *fz_sensor) { fz_ = fz_sensor; }
  void set_f3_sensor(sensor::Sensor *f3_sensor) { f3_ = f3_sensor; }
  void set_f4_sensor(sensor::Sensor *f4_sensor) { f4_ = f4_sensor; }
  void set_fy_sensor(sensor::Sensor *fy_sensor) { fy_ = fy_sensor; }
  void set_f5_sensor(sensor::Sensor *f5_sensor) { f5_ = f5_sensor; }
  void set_fxl_sensor(sensor::Sensor *fxl_sensor) { fxl_ = fxl_sensor; }
  void set_f6_sensor(sensor::Sensor *f6_sensor) { f6_ = f6_sensor; }
  void set_f7_sensor(sensor::Sensor *f7_sensor) { f7_ = f7_sensor; }
  void set_f8_sensor(sensor::Sensor *f8_sensor) { f8_ = f8_sensor; }
  void set_nir_sensor(sensor::Sensor *nir_sensor) { nir_ = nir_sensor; }
  void set_clear_sensor(sensor::Sensor *clear_sensor) { clear_ = clear_sensor; }
  void set_illuminance_sensor(sensor::Sensor *sensor) { illuminance_ = sensor; }
  void set_irradiance_sensor(sensor::Sensor *sensor) { irradiance_ = sensor; }
  void set_ppfd_sensor(sensor::Sensor *sensor) { ppfd_ = sensor; }
  void set_saturation_sensor(sensor::Sensor *sensor) { this->saturated_ = sensor; }

  void set_gain(AS7343Gain gain) { gain_ = gain; }
  void set_atime(uint8_t atime) { atime_ = atime; }
  void set_astep(uint16_t astep) { astep_ = astep; }

  AS7343Gain get_gain();
  uint8_t get_atime();
  uint16_t get_astep();

  bool setup_gain(AS7343Gain gain);
  bool setup_atime(uint8_t atime);
  bool setup_astep(uint16_t astep);

  bool change_gain(AS7343Gain gain);

  float get_gain_multiplier(AS7343Gain gain);

  bool read_18_channels(std::array<uint16_t, AS7343_NUM_CHANNELS> &data);
  float calculate_ppfd(float tint_ms, float gain_x, AS7343Gain gain);
  void calculate_irradiance(float tint_ms, float gain_x, float &irradiance, float &lux, AS7343Gain gain);
  float calculate_spectre_();

  bool wait_for_data(uint16_t timeout = 1000);
  bool is_data_ready();
  bool enable_power(bool enable);
  bool enable_spectral_measurement(bool enable);

  bool read_register_bit(uint8_t address, uint8_t bit_position);
  bool write_register_bit(uint8_t address, bool value, uint8_t bit_position);
  bool set_register_bit(uint8_t address, uint8_t bit_position);
  bool clear_register_bit(uint8_t address, uint8_t bit_position);
  uint16_t swap_bytes(uint16_t data);

 protected:
  void set_bank_for_reg_(AS7343Registers reg = AS7343Registers::ENABLE);
  bool bank_{false};
  bool readings_saturated_{false};

  sensor::Sensor *f1_{nullptr};
  sensor::Sensor *f2_{nullptr};
  sensor::Sensor *fz_{nullptr};
  sensor::Sensor *f3_{nullptr};
  sensor::Sensor *f4_{nullptr};
  sensor::Sensor *fy_{nullptr};
  sensor::Sensor *f5_{nullptr};
  sensor::Sensor *fxl_{nullptr};
  sensor::Sensor *f6_{nullptr};
  sensor::Sensor *f7_{nullptr};
  sensor::Sensor *f8_{nullptr};
  sensor::Sensor *nir_{nullptr};
  sensor::Sensor *clear_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  sensor::Sensor *irradiance_{nullptr};
  sensor::Sensor *ppfd_{nullptr};

  sensor::Sensor *saturated_{nullptr};

  uint16_t astep_;
  AS7343Gain gain_;
  uint8_t atime_;

  std::array<uint16_t, AS7343_NUM_CHANNELS> channel_readings_;
  AS7343Gain readings_gain_;
  uint8_t readings_done_;

  std::array<float, AS7343_NUM_CHANNELS> channel_basic_readings_;

  float get_tint_();
  void optimizer_(float max_TINT);
  void direct_config_3_chain_();
  void setup_tint_(float tint);

  bool spectral_post_process_();
  void get_optimized_gain_(uint16_t maximum_adc, uint16_t highest_adc, uint8_t lower_gain_limit,
                           uint8_t upper_gain_limit, uint8_t &out_gain, bool &out_saturation);

  uint16_t get_maximum_spectral_adc_();
  uint16_t get_maximum_spectral_adc_(uint16_t atime, uint16_t astep);
  // uint16_t get_highest_value(std::array<uint16_t, AS7343_NUM_CHANNELS> &data);

  template<typename T, size_t N> T get_highest_value(std::array<T, N> &data);

 public:
  bool as7352_set_integration_time_us(uint32_t time_us);
};

}  // namespace as7343
}  // namespace esphome
