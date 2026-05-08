#include "es9038q2m_katana.h"

#include <algorithm>
#include <cmath>
#include <cinttypes>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::es9038q2m_katana {

static const char *const TAG = "es9038q2m_katana";

namespace {

constexpr uint8_t KATANA_CHIP_ID = 0x30;

// Register map exposed by the Katana control interface at I2C address 0x30.
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

// Bitfields for KATANA_REG_FORMAT.
constexpr uint8_t FMT_STEREO = 0x80;
constexpr uint8_t FMT_16BIT = 0x10;
constexpr uint8_t FMT_24BIT = 0x20;
constexpr uint8_t FMT_32BIT = 0x30;
constexpr uint8_t FMT_44100 = 0x04;
constexpr uint8_t FMT_48000 = 0x05;
constexpr uint8_t FMT_88200 = 0x06;
constexpr uint8_t FMT_96000 = 0x07;
constexpr uint8_t FMT_176400 = 0x08;
constexpr uint8_t FMT_192000 = 0x09;
constexpr uint8_t FMT_352800 = 0x0A;
constexpr uint8_t FMT_384000 = 0x0B;

// DSP program IDs accepted by the control interface.
// `hybrid` uses program ID 6, so keep that mapping explicit.
constexpr uint8_t DSP_LINEAR_FAST = 0;
constexpr uint8_t DSP_LINEAR_SLOW = 1;
constexpr uint8_t DSP_MIN_FAST = 2;
constexpr uint8_t DSP_MIN_SLOW = 3;
constexpr uint8_t DSP_APODIZING = 4;
constexpr uint8_t DSP_HYBRID = 6;
constexpr uint8_t DSP_BRICK_WALL = 7;

constexpr uint8_t KATANA_DOP_OFF = 0x00;
constexpr uint8_t KATANA_DOP_ON = 0x01;
constexpr uint8_t KATANA_UNMUTED = 0x00;
constexpr uint8_t KATANA_MUTED = 0x01;

// Startup attenuation kept slightly below full scale.
constexpr uint8_t STARTUP_VOLUME_REG = 0x28;

// Let the control interface settle after reset without blocking the main loop.
constexpr uint32_t RESET_SETTLE_DELAY_MS = 10;

const char *const KATANA_REGISTER_NAMES[] = {
    "ChipID", "Reset", "Vol_CH1", "Vol_CH2", "Mute", "DSP_Pgm", "Deemph", "DoP", "Format", "Command", "MuteStr",
};

constexpr uint8_t KATANA_LAST_REGISTER = KATANA_REG_MUTE_STREAM;

}  // namespace

bool ES9038Q2MKatana::write_register_(uint8_t reg, uint8_t value) { return this->write_byte(reg, value); }

bool ES9038Q2MKatana::read_register_(uint8_t reg, uint8_t *value) { return this->read_byte(reg, value); }

uint8_t ES9038Q2MKatana::filter_shape_to_dsp_program_(FilterShape shape) const {
  switch (shape) {
    case FILTER_SHAPE_LINEAR_PHASE_FAST:
      return DSP_LINEAR_FAST;
    case FILTER_SHAPE_LINEAR_PHASE_SLOW:
      return DSP_LINEAR_SLOW;
    case FILTER_SHAPE_MIN_PHASE_FAST:
      return DSP_MIN_FAST;
    case FILTER_SHAPE_MIN_PHASE_SLOW:
      return DSP_MIN_SLOW;
    case FILTER_SHAPE_APODIZING:
      return DSP_APODIZING;
    case FILTER_SHAPE_HYBRID:
      return DSP_HYBRID;
    case FILTER_SHAPE_BRICK_WALL:
      return DSP_BRICK_WALL;
    default:
      return DSP_APODIZING;
  }
}

const char *ES9038Q2MKatana::filter_shape_to_string_(FilterShape shape) const {
  switch (shape) {
    case FILTER_SHAPE_LINEAR_PHASE_FAST:
      return "linear_phase_fast";
    case FILTER_SHAPE_LINEAR_PHASE_SLOW:
      return "linear_phase_slow";
    case FILTER_SHAPE_MIN_PHASE_FAST:
      return "minimum_phase_fast";
    case FILTER_SHAPE_MIN_PHASE_SLOW:
      return "minimum_phase_slow";
    case FILTER_SHAPE_APODIZING:
      return "apodizing";
    case FILTER_SHAPE_HYBRID:
      return "hybrid";
    case FILTER_SHAPE_BRICK_WALL:
      return "brick_wall";
    default:
      return "unknown";
  }
}

const char *ES9038Q2MKatana::deemphasis_mode_to_string_(DeemphasisMode deemphasis_mode) const {
  switch (deemphasis_mode) {
    case DEEMPHASIS_BYPASS:
      return "bypass";
    case DEEMPHASIS_32KHZ:
      return "32khz";
    case DEEMPHASIS_44_1KHZ:
      return "44_1khz";
    case DEEMPHASIS_48KHZ:
      return "48khz";
    default:
      return "unknown";
  }
}

uint8_t ES9038Q2MKatana::build_format_register_() const {
  // The format register packs channel count, word length and sample rate.
  // This component always configures stereo playback.
  uint8_t format = FMT_STEREO;

  switch (this->bits_per_sample_) {
    case 16:
      format |= FMT_16BIT;
      break;
    case 24:
      format |= FMT_24BIT;
      break;
    case 32:
      format |= FMT_32BIT;
      break;
    default:
      format |= FMT_16BIT;
      break;
  }

  switch (this->sample_rate_) {
    case 44100:
      format |= FMT_44100;
      break;
    case 48000:
      format |= FMT_48000;
      break;
    case 88200:
      format |= FMT_88200;
      break;
    case 96000:
      format |= FMT_96000;
      break;
    case 176400:
      format |= FMT_176400;
      break;
    case 192000:
      format |= FMT_192000;
      break;
    case 352800:
      format |= FMT_352800;
      break;
    case 384000:
      format |= FMT_384000;
      break;
    default:
      format |= FMT_48000;
      break;
  }

  return format;
}

bool ES9038Q2MKatana::apply_startup_configuration_() {
  this->format_reg_ = this->build_format_register_();

  // Apply the default playback format used at startup.
  if (!this->write_register_(KATANA_REG_FORMAT, this->format_reg_)) {
    ESP_LOGE(TAG, "Failed to write audio format register");
    return false;
  }

  // Clear any pending command state before applying the rest of the defaults.
  if (!this->write_register_(KATANA_REG_COMMAND, 0x00)) {
    ESP_LOGE(TAG, "Failed to clear command register");
    return false;
  }

  if (!this->write_register_(KATANA_REG_DSP_PROGRAM, this->filter_shape_to_dsp_program_(this->filter_shape_))) {
    ESP_LOGE(TAG, "Failed to write DSP filter program");
    return false;
  }

  if (!this->write_register_(KATANA_REG_DEEMPHASIS, static_cast<uint8_t>(this->deemphasis_mode_))) {
    ESP_LOGE(TAG, "Failed to write de-emphasis mode");
    return false;
  }

  // This only enables DoP handling in the control interface.
  // The upstream source must already be sending valid DoP frames.
  if (!this->write_register_(KATANA_REG_DOP, this->dop_enabled_ ? KATANA_DOP_ON : KATANA_DOP_OFF)) {
    ESP_LOGE(TAG, "Failed to write DoP mode");
    return false;
  }

  // Keep left and right volume in sync with ESPHome's single master control.
  if (!this->write_register_(KATANA_REG_VOLUME_1, this->volume_reg_) ||
      !this->write_register_(KATANA_REG_VOLUME_2, this->volume_reg_)) {
    ESP_LOGE(TAG, "Failed to set initial volume");
    return false;
  }

  // Keep the output mute bit and stream mute flag aligned at startup.
  const uint8_t mute_value = this->is_muted_ ? KATANA_MUTED : KATANA_UNMUTED;
  if (!this->write_register_(KATANA_REG_MUTE, mute_value) ||
      !this->write_register_(KATANA_REG_MUTE_STREAM, mute_value)) {
    ESP_LOGE(TAG, "Failed to apply mute state");
    return false;
  }

  return true;
}

void ES9038Q2MKatana::setup() {
  this->volume_reg_ = STARTUP_VOLUME_REG;
  this->format_reg_ = this->build_format_register_();

  if (!this->read_register_(KATANA_REG_CHIP_ID, &this->chip_id_)) {
    ESP_LOGE(TAG, "Failed to read chip ID");
    this->mark_failed();
    return;
  }
  if (this->chip_id_ != KATANA_CHIP_ID) {
    ESP_LOGE(TAG, "Unexpected chip ID 0x%02X, expected 0x%02X", this->chip_id_, KATANA_CHIP_ID);
    this->mark_failed();
    return;
  }

  // Reset the control interface here, then finish configuration from loop()
  // once the device is ready to accept writes again.
  if (!this->write_register_(KATANA_REG_RESET, 0x01)) {
    ESP_LOGE(TAG, "Failed to reset Katana-compatible control logic");
    this->mark_failed();
    return;
  }

  this->init_deadline_ms_ = millis() + RESET_SETTLE_DELAY_MS;
  this->init_phase_ = InitPhase::WAIT_RESET_SETTLE;
}

void ES9038Q2MKatana::loop() {
  if (this->is_failed() || this->init_phase_ == InitPhase::DONE)
    return;

  if (this->init_phase_ == InitPhase::WAIT_RESET_SETTLE) {
    if (static_cast<int32_t>(millis() - this->init_deadline_ms_) < 0)
      return;

    if (!this->apply_startup_configuration_()) {
      this->mark_failed();
      return;
    }

    this->init_complete_ = true;
    this->init_phase_ = InitPhase::DONE;
  }
}

void ES9038Q2MKatana::dump_registers_live_() {
  ESP_LOGCONFIG(TAG, "  Live register dump:");
  for (uint8_t reg = 0; reg <= KATANA_LAST_REGISTER; reg++) {
    uint8_t value = 0;
    if (this->read_register_(reg, &value)) {
      ESP_LOGCONFIG(TAG, "    [%u] %-8s: 0x%02X", reg, KATANA_REGISTER_NAMES[reg], value);
    } else {
      ESP_LOGCONFIG(TAG, "    [%u] %-8s: <read failed>", reg, KATANA_REGISTER_NAMES[reg]);
    }
  }
}

void ES9038Q2MKatana::dump_config() {
  ESP_LOGCONFIG(TAG, "ES9038Q2M Katana-compatible audio DAC");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Chip ID: 0x%02X", this->chip_id_);
  ESP_LOGCONFIG(TAG, "  Filter shape: %s", this->filter_shape_to_string_(this->filter_shape_));
  ESP_LOGCONFIG(TAG, "  Default sample rate: %" PRIu32 " Hz", this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Default bits per sample: %u", this->bits_per_sample_);
  ESP_LOGCONFIG(TAG, "  De-emphasis: %s", this->deemphasis_mode_to_string_(this->deemphasis_mode_));
  ESP_LOGCONFIG(TAG, "  DoP mode: %s", this->dop_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Startup format register: 0x%02X", this->format_reg_);
  ESP_LOGCONFIG(TAG, "  Register dump: %s", this->dump_registers_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Init complete: %s", YESNO(this->init_complete_));

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with the Katana-compatible control logic failed");
    return;
  }

  if (this->dump_registers_) {
    this->dump_registers_live_();
  }
}

bool ES9038Q2MKatana::set_volume(float volume) {
  volume = std::clamp(volume, 0.0f, 1.0f);

  // Hardware volume is stored as attenuation: 0 is loudest, 255 is quietest.
  const auto register_value = static_cast<uint8_t>(std::lround((1.0f - volume) * 255.0f));

  // Cache early requests until startup configuration has been applied.
  if (!this->init_complete_) {
    this->volume_reg_ = register_value;
    return true;
  }

  if (!this->write_register_(KATANA_REG_VOLUME_1, register_value) ||
      !this->write_register_(KATANA_REG_VOLUME_2, register_value)) {
    ESP_LOGW(TAG, "Failed to update volume");
    return false;
  }

  this->volume_reg_ = register_value;
  return true;
}

float ES9038Q2MKatana::volume() { return 1.0f - (static_cast<float>(this->volume_reg_) / 255.0f); }

bool ES9038Q2MKatana::set_mute_state_(bool mute_state) {
  // As with volume, cache early requests until the device is ready.
  if (!this->init_complete_) {
    this->is_muted_ = mute_state;
    return true;
  }

  if (!this->write_register_(KATANA_REG_MUTE, mute_state ? KATANA_MUTED : KATANA_UNMUTED)) {
    ESP_LOGW(TAG, "Failed to update mute state");
    return false;
  }

  this->is_muted_ = mute_state;
  return true;
}

}  // namespace esphome::es9038q2m_katana
