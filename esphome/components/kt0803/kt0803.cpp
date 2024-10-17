#include "kt0803.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cmath>

namespace esphome {
namespace kt0803 {

#define countof(s) (sizeof(s) / sizeof((s)[0]))

// TODO: std::clamp isn't here yet
#define clamp(v, lo, hi) std::max(std::min(v, hi), lo)

static const char *const TAG = "kt0803";

KT0803Component::KT0803Component() {
  this->chip_id_ = ChipId::KT0803L;
  this->reset_ = false;
  memset(&this->state_, 0, sizeof(this->state_));
  // set datasheet defaults, except frequency to 87.5MHz
  this->state_.REG_00 = 0x6B;
  this->state_.REG_01 = 0xC3;
  this->state_.REG_02 = 0x40;
  this->state_.REG_04 = 0x04;
  this->state_.REG_0E = 0x02;
  this->state_.REG_10 = 0xA8;
  this->state_.REG_12 = 0x80;
  this->state_.REG_13 = 0x80;
  this->state_.REG_15 = 0xE0;
  this->state_.REG_26 = 0xA0;
}

bool KT0803Component::check_reg_(uint8_t addr) {
  switch (addr) {  // check KT0803 address range
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x13:
      return true;
  }

  if (this->chip_id_ != ChipId::KT0803) {
    switch (addr) {  // check KT0803K/M address range too
      case 0x00:
      case 0x01:
      case 0x02:
      case 0x04:
      case 0x0B:
      case 0x0E:
      case 0x0F:
      case 0x10:
      case 0x12:
      case 0x14:
      case 0x16:
        return true;
    }
  }

  if (this->chip_id_ == ChipId::KT0803L) {
    switch (addr) {  // check KT0803L address range too
      case 0x0C:
      case 0x15:
      case 0x17:
      case 0x1E:
      case 0x26:
      case 0x27:
        return true;
    }
  }

  return false;
}

void KT0803Component::write_reg_(uint8_t addr) {
  if (addr >= sizeof(this->regs_) || !this->check_reg_(addr)) {
    ESP_LOGE(TAG, "write_reg_(0x%02X) invalid register address", addr);
    return;
  }

  if (addr == 0x13 && this->state_.PA_CTRL == 1) {
    ESP_LOGW(TAG, "write_reg_(0x%02X) PA_CTRL = 1 can destroy the device", addr);
    return;  // TODO: remove this when everything tested and works
  }

  if (this->reset_) {
    uint8_t value = this->regs_[addr];
    ESP_LOGV(TAG, "write_reg_(0x%02X) = 0x%02X", addr, value);
    this->write_byte(addr, value);
  } else {
    if (this->get_component_state() & COMPONENT_STATE_LOOP) {
      ESP_LOGE(TAG, "write_reg_(0x%02X) device was not reset", addr);
    }
  }
}

bool KT0803Component::read_reg_(uint8_t addr) {
  if (addr >= sizeof(this->regs_) || !this->check_reg_(addr)) {
    ESP_LOGE(TAG, "write_reg_(0x%02X) invalid register address", addr);
    return false;
  }

  uint8_t c;
  if (i2c::ERROR_OK == this->read_register(addr, &c, 1, false)) {
    this->regs_[addr] = c;
    return true;
  }

  ESP_LOGE(TAG, "read_reg_(0x%02X) cannot read register", addr);
  return false;
}

// overrides

void KT0803Component::setup() {
  /*
  for (size_t addr = 0; addr < 0x2F; addr++) {
    uint8_t c;
    if (i2c::ERROR_OK == this->read_register(addr, &c, 1, false)) {
      ESP_LOGV(TAG, "setup register[%02X]: %02X", addr, c);
    }
  }
  */
  this->reset_ = true;

  if (this->chip_id_ != ChipId::KT0803) {
    this->state_.PGAMOD = 1;  // enable 1dB resolution for PGA
  }

  for (size_t addr = 0; addr < sizeof(this->state_); addr++) {
    if (addr != 0x0F && this->check_reg_(addr)) {
      this->write_reg_(addr);
    }
  }

  this->publish_pw_ok();
  this->publish_slncid();
  this->publish_frequency();
  this->publish_pga();
  this->publish_rfgain();
  this->publish_mute();
  this->publish_mono();
  this->publish_pre_emphasis();
  this->publish_pilot_tone_amplitude();
  this->publish_bass_boost_control();
  this->publish_alc_enable();
  this->publish_auto_pa_down();
  this->publish_pa_down();
  this->publish_standby_enable();
  this->publish_alc_attack_time();
  this->publish_alc_decay_time();
  this->publish_pa_bias();
  this->publish_audio_limiter_level();
  this->publish_switch_mode();
  this->publish_silence_high();
  this->publish_silence_low();
  this->publish_silence_detection();
  this->publish_silence_duration();
  this->publish_silence_high_counter();
  this->publish_silence_low_counter();
  this->publish_alc_gain();
  this->publish_xtal_sel();
  this->publish_au_enhance();
  this->publish_frequency_deviation();
  this->publish_ref_clk();
  this->publish_xtal_enable();
  this->publish_ref_clk_enable();
  this->publish_alc_high();
  this->publish_alc_hold_time();
  this->publish_alc_low();
}

void KT0803Component::dump_config() {
  ESP_LOGCONFIG(TAG, "KT0803:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "failed!");
  }
  ESP_LOGCONFIG(TAG, "  Chip: %s", this->get_chip_string().c_str());
  ESP_LOGCONFIG(TAG, "  Frequency: %.2f MHz", this->get_frequency());
  // TODO: ...and everything else...
  LOG_UPDATE_INTERVAL(this);
}

void KT0803Component::update() {
  if (this->read_reg_(0x0F)) {
    this->publish_pw_ok();
    this->publish_slncid();
  }
  /*
    for (size_t addr = 0; addr < 0x2F; addr++) {
      uint8_t c;
      if (i2c::ERROR_OK == this->read_register(addr, &c, 1, false)) {
        ESP_LOGV(TAG, "update register[%02X]: %02X", addr, c);
      }
    }
  */
}

void KT0803Component::loop() {}

// config

void KT0803Component::set_chip_id(ChipId value) { this->chip_id_ = value; }

ChipId KT0803Component::get_chip_id() { return this->chip_id_; }

std::string KT0803Component::get_chip_string() const {
  switch (this->chip_id_) {
    case ChipId::KT0803:
      return "KT0803";
    case ChipId::KT0803K:
      return "KT0803K";
    case ChipId::KT0803M:
      return "KT0803M";
    case ChipId::KT0803L:
      return "KT0803L";
    default:
      return "Unknown";
  }
}

void KT0803Component::set_frequency(float value) {
  if (!(CHSEL_MIN <= value && value <= CHSEL_MAX)) {
    ESP_LOGE(TAG, "set_frequency(%.2f) invalid (%.2f - %.2f)", value, CHSEL_MIN, CHSEL_MAX);
    return;
  }

  uint16_t ch = (uint16_t) std::lround(value * 20);
  this->state_.CHSEL2 = (uint8_t) ((ch >> 9) & 0x07);
  this->state_.CHSEL1 = (uint8_t) ((ch >> 1) & 0xff);
  this->state_.CHSEL0 = (uint8_t) ((ch >> 0) & 0x01);
  this->write_reg_(0x00);
  this->write_reg_(0x01);
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x02);
  }

  this->publish_frequency();
}

float KT0803Component::get_frequency() {
  uint16_t ch = 0;
  ch |= (uint16_t) this->state_.CHSEL2 << 9;
  ch |= (uint16_t) this->state_.CHSEL1 << 1;
  ch |= (uint16_t) this->state_.CHSEL0 << 0;
  return (float) ch / 20;
}

void KT0803Component::set_pga(float value) {
  if (!(PGA_MIN <= value && value <= PGA_MAX)) {
    ESP_LOGE(TAG, "set_pga(%.2f) invalid (%.2f - %.2f)", value, PGA_MIN, PGA_MAX);
    return;
  }

  /*
  12  01100 => 11111 31
  11  01011 => 11110 30
  10  01010 => 11101 29
  9   01001 => 11100 28
  8   01000 => 11011 27
  7   00111 => 11010 26
  6   00110 => 11001 25
  5   00101 => 11000 24
  4   00100 => 10111 23
  3   00011 => 10110 22
  2   00010 => 10101 21
  1   00001 => 10100 20
  0   00000 => 10011 19
  0   00000 => 10010 18
  0   00000 => 10001 17
  0   00000 => 10000 16
  0   00000 => 00000  0
  -1  11111 => 00001  1
  -2  11110 => 00010  2
  -3  11101 => 00011  3
  -4  11100 => 00100  4
  -5  11011 => 00101  5
  -6  11010 => 00110  6
  -7  11001 => 00111  7
  -8  11000 => 01000  8
  -9  10111 => 01001  9
  -10 10110 => 01010 10
  -11 10101 => 01011 11
  -12 10100 => 01100 12
  -13 10011 => 01101 13
  -14 10010 => 01110 14
  -15 10001 => 01111 15
  */

  int pga = (int) std::lround(value);
  if (pga > 0) {
    pga += 19;
  } else if (pga < 0) {
    pga = -pga;
  }
  this->state_.PGA = (uint8_t) (pga >> 2);
  this->state_.PGA_LSB = (uint8_t) (pga & 3);
  this->write_reg_(0x01);
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x04);
  }

  this->publish_pga();
}

float KT0803Component::get_pga() {
  int pga = (int) ((this->state_.PGA << 2) | this->state_.PGA_LSB);
  if (pga >= 20) {
    pga -= 19;
  } else if (0 < pga && pga <= 15) {
    pga = -pga;
  } else
    pga = 0;
  return (float) pga;
}

static const uint16_t RF_GAIN_MAP[] = {
    955, 965, 975, 982, 989, 1000, 1015, 1028, 1051, 1056, 1062, 1065, 1070, 1074, 1077, 1080,
};

void KT0803Component::set_rfgain(float value) {
  if (!(RFGAIN_MIN <= value && value <= RFGAIN_MAX)) {
    ESP_LOGE(TAG, "set_rfgain(%.2f) invalid (%.2f - %.2f)", value, RFGAIN_MIN, RFGAIN_MAX);
    return;
  }

  uint16_t v = (uint16_t) std::lround(value * 10);

  for (size_t i = 0; i < countof(RF_GAIN_MAP); i++) {
    if (v <= RF_GAIN_MAP[i]) {
      this->state_.RFGAIN0 = (i >> 0) & 3;
      this->state_.RFGAIN1 = (i >> 2) & 1;
      this->state_.RFGAIN2 = (i >> 3) & 1;
      break;
    }
  }

  // TODO: there is an annoying hiss during this, mute/unmute does nothing
  // maybe write all three registers in one go

  this->write_reg_(0x00);
  this->write_reg_(0x01);
  this->write_reg_(0x13);

  this->publish_rfgain();
}

float KT0803Component::get_rfgain() {
  uint8_t i = (this->state_.RFGAIN2 << 3) | (this->state_.RFGAIN1 << 2) | this->state_.RFGAIN0;
  return (float) RF_GAIN_MAP[i] / 10;
}

void KT0803Component::set_mute(bool value) {
  this->state_.MUTE = value ? 1 : 0;
  this->write_reg_(0x02);

  this->publish_mute();
}

bool KT0803Component::get_mute() { return this->state_.MUTE == 1; }

void KT0803Component::set_mono(bool value) {
  this->state_.MONO = value ? 1 : 0;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x04);
  }

  this->publish_mono();
}

bool KT0803Component::get_mono() { return this->state_.MONO == 1; }

void KT0803Component::set_pre_emphasis(PreEmphasis value) {
  if (value >= PreEmphasis::LAST) {
    ESP_LOGE(TAG, "set_pre_emphasis(%d) invalid (50 or 75)", (int) value);
    return;
  }

  this->state_.PHTCNST = (uint8_t) value;
  this->write_reg_(0x02);

  this->publish_pre_emphasis();
}

PreEmphasis KT0803Component::get_pre_emphasis() { return (PreEmphasis) this->state_.PHTCNST; }

void KT0803Component::set_pilot_tone_amplitude(PilotToneAmplitude value) {
  if (value >= PilotToneAmplitude::LAST) {
    ESP_LOGE(TAG, "PilotToneAmplitude(%d) invalid (LOW or HIGH)", (int) value);
    return;
  }

  this->state_.PLTADJ = (uint8_t) value;
  this->write_reg_(0x02);

  this->publish_pilot_tone_amplitude();
}

PilotToneAmplitude KT0803Component::get_pilot_tone_amplitude() { return (PilotToneAmplitude) this->state_.PLTADJ; }

void KT0803Component::set_bass_boost_control(BassBoostControl value) {
  if (value >= BassBoostControl::LAST) {
    ESP_LOGE(TAG, "set_bass(%d) invalid", (int) value);
    return;
  }

  this->state_.BASS = (uint8_t) value;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x04);
  }

  this->publish_bass_boost_control();
}

BassBoostControl KT0803Component::get_bass_boost_control() { return (BassBoostControl) this->state_.BASS; }

void KT0803Component::set_alc_enable(bool value) {
  this->state_.ALC_EN = value ? 1 : 0;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x04);
  }

  this->publish_alc_enable();
}

bool KT0803Component::get_alc_enable() { return this->state_.ALC_EN == 1; }

void KT0803Component::set_auto_pa_down(bool value) {
  this->state_.AUTO_PADN = value ? 1 : 0;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x0B);
  }

  this->publish_auto_pa_down();
}

bool KT0803Component::get_auto_pa_down() { return this->state_.AUTO_PADN == 1; }

void KT0803Component::set_pa_down(bool value) {
  this->state_.PDPA = value ? 1 : 0;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x0B);
  }

  this->publish_pa_down();
}

bool KT0803Component::get_pa_down() { return this->state_.PDPA == 1; }

void KT0803Component::set_standby_enable(bool value) {
  this->state_.Standby = value ? 1 : 0;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x0B);
  }

  this->publish_standby_enable();
}

bool KT0803Component::get_standby_enable() { return this->state_.Standby == 1; }

void KT0803Component::set_alc_attack_time(AlcTime value) {
  if (value >= AlcTime::LAST) {
    ESP_LOGE(TAG, "set_alc_attack_time(%d) invalid", (int) value);
    return;
  }

  this->state_.ALC_ATTACK_TIME = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x0C);
  }

  this->publish_alc_attack_time();
}

AlcTime KT0803Component::get_alc_attack_time() { return (AlcTime) this->state_.ALC_ATTACK_TIME; }

void KT0803Component::set_alc_decay_time(AlcTime value) {
  if (value >= AlcTime::LAST) {
    ESP_LOGE(TAG, "set_alc_decay_time(%d) invalid", (int) value);
    return;
  }

  this->state_.ALC_DECAY_TIME = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x0C);
  }

  this->publish_alc_decay_time();
}

AlcTime KT0803Component::get_alc_decay_time() { return (AlcTime) this->state_.ALC_DECAY_TIME; }

void KT0803Component::set_pa_bias(bool value) {
  this->state_.PA_BIAS = value ? 1 : 0;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x0E);
  }

  this->publish_pa_bias();
}

bool KT0803Component::get_pa_bias() { return this->state_.PA_BIAS == 1; }

void KT0803Component::set_audio_limiter_level(AudioLimiterLevel value) {
  if (value >= AudioLimiterLevel::LAST) {
    ESP_LOGE(TAG, "set_audio_limiter_level(%d) invalid", (int) value);
    return;
  }

  this->state_.LMTLVL = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803K || this->chip_id_ == ChipId::KT0803M) {
    this->write_reg_(0x10);
  }

  this->publish_audio_limiter_level();
}

AudioLimiterLevel KT0803Component::get_audio_limiter_level() { return (AudioLimiterLevel) this->state_.LMTLVL; }

void KT0803Component::set_switch_mode(SwitchMode value) {
  if (value >= SwitchMode::LAST) {
    ESP_LOGE(TAG, "set_switch_mode(%d) invalid", (int) value);
    return;
  }

  this->state_.SW_MOD = (uint8_t) value;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x12);
  }

  this->publish_switch_mode();
}

SwitchMode KT0803Component::get_switch_mode() { return (SwitchMode) this->state_.SW_MOD; }

void KT0803Component::set_silence_high(SilenceHigh value) {
  if (value >= SilenceHigh::LAST) {
    ESP_LOGE(TAG, "set_silence_high(%d) invalid", (int) value);
    return;
  }

  this->state_.SLNCTHH = (uint8_t) value;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x12);
  }

  this->publish_silence_high();
}

SilenceHigh KT0803Component::get_silence_high() { return (SilenceHigh) this->state_.SLNCTHH; }

void KT0803Component::set_silence_low(SilenceLow value) {
  if (value >= SilenceLow::LAST) {
    ESP_LOGE(TAG, "set_silence_low(%d) invalid", (int) value);
    return;
  }

  this->state_.SLNCTHL = (uint8_t) value;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x12);
  }

  this->publish_silence_low();
}

SilenceLow KT0803Component::get_silence_low() { return (SilenceLow) this->state_.SLNCTHL; }

void KT0803Component::set_silence_detection(bool value) {
  this->state_.SLNCDIS = value ? 0 : 1;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x12);
  }

  this->publish_silence_detection();
}

bool KT0803Component::get_silence_detection() { return this->state_.SLNCDIS == 0; }

void KT0803Component::set_silence_duration(SilenceLowAndHighLevelDurationTime value) {
  if (value >= SilenceLowAndHighLevelDurationTime::LAST) {
    ESP_LOGE(TAG, "set_silence_duration(%d) invalid", (int) value);
    return;
  }

  this->state_.SLNCTIME0 = (uint8_t) value & 7;
  this->state_.SLNCTIME1 = (uint8_t) value >> 3;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x14);
  }

  this->publish_silence_duration();
}

SilenceLowAndHighLevelDurationTime KT0803Component::get_silence_duration() {
  return (SilenceLowAndHighLevelDurationTime) ((this->state_.SLNCTIME1 << 3) | this->state_.SLNCTIME0);
}

void KT0803Component::set_silence_high_counter(SilenceHighLevelCounter value) {
  if (value >= SilenceHighLevelCounter::LAST) {
    ESP_LOGE(TAG, "set_silence_high_counter(%d) invalid", (int) value);
    return;
  }

  this->state_.SLNCCNTHIGH = (uint8_t) value;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x14);
  }

  this->publish_silence_high_counter();
}

SilenceHighLevelCounter KT0803Component::get_silence_high_counter() {
  return (SilenceHighLevelCounter) this->state_.SLNCCNTHIGH;
}

void KT0803Component::set_silence_low_counter(SilenceLowLevelCounter value) {
  if (value >= SilenceLowLevelCounter::LAST) {
    ESP_LOGE(TAG, "set_silence_low_counter(%d) invalid", (int) value);
    return;
  }

  this->state_.SLNCCNTLOW = (uint8_t) value;
  if (this->chip_id_ != ChipId::KT0803) {
    this->write_reg_(0x16);
  }

  this->publish_silence_low_counter();
}

SilenceLowLevelCounter KT0803Component::get_silence_low_counter() {
  return (SilenceLowLevelCounter) this->state_.SLNCCNTLOW;
}

static const int8_t ALC_GAIN_MAP[] = {-6, -9, -12, -15, 6, 3, 0, -3};

void KT0803Component::set_alc_gain(float value) {
  if (!(ALC_GAIN_MIN <= value && value <= ALC_GAIN_MAX)) {
    ESP_LOGE(TAG, "set_alc_gain(%.2f) invalid (%.2f - %.2f)", value, ALC_GAIN_MIN, ALC_GAIN_MAX);
    return;
  }

  int v = (int) std::lround(value);

  if (v <= -15) {
    this->state_.ALCCMPGAIN = 3;
  } else if (v <= -12) {
    this->state_.ALCCMPGAIN = 2;
  } else if (v <= -9) {
    this->state_.ALCCMPGAIN = 1;
  } else if (v <= -6) {
    this->state_.ALCCMPGAIN = 0;
  } else if (v <= -3) {
    this->state_.ALCCMPGAIN = 7;
  } else if (v <= 0) {
    this->state_.ALCCMPGAIN = 6;
  } else if (v <= 3) {
    this->state_.ALCCMPGAIN = 5;
  } else if (v <= 6) {
    this->state_.ALCCMPGAIN = 4;
  }

  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x15);
  }

  this->publish_alc_gain();
}

float KT0803Component::get_alc_gain() { return (float) ALC_GAIN_MAP[this->state_.ALCCMPGAIN]; }

void KT0803Component::set_xtal_sel(XtalSel value) {
  if (value >= XtalSel::LAST) {
    ESP_LOGE(TAG, "set_xtal_sel(%d) invalid", (int) value);
    return;
  }
  this->state_.XTAL_SEL = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x17);
  }

  this->publish_xtal_sel();
}

XtalSel KT0803Component::get_xtal_sel() { return (XtalSel) this->state_.XTAL_SEL; }

void KT0803Component::set_au_enhance(bool value) {
  this->state_.AU_ENHANCE = value ? 1 : 0;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x17);
  }

  this->publish_au_enhance();
}

bool KT0803Component::get_au_enhance() { return this->state_.AU_ENHANCE == 1; }

void KT0803Component::set_frequency_deviation(FrequencyDeviation value) {
  if (value >= FrequencyDeviation::LAST) {
    ESP_LOGE(TAG, "set_frequency_deviation(%d) invalid", (int) value);
    return;
  }

  if (this->chip_id_ == ChipId::KT0803K || this->chip_id_ == ChipId::KT0803M) {
    this->state_.FDEV_K = (uint8_t) value;
    this->write_reg_(0x04);
  } else if (this->chip_id_ == ChipId::KT0803L) {
    this->state_.FDEV_L = (uint8_t) value;
    this->write_reg_(0x17);
  }

  this->publish_frequency_deviation();
}

FrequencyDeviation KT0803Component::get_frequency_deviation() {
  if (this->chip_id_ == ChipId::KT0803K || this->chip_id_ == ChipId::KT0803M) {
    return (FrequencyDeviation) this->state_.FDEV_K;
  } else if (this->chip_id_ == ChipId::KT0803L) {
    return (FrequencyDeviation) this->state_.FDEV_L;
  }

  return FrequencyDeviation::FDEV_75KHZ;
}

void KT0803Component::set_ref_clk(ReferenceClock value) {
  if (value >= ReferenceClock::LAST) {
    ESP_LOGE(TAG, "set_ref_clk(%d) invalid", (int) value);
    return;
  }

  this->state_.REF_CLK = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x1E);
  }

  this->publish_ref_clk();
}

ReferenceClock KT0803Component::get_ref_clk() { return (ReferenceClock) this->state_.REF_CLK; }

void KT0803Component::set_xtal_enable(bool value) {
  this->state_.XTALD = value ? 0 : 1;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x1E);
  }

  this->publish_xtal_enable();
}

bool KT0803Component::get_xtal_enable() { return this->state_.XTALD == 0; }

void KT0803Component::set_ref_clk_enable(bool value) {
  this->state_.DCLK = value ? 1 : 0;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x1E);
  }

  this->publish_ref_clk_enable();
}

bool KT0803Component::get_ref_clk_enable() { return this->state_.DCLK == 1; }

void KT0803Component::set_alc_high(AlcHigh value) {
  if (value >= AlcHigh::LAST) {
    ESP_LOGE(TAG, "set_alc_high(%d) invalid", (int) value);
    return;
  }

  this->state_.ALCHIGHTH = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x26);
  }

  this->publish_alc_high();
}

AlcHigh KT0803Component::get_alc_high() { return (AlcHigh) this->state_.ALCHIGHTH; }

void KT0803Component::set_alc_hold_time(AlcHoldTime value) {
  if (value >= AlcHoldTime::LAST) {
    ESP_LOGE(TAG, "set_alc_hold(%d) invalid", (int) value);
    return;
  }

  this->state_.ALCHOLD = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x26);
  }

  this->publish_alc_hold_time();
}

AlcHoldTime KT0803Component::get_alc_hold_time() { return (AlcHoldTime) this->state_.ALCHOLD; }

void KT0803Component::set_alc_low(AlcLow value) {
  if (value >= AlcLow::LAST) {
    ESP_LOGE(TAG, "set_alc_low(%d) invalid", (int) value);
    return;
  }

  this->state_.ALCLOWTH = (uint8_t) value;
  if (this->chip_id_ == ChipId::KT0803L) {
    this->write_reg_(0x26);
  }

  this->publish_alc_low();
}

AlcLow KT0803Component::get_alc_low() { return (AlcLow) this->state_.ALCLOWTH; }

// publish

void KT0803Component::publish_pw_ok() { this->publish(this->pw_ok_binary_sensor_, this->state_.PW_OK == 1); }

void KT0803Component::publish_slncid() { this->publish(this->slncid_binary_sensor_, this->state_.SLNCID == 1); }

void KT0803Component::publish_frequency() { this->publish(this->frequency_number_, this->get_frequency()); }

void KT0803Component::publish_pga() { this->publish(this->pga_number_, this->get_pga()); }

void KT0803Component::publish_rfgain() { this->publish(this->rfgain_number_, this->get_rfgain()); }

void KT0803Component::publish_mute() { this->publish(this->mute_switch_, this->get_mute()); }

void KT0803Component::publish_mono() { this->publish(this->mono_switch_, this->get_mono()); }

void KT0803Component::publish_pre_emphasis() {
  this->publish(this->pre_emphasis_select_, (size_t) this->get_pre_emphasis());
}

void KT0803Component::publish_pilot_tone_amplitude() {
  this->publish(this->pilot_tone_amplitude_select_, (size_t) this->get_pilot_tone_amplitude());
}

void KT0803Component::publish_bass_boost_control() {
  this->publish(this->bass_boost_control_select_, (size_t) this->get_bass_boost_control());
}

void KT0803Component::publish_alc_enable() { this->publish(this->alc_enable_switch_, this->get_alc_enable()); }

void KT0803Component::publish_auto_pa_down() { this->publish(this->auto_pa_down_switch_, this->get_auto_pa_down()); }

void KT0803Component::publish_pa_down() { this->publish(this->pa_down_switch_, this->get_pa_down()); }

void KT0803Component::publish_alc_attack_time() {
  this->publish(this->alc_attack_time_select_, (size_t) this->get_alc_attack_time());
}

void KT0803Component::publish_alc_decay_time() {
  this->publish(this->alc_decay_time_select_, (size_t) this->get_alc_decay_time());
}

void KT0803Component::publish_standby_enable() {
  this->publish(this->standby_enable_switch_, this->get_standby_enable());
}

void KT0803Component::publish_pa_bias() { this->publish(this->pa_bias_switch_, this->get_pa_bias()); }

void KT0803Component::publish_audio_limiter_level() {
  this->publish(this->audio_limiter_level_select_, (size_t) this->get_audio_limiter_level());
}

void KT0803Component::publish_switch_mode() {
  this->publish(this->switch_mode_select_, (size_t) this->get_switch_mode());
}

void KT0803Component::publish_silence_high() {
  this->publish(this->silence_high_select_, (size_t) this->get_silence_high());
}

void KT0803Component::publish_silence_low() {
  this->publish(this->silence_low_select_, (size_t) this->get_silence_low());
}

void KT0803Component::publish_silence_detection() {
  this->publish(this->silence_detection_switch_, this->get_silence_detection());
}

void KT0803Component::publish_silence_duration() {
  this->publish(this->silence_duration_select_, (size_t) this->get_silence_duration());
}

void KT0803Component::publish_silence_high_counter() {
  this->publish(this->silence_high_counter_select_, (size_t) this->get_silence_high_counter());
}

void KT0803Component::publish_silence_low_counter() {
  this->publish(this->silence_low_counter_select_, (size_t) this->get_silence_low_counter());
}

void KT0803Component::publish_alc_gain() { this->publish(this->alc_gain_number_, this->get_alc_gain()); }

void KT0803Component::publish_xtal_sel() { this->publish(this->xtal_sel_select_, (size_t) this->get_xtal_sel()); }

void KT0803Component::publish_au_enhance() { this->publish(this->au_enhance_switch_, this->get_au_enhance()); }

void KT0803Component::publish_frequency_deviation() {
  this->publish(this->frequency_deviation_select_, (size_t) this->get_frequency_deviation());
}

void KT0803Component::publish_ref_clk() { this->publish(this->ref_clk_select_, (size_t) this->get_ref_clk()); }

void KT0803Component::publish_xtal_enable() {
  this->publish(this->xtal_enable_switch_, (size_t) this->get_xtal_enable());
}

void KT0803Component::publish_ref_clk_enable() {
  this->publish(this->ref_clk_enable_switch_, (size_t) this->get_ref_clk_enable());
}

void KT0803Component::publish_alc_high() { this->publish(this->alc_high_select_, (size_t) this->get_alc_high()); }

void KT0803Component::publish_alc_hold_time() {
  this->publish(this->alc_hold_time_select_, (size_t) this->get_alc_hold_time());
}

void KT0803Component::publish_alc_low() { this->publish(this->alc_low_select_, (size_t) this->get_alc_low()); }

void KT0803Component::publish(text_sensor::TextSensor *s, const std::string &state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void KT0803Component::publish(binary_sensor::BinarySensor *s, bool state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void KT0803Component::publish(sensor::Sensor *s, float state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void KT0803Component::publish(number::Number *n, float state) {
  if (n != nullptr) {
    if (!n->has_state() || n->state != state) {
      n->publish_state(state);
    }
  }
}

void KT0803Component::publish(switch_::Switch *s, bool state) {
  if (s != nullptr) {
    if (s->state != state) {  // ?
      s->publish_state(state);
    }
  }
}

void KT0803Component::publish(select::Select *s, size_t index) {
  if (s != nullptr) {
    if (auto state = s->at(index)) {
      if (!s->has_state() || s->state != *state) {
        s->publish_state(*state);
      }
    }
  }
}

void KT0803Component::publish(text::Text *t, const std::string &state) {
  if (t != nullptr) {
    if (!t->has_state() || t->state != state) {
      t->publish_state(state);
    }
  }
}

}  // namespace kt0803
}  // namespace esphome
