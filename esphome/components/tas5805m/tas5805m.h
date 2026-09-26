#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::tas5805m {

enum DacMode : uint8_t {
  DAC_MODE_BTL = 0,   // Bridge tied load, two speakers
  DAC_MODE_PBTL = 1,  // Parallel bridge tied load, one speaker
};

enum MixerMode : uint8_t {
  MIXER_MODE_STEREO = 0,
  MIXER_MODE_STEREO_INVERSE,
  MIXER_MODE_MONO,
  MIXER_MODE_LEFT,
  MIXER_MODE_RIGHT,
};

class TAS5805M : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void update() override;

  /// Clear faults, leave deep sleep and switch to play; the device waits in Hi-Z until an I2S clock is present.
  /// This is also how the output is restarted after a DC or over current fault.
  void activate();
  /// Switch to deep sleep, the lowest power state that keeps I2C and the DSP running.
  void deactivate();

  bool set_mute_off() override { return this->set_mute_(false); }
  bool set_mute_on() override { return this->set_mute_(true); }
  bool set_volume(float volume) override;

  bool is_muted() override { return this->is_muted_; }
  float volume() override { return this->volume_; }

  void set_enable_pin(GPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }
  void set_analog_gain(float analog_gain_db) { this->analog_gain_db_ = analog_gain_db; }
  void set_dac_mode(DacMode dac_mode) { this->dac_mode_ = dac_mode; }
  void set_mixer_mode(MixerMode mixer_mode) { this->mixer_mode_ = mixer_mode; }
  void set_volume_min_db(float volume_min_db) { this->volume_min_db_ = volume_min_db; }
  void set_volume_max_db(float volume_max_db) { this->volume_max_db_ = volume_max_db; }

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(have_fault)
  SUB_BINARY_SENSOR(left_channel_dc_fault)
  SUB_BINARY_SENSOR(right_channel_dc_fault)
  SUB_BINARY_SENSOR(left_channel_over_current)
  SUB_BINARY_SENSOR(right_channel_over_current)
  SUB_BINARY_SENSOR(otp_crc_check)
  SUB_BINARY_SENSOR(bq_write_failed)
  SUB_BINARY_SENSOR(clock_fault)
  SUB_BINARY_SENSOR(pvdd_over_voltage)
  SUB_BINARY_SENSOR(pvdd_under_voltage)
  SUB_BINARY_SENSOR(over_temp_shutdown)
  SUB_BINARY_SENSOR(over_temp_warning)
#endif

 protected:
  bool select_book_page_(uint8_t book, uint8_t page);
  bool init_();
  bool write_ctrl_state_(uint8_t state, bool muted);
  bool set_mute_(bool muted);
  bool write_volume_();
  bool write_mixer_();
  bool read_faults_();

  GPIOPin *enable_pin_{nullptr};
  float volume_{0};
  float analog_gain_db_{-15.5f};
  float volume_min_db_{-103.0f};
  float volume_max_db_{24.0f};
  uint32_t logged_faults_{0};  // Fault bits as last logged, packed like the fault masks
  DacMode dac_mode_{DAC_MODE_BTL};
  MixerMode mixer_mode_{MIXER_MODE_STEREO};
  uint8_t ctrl_state_{0};
  uint8_t power_state_{0xFF};  // Last POWER_STATE seen by update(), 0xFF until the first read
};

}  // namespace esphome::tas5805m
