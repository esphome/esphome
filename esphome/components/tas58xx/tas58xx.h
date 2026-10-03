#pragma once

#include <array>

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::tas58xx {

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

/// Fault binary sensors that map to a single fault bit. Note that ordering is important.
/// The Python FAULT_SENSORS list in binary_sensor.py uses the same names and ordering.
enum FaultSensor : uint8_t {
  // Faults common to TAS5805M and TAS5825M
  FAULT_SENSOR_LEFT_CHANNEL_DC_FAULT,
  FAULT_SENSOR_RIGHT_CHANNEL_DC_FAULT,
  FAULT_SENSOR_LEFT_CHANNEL_OVER_CURRENT,
  FAULT_SENSOR_RIGHT_CHANNEL_OVER_CURRENT,
  FAULT_SENSOR_OTP_CRC_CHECK,
  FAULT_SENSOR_BQ_WRITE_FAILED,
  FAULT_SENSOR_CLOCK_FAULT,
  FAULT_SENSOR_PVDD_OVER_VOLTAGE,
  FAULT_SENSOR_PVDD_UNDER_VOLTAGE,
  FAULT_SENSOR_OVER_TEMP_SHUTDOWN,
  FAULT_SENSOR_OVER_TEMP_WARNING,
  // Faults available for TAS5825M Only
  FAULT_SENSOR_LOAD_EEPROM_ERROR,
  FAULT_SENSOR_RIGHT_CHANNEL_CBC_OVER_CURRENT,
  FAULT_SENSOR_LEFT_CHANNEL_CBC_OVER_CURRENT,
  FAULT_SENSOR_LEFT_CHANNEL_CBC_OVER_CURRENT_WARNING,
  FAULT_SENSOR_RIGHT_CHANNEL_CBC_OVER_CURRENT_WARNING,
  FAULT_SENSOR_OVER_TEMP_146C_WARNING,
  // FAULT_SENSOR_OVER_TEMP_122C_WARNING,
  // FAULT_SENSOR_OVER_TEMP_112C_WARNING,
  FAULT_SENSOR_COUNT,  // keep last
};

// compile time check
static_assert(
    FAULT_SENSOR_COUNT == 17,
    "enum FaultSensor altered: Update FAULT_SENSORS in binary_sensor.py and fault_sensor_bits in each model's cpp");

/// ModelInfo::fault_sensor_bits value for a FaultSensor that the model does not have.
static constexpr uint8_t NO_BIT = 0xFF;

/// Everything that differs between models of the family. One constant instance exists per model, see
/// model_*.cpp, and each TAS58xx instance points to the one for its model.
///
/// Fault bits are packed into a 32 bit word: CHAN_FAULT, GLOBAL_FAULT1, GLOBAL_FAULT2 and OT_WARNING, one byte
/// each, from low to high. A bit index is register * 8 + bit.
struct ModelInfo {
  const LogString *(*name)();
  /// Remainder of the startup sequence, run after the reset, as {register, value} pairs in PROGMEM
  const uint8_t (*startup_sequence)[2];
  uint8_t startup_sequence_length;
  /// Location of the four 9.23 fixed point input mixer coefficients: LEFT_TO_LEFT, RIGHT_TO_LEFT, LEFT_TO_RIGHT,
  /// RIGHT_TO_RIGHT
  uint8_t mixer_book;
  uint8_t mixer_page;
  uint8_t mixer_register;
  /// Faults logged as errors and reported by have_fault
  uint32_t fault_error_mask;
  /// Faults logged as warnings
  uint32_t fault_warning_mask;
  /// Faults that keep the output off until cleared by activate()
  uint32_t fault_output_off_mask;
  /// Faults that stay set after the condition is gone and are cleared after each read
  uint32_t fault_latched_mask;
  const LogString *(*fault_name)(uint8_t index);
  /// Bit index for each FaultSensor, NO_BIT used where the model does not have that fault bit
  uint8_t fault_sensor_bits[FAULT_SENSOR_COUNT];
};

#ifdef USE_TAS58XX_TAS5805M
extern const ModelInfo TAS5805M_MODEL;
#endif
#ifdef USE_TAS58XX_TAS5825M
extern const ModelInfo TAS5825M_MODEL;
#endif

class TAS58xx : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice {
 public:
  explicit TAS58xx(const ModelInfo *model) : model_(model) {}

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

  void on_audio_started() override;

  void set_enable_pin(GPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }
  void set_analog_gain(float analog_gain_db) { this->analog_gain_db_ = analog_gain_db; }
  void set_dac_mode(DacMode dac_mode) { this->dac_mode_ = dac_mode; }
  void set_mixer_mode(MixerMode mixer_mode) { this->mixer_mode_ = mixer_mode; }
  void set_volume_min_db(float volume_min_db) { this->volume_min_db_ = volume_min_db; }
  void set_volume_max_db(float volume_max_db) { this->volume_max_db_ = volume_max_db; }

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(have_fault)
  void set_fault_binary_sensor(FaultSensor fault, binary_sensor::BinarySensor *sensor) {
    this->fault_binary_sensors_[fault] = sensor;
  }
#endif

 protected:
  bool select_book_page_(uint8_t book, uint8_t page);
  bool init_();
  bool write_ctrl_state_(uint8_t state, bool muted);
  bool set_mute_(bool muted);
  bool write_volume_();
  bool write_mixer_();
  bool read_faults_();

  const ModelInfo *model_;
  GPIOPin *enable_pin_{nullptr};
#ifdef USE_BINARY_SENSOR
  std::array<binary_sensor::BinarySensor *, FAULT_SENSOR_COUNT> fault_binary_sensors_{};
#endif
  float volume_{0};
  float analog_gain_db_{-15.5f};
  float volume_min_db_{-103.0f};
  float volume_max_db_{24.0f};
  uint32_t logged_faults_{0};  // Fault bits as last logged, packed like the fault masks
  DacMode dac_mode_{DAC_MODE_BTL};
  MixerMode mixer_mode_{MIXER_MODE_STEREO};
  uint8_t ctrl_state_{0};
  uint8_t power_state_{0xFF};  // Last POWER_STATE seen by update(), 0xFF until the first read
  bool mixer_written_{false};  // Mixer written since the last reset
};

}  // namespace esphome::tas58xx
