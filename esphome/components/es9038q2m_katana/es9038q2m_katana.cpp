#include "es9038q2m_katana.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <cmath>

namespace esphome::es9038q2m_katana {

ESPHOME_LOG_TAG(TAG, "es9038q2m_katana");

namespace {

constexpr uint8_t KATANA_CHIP_ID = 0x30;

constexpr uint8_t KATANA_REG_CHIP_ID = 0x00;
constexpr uint8_t KATANA_REG_RESET = 0x01;
constexpr uint8_t KATANA_REG_VOLUME_1 = 0x02;
constexpr uint8_t KATANA_REG_VOLUME_2 = 0x03;
constexpr uint8_t KATANA_REG_MUTE = 0x04;
constexpr uint8_t KATANA_REG_DSP_PROGRAM = 0x05;
constexpr uint8_t KATANA_REG_DEEMPHASIS = 0x06;
constexpr uint8_t KATANA_REG_DOP = 0x07;
constexpr uint8_t KATANA_REG_FORMAT = 0x08;
constexpr uint8_t KATANA_REG_COMMAND = 0x09;
constexpr uint8_t KATANA_REG_MUTE_STREAM = 0x0A;

constexpr float KATANA_VOLUME_MIDPOINT = 0.5f;
constexpr float KATANA_VOLUME_MIDPOINT_STEPS = 50.0f;  // -25 dB at 50% volume.
// Cubic amplitude gain: -20 * log10(volume^3) / 0.5 attenuation steps.
constexpr float KATANA_VOLUME_CUBIC_SCALE = 120.0f;
constexpr uint8_t KATANA_VOLUME_FULL_ATTENUATION = 255;

// The control logic ignores writes for this long after a reset.
constexpr uint32_t RESET_SETTLE_DELAY_MS = 10;

// Indexed by FilterShape
PROGMEM_STRING_TABLE(FilterShapeStrings, "linear_phase_fast", "linear_phase_slow", "minimum_phase_fast",
                     "minimum_phase_slow", "apodizing", "brick_wall", "hybrid", "brick_wall");

const LogString *filter_shape_to_str(FilterShape shape) {
  return FilterShapeStrings::get_log_str(static_cast<uint8_t>(shape), FilterShapeStrings::LAST_INDEX);
}

// Indexed by DeemphasisMode
PROGMEM_STRING_TABLE(DeemphasisStrings, "bypass", "32khz", "44_1khz", "48khz");

const LogString *deemphasis_to_str(DeemphasisMode mode) {
  return DeemphasisStrings::get_log_str(mode, DEEMPHASIS_BYPASS);
}

}  // namespace

void ES9038Q2MKatana::setup() {
  if (!this->check_chip_id(this, KATANA_REG_CHIP_ID, KATANA_CHIP_ID))
    return;
  if (!this->setup_check(this->write_byte(KATANA_REG_RESET, 0x01)))
    return;
  delay(RESET_SETTLE_DELAY_MS);

  // MUTE_STREAM is the gate the Linux driver closes between streams; ESPHome has no such
  // notion, so it stays open and mute is only ever driven through MUTE.
  if (!this->write_byte(KATANA_REG_FORMAT, this->format_) || !this->write_byte(KATANA_REG_COMMAND, 0x00) ||
      !this->write_byte(KATANA_REG_DSP_PROGRAM, this->filter_shape_) ||
      !this->write_byte(KATANA_REG_DEEMPHASIS, this->deemphasis_mode_) ||
      !this->write_byte(KATANA_REG_DOP, this->dop_enabled_ ? 1 : 0) || !this->write_volume_() ||
      !this->write_byte(KATANA_REG_MUTE, this->is_muted_ ? 1 : 0) || !this->write_byte(KATANA_REG_MUTE_STREAM, 0x00)) {
    this->mark_failed(LOG_STR("Failed to configure the control logic"));
  }
}

void ES9038Q2MKatana::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ES9038Q2M Katana:\n"
                "  Filter shape: %s\n"
                "  De-emphasis: %s\n"
                "  Format register: 0x%02X\n"
                "  DoP: %s",
                LOG_STR_ARG(filter_shape_to_str(this->filter_shape_)),
                LOG_STR_ARG(deemphasis_to_str(this->deemphasis_mode_)), this->format_, ONOFF(this->dop_enabled_));
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    return;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  for (uint8_t reg = KATANA_REG_CHIP_ID; reg <= KATANA_REG_MUTE_STREAM; reg++) {
    uint8_t value = 0;
    if (this->read_byte(reg, &value)) {
      ESP_LOGV(TAG, "  Register 0x%02X: 0x%02X", reg, value);
    } else {
      ESP_LOGV(TAG, "  Register 0x%02X: read failed", reg);
    }
  }
#endif
}

// Both channel registers follow ESPHome's single master volume.
bool ES9038Q2MKatana::write_volume_() {
  return this->write_byte(KATANA_REG_VOLUME_1, this->volume_reg_) &&
         this->write_byte(KATANA_REG_VOLUME_2, this->volume_reg_);
}

// Records the outcome of a runtime write; the cached state only changes on success.
bool ES9038Q2MKatana::check_write_(bool ok) { return this->status_comm_result(ok); }

bool ES9038Q2MKatana::set_volume(float volume) {
  if (this->is_failed())
    return false;
  volume = clamp(volume, 0.0f, 1.0f);
  const uint8_t previous = this->volume_reg_;
  float attenuation = KATANA_VOLUME_FULL_ATTENUATION;
  if (volume >= KATANA_VOLUME_MIDPOINT) {
    attenuation = KATANA_VOLUME_MIDPOINT_STEPS * (1.0f - volume) / (1.0f - KATANA_VOLUME_MIDPOINT);
  } else if (volume > 0.0f) {
    attenuation = KATANA_VOLUME_MIDPOINT_STEPS - KATANA_VOLUME_CUBIC_SCALE * log10f(volume / KATANA_VOLUME_MIDPOINT);
  }
  this->volume_reg_ =
      static_cast<uint8_t>(lroundf(clamp(attenuation, 0.0f, static_cast<float>(KATANA_VOLUME_FULL_ATTENUATION))));
  if (this->check_write_(this->write_volume_()))
    return true;
  this->volume_reg_ = previous;
  return false;
}

float ES9038Q2MKatana::volume() {
  if (this->volume_reg_ == KATANA_VOLUME_FULL_ATTENUATION)
    return 0.0f;
  if (this->volume_reg_ <= KATANA_VOLUME_MIDPOINT_STEPS)
    return 1.0f - this->volume_reg_ * (1.0f - KATANA_VOLUME_MIDPOINT) / KATANA_VOLUME_MIDPOINT_STEPS;
  return KATANA_VOLUME_MIDPOINT *
         powf(10.0f, (KATANA_VOLUME_MIDPOINT_STEPS - this->volume_reg_) / KATANA_VOLUME_CUBIC_SCALE);
}

bool ES9038Q2MKatana::set_mute_state_(bool mute_state) {
  if (this->is_failed())
    return false;
  if (!this->check_write_(this->write_byte(KATANA_REG_MUTE, mute_state ? 1 : 0)))
    return false;
  this->is_muted_ = mute_state;
  return true;
}

}  // namespace esphome::es9038q2m_katana
