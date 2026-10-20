#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::es9038q2m_katana {

// DSP program numbers of the Katana control interface; there is no program 5.
enum FilterShape : uint8_t {
  FILTER_SHAPE_LINEAR_PHASE_FAST = 0,
  FILTER_SHAPE_LINEAR_PHASE_SLOW = 1,
  FILTER_SHAPE_MIN_PHASE_FAST = 2,
  FILTER_SHAPE_MIN_PHASE_SLOW = 3,
  FILTER_SHAPE_APODIZING = 4,
  FILTER_SHAPE_HYBRID = 6,
  FILTER_SHAPE_BRICK_WALL = 7,
};

enum DeemphasisMode : uint8_t {
  DEEMPHASIS_BYPASS = 0,
  DEEMPHASIS_32KHZ = 1,
  DEEMPHASIS_44_1KHZ = 2,
  DEEMPHASIS_48KHZ = 3,
};

// Boards that expose an ES9038Q2M through the Allo Katana control logic at I2C address 0x30.
// Ported from the Linux driver sound/soc/bcm/allo-katana-codec.c; the DAC's own registers
// are never written, the control logic applies the configuration itself.
class ES9038Q2MKatana : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  // Cubic below 50%, then 0.5 dB per 1% above, meeting at -25 dB.
  // Zero selects full attenuation (-127.5 dB).
  bool set_volume(float volume) override;
  float volume() override;
  bool set_mute_off() override { return this->set_mute_state_(false); }
  bool set_mute_on() override { return this->set_mute_state_(true); }
  bool is_muted() override { return this->is_muted_; }

  void set_filter_shape(FilterShape shape) { this->filter_shape_ = shape; }
  void set_format(uint8_t format) { this->format_ = format; }
  void set_deemphasis_mode(DeemphasisMode mode) { this->deemphasis_mode_ = mode; }
  void set_dop_enabled(bool enabled) { this->dop_enabled_ = enabled; }

 protected:
  bool set_mute_state_(bool mute_state);
  bool write_volume_();
  bool check_write_(bool ok);

  FilterShape filter_shape_{FILTER_SHAPE_APODIZING};
  DeemphasisMode deemphasis_mode_{DEEMPHASIS_BYPASS};
  uint8_t format_{0};
  // Attenuation in 0.5 dB steps: 0 is loudest, 255 quietest. Starts 20 dB down.
  uint8_t volume_reg_{0x28};
  bool dop_enabled_{false};
};

}  // namespace esphome::es9038q2m_katana
