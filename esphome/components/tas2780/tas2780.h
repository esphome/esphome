#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {

namespace sensor {
class Sensor;
}

namespace tas2780 {

enum ChannelSelect : uint8_t { MONO_DWN_MIX, LEFT_CHANNEL, RIGHT_CHANNEL };

class TAS2780 : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void reset();
  void activate(uint8_t power_mode = 2);
  void deactivate();
  void update_register();

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

  void set_amp_level(uint8_t amp_level) { this->amp_level_ = amp_level; }
  void set_power_mode(uint8_t power_mode) { this->power_mode_ = power_mode; }
  void set_vol_range_min(float min_val) { this->vol_range_min_ = min_val; }
  void set_vol_range_max(float max_val) { this->vol_range_max_ = max_val; }
  void set_selected_channel(uint8_t channel) { this->selected_channel_ = static_cast<ChannelSelect>(channel); }

#ifdef USE_SENSOR
  void set_pvdd_sensor(sensor::Sensor *sensor) { this->pvdd_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }

  float get_pvdd_voltage();
  float get_temperature();
#endif

 protected:
  void init();
  void set_power_mode_(const uint8_t power_mode);
  bool write_mute_();
  bool write_volume_();
  void log_error_states_();

#ifdef USE_SENSOR
  uint16_t read_sar_adc_(uint8_t msb_reg, uint8_t lsb_reg);
  void update_sensors_();
#endif

  float volume_{0};
  uint8_t power_mode_{2};
  uint8_t amp_level_{8};
  float vol_range_min_{0.3f};
  float vol_range_max_{1.0f};
  ChannelSelect selected_channel_{MONO_DWN_MIX};

#ifdef USE_SENSOR
  sensor::Sensor *pvdd_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
#endif
};

}  // namespace tas2780
}  // namespace esphome