#pragma once

#include <cstdint>

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome::es9038q2m_katana {

// ESPHome port of the control interface exposed by the Allo Katana board family.
// The implementation is based on the Raspberry Pi Linux kernel ASoC driver at:
// sound/soc/bcm/allo-katana-codec.c
//
// The ES9038Q2M datasheet describes the DAC's native register map, but this component
// does not talk to the DAC directly. It talks to the board's control logic at I2C
// address 0x30, which then applies the corresponding configuration to the DAC.

// Filter selections exposed by the Katana control interface.
enum FilterShape : uint8_t {
  FILTER_SHAPE_LINEAR_PHASE_FAST = 0,
  FILTER_SHAPE_LINEAR_PHASE_SLOW = 1,
  FILTER_SHAPE_MIN_PHASE_FAST = 2,
  FILTER_SHAPE_MIN_PHASE_SLOW = 3,
  FILTER_SHAPE_APODIZING = 4,
  FILTER_SHAPE_HYBRID = 5,
  FILTER_SHAPE_BRICK_WALL = 7,
};

// De-emphasis modes exposed by the Katana control interface.
// "Bypass" is the default for normal playback.
enum DeemphasisMode : uint8_t {
  DEEMPHASIS_BYPASS = 0,
  DEEMPHASIS_32KHZ = 1,
  DEEMPHASIS_44_1KHZ = 2,
  DEEMPHASIS_48KHZ = 3,
};

// Audio DAC for boards that expose an ES9038Q2M through a Katana-compatible
// control interface at I2C address 0x30.
// This is not a raw ES9038Q2M driver: it only exposes what that interface provides.
class ES9038Q2MKatana : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Runtime controls exposed through ESPHome's AudioDac interface.
  bool set_volume(float volume) override;
  float volume() override;
  bool set_mute_off() override { return this->set_mute_state_(false); }
  bool set_mute_on() override { return this->set_mute_state_(true); }
  bool is_muted() override { return this->is_muted_; }

  // Static configuration populated from the Python schema.
  void set_filter_shape(FilterShape shape) { this->filter_shape_ = shape; }
  void set_bits_per_sample(uint8_t bits) { this->bits_per_sample_ = bits; }
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_deemphasis_mode(DeemphasisMode deemphasis_mode) { this->deemphasis_mode_ = deemphasis_mode; }
  void set_dop_enabled(bool dop_enabled) { this->dop_enabled_ = dop_enabled; }
  void set_dump_registers(bool dump_registers) { this->dump_registers_ = dump_registers; }

 protected:
  enum class InitPhase : uint8_t {
    IDLE,
    WAIT_RESET_SETTLE,
    DONE,
  };

  bool set_mute_state_(bool mute_state);
  bool write_register_(uint8_t reg, uint8_t value);
  bool read_register_(uint8_t reg, uint8_t *value);
  bool apply_startup_configuration_();
  void dump_registers_live_();
  uint8_t build_format_register_() const;
  uint8_t filter_shape_to_dsp_program_(FilterShape shape) const;
  const char *filter_shape_to_string_(FilterShape shape) const;
  const char *deemphasis_mode_to_string_(DeemphasisMode deemphasis_mode) const;

  // Cached setup values written once the control interface is ready after reset.
  FilterShape filter_shape_{FILTER_SHAPE_APODIZING};
  uint8_t bits_per_sample_{16};
  uint32_t sample_rate_{48000};
  DeemphasisMode deemphasis_mode_{DEEMPHASIS_BYPASS};
  bool dop_enabled_{false};
  bool dump_registers_{false};

  // Cached runtime state used for deferred init and control updates.
  uint8_t volume_reg_{0x28};
  uint8_t chip_id_{0};
  uint8_t format_reg_{0};
  uint32_t init_deadline_ms_{0};
  InitPhase init_phase_{InitPhase::IDLE};
  bool init_complete_{false};
};

}  // namespace esphome::es9038q2m_katana
