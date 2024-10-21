#include "si4713.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cmath>

namespace esphome {
namespace si4713_i2c {

// TODO: std::clamp isn't here yet
#define clamp(v, lo, hi) std::max(std::min(v, hi), lo)

static const char *const TAG = "si4713";

Si4713Component::Si4713Component() {
  this->reset_pin_ = nullptr;
  this->reset_ = false;
  this->op_mode_ = OpMode::OPMODE_ANALOG;
  this->power_enable_ = true;
  this->frequency_ = 8750;
  this->power_ = 115;
  this->antcap_ = 0;
  this->tx_acomp_preset_ = AcompPreset::ACOMP_CUSTOM;
  memset(this->gpio_, 0, sizeof(gpio_));
}

bool Si4713Component::send_cmd_(const CmdBase *cmd, size_t cmd_size, ResBase *res, size_t res_size) {
  const uint8_t *buff = (const uint8_t *) cmd;

  if (!this->reset_) {
    if (this->get_component_state() & COMPONENT_STATE_LOOP) {
      ESP_LOGE(TAG, "%s(0x%02X, %d) device was not reset", __func__, cmd->CMD, cmd_size);
    }
    return false;
  }

  if (cmd->CMD != CmdType::GET_INT_STATUS && cmd->CMD != CmdType::TX_ASQ_STATUS) {
    ESP_LOGV(TAG, "Cmd %02X %d", cmd->CMD, cmd_size);
  }

  if (cmd->CMD == CmdType::SET_PROPERTY) {
    const CmdSetProperty *c = (const CmdSetProperty *) cmd;
    ESP_LOGV(TAG, "Set %02X%02X %02X%02X", c->PROPH, c->PROPL, c->PROPDH, c->PROPDL);
  }

  i2c::ErrorCode err = this->write(buff, cmd_size);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "%s(0x%02X, %d) write error", __func__, cmd->CMD, cmd_size);
    // this->mark_failed();
    return false;
  }

  ResBase status;
  while (status.CTS == 0) {
    err = this->read((uint8_t *) &status, 1);  // TODO: read res_size into res here?
    if (err != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "%s(0x%02X, %d) read status error", __func__, cmd->CMD, cmd_size);
      // this->mark_failed();
      return false;
    }
  }

  if (res != nullptr) {
    //((uint8_t*) res)[0] = status;
    err = this->read((uint8_t *) res, res_size);
    if (err != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "%s(0x%02X, %d) read response error", __func__, cmd->CMD, cmd_size);
      // this->mark_failed();
      return false;
    }

    if (cmd->CMD == CmdType::GET_PROPERTY) {
      const CmdGetProperty *c = (const CmdGetProperty *) cmd;
      ResGetProperty *r = (ResGetProperty *) res;
      ESP_LOGV(TAG, "Get %02X%02X %02X%02X", c->PROPH, c->PROPL, r->PROPDH, r->PROPDL);
    }
  }

  return true;
}

void Si4713Component::rds_update_() {}

bool Si4713Component::device_reset_() {
  if (this->reset_pin_ == nullptr) {
    ESP_LOGE(TAG, "cannot reset device, reset pin is not set");
    return false;
  }

  if (!this->reset_) {
    this->reset_pin_->setup();
  }

  this->reset_pin_->digital_write(true);
  delay_microseconds_safe(10);
  this->reset_pin_->digital_write(false);
  delay_microseconds_safe(10);
  this->reset_pin_->digital_write(true);

  this->reset_ = true;

  return true;
}

bool Si4713Component::power_up_() {
  // NOTE: In FMTX component 1.0 and 2.0, a reset is required when the system
  // controller writes a command other than POWER_UP when in powerdown mode
  this->device_reset_();

  CmdPowerUp cmd;
  cmd.OPMODE = (uint8_t) this->op_mode_;
  cmd.FUNC = 2;                                                  // transmit
  cmd.XOSCEN = this->op_mode_ == OpMode::OPMODE_ANALOG ? 1 : 0;  // auto-enable(?) xtal for analog mode
  cmd.GPO2OEN = 0;                                               // we do this later
  cmd.CTSIEN = 0;                                                // no interrupts
  return this->send_cmd_(cmd);
}

bool Si4713Component::power_down_() { return this->send_cmd_(CmdPowerDown()); }

bool Si4713Component::detect_chip_id_() {
  ResGetRev res;
  if (!this->send_cmd_(CmdGetRev(), res)) {
    return false;
  }

  char buff[32] = {0};
  snprintf(buff, sizeof(buff), "Si47%02d Rev %d", res.PN, res.CHIPREV);
  this->chip_id_ = buff;

  // TODO: support all transmitters 10/11/12/13/20/21, mask unsupported features

  if (res.PN != 10 && res.PN != 11 && res.PN != 12 && res.PN != 13) {
    ESP_LOGE(TAG, "Si47%02d is not supported", res.PN);
    return false;
  }

  return true;
}

bool Si4713Component::tune_freq_(uint16_t freq) { return this->send_cmd_(CmdTxTuneFreq(freq)) && this->stc_wait_(); }

bool Si4713Component::tune_power_(uint8_t power, uint8_t antcap) {
  return this->send_cmd_(CmdTxTunePower(this->power_enable_ ? power : 0, antcap)) && this->stc_wait_();
}

bool Si4713Component::stc_wait_() {
  ResGetIntStatus res;
  while (res.STCINT != 1) {  // risky loop of the day, it will be fine
    if (!this->send_cmd_(CmdGetIntStatus(), res)) {
      return false;
    }
  }

  return this->send_cmd_(CmdTxTuneStatus(0), this->tune_status_);
}

// overrides

void Si4713Component::setup() {
  // CHIP STATE: POWER DOWN

  if (!this->power_up_()) {
    this->mark_failed();
    return;
  }

  if (!this->detect_chip_id_()) {
    this->mark_failed();
    return;
  }

  // CHIP STATE: POWER UP

  // Set RCLK settings
  this->set_prop_(this->refclk_freq_);
  this->set_prop_(this->refclk_prescale_);
  // Mono/Stereo?
  this->set_prop_(this->tx_pilot_frequency_);
  this->set_prop_(this->tx_pilot_deviation_);
  this->set_prop_(this->tx_component_enable_);
  // Set Audio Deviation
  this->set_prop_(this->tx_audio_deviation_);
  // Transmit RDS?
  this->set_prop_(this->tx_rds_deviation_);
  // Preemphasis?
  this->set_prop_(this->tx_pre_emphasis_);
  this->set_prop_(this->tx_acomp_enable_);
  // Compressor?
  // if (tx_acomp_enable_.ACEN) {
  this->set_prop_(this->tx_acomp_threshold_);
  this->set_prop_(this->tx_acomp_attack_time_);
  this->set_prop_(this->tx_acomp_release_time_);
  this->set_prop_(this->tx_acomp_gain_);
  //}
  // Limiter?
  // if (tx_acomp_enable_.LIMITEN) {
  this->set_prop_(tx_limiter_releasee_time_);
  //}
  // Tune
  this->tune_freq_(this->frequency_);
  this->tune_power_(this->power_, this->antcap_);

  // CHIP STATE: TRANSMITTING

  // if (this->op_mode_ == OpMode::OPMODE_DIGITAL) {
  //  Digital Audio Input?
  this->set_prop_(digital_input_format_);
  this->set_prop_(digital_input_sample_rate_);
  //} else if (this->op_mode_ == OpMode::OPMODE_ANALOG) {
  // Analog Audio Input?
  this->set_prop_(tx_line_input_level_);
  //}
  // Mute? Optional: Mute or Unmute Audio based on ASQ status
  this->set_prop_(this->tx_line_input_mute_);
  // GPIO
  this->send_cmd_(CmdGpioCtl(1, 1, 0));  // enable gpio1 and gpio2, clock is on gpio3
  this->send_cmd_(CmdGpioSet(0, 0, 0));  // init gpio to low (TODO: config)
  // AQS
  this->set_prop_(this->tx_asq_interrupt_source_);
  this->set_prop_(this->tx_asq_level_low_);
  this->set_prop_(this->tx_asq_duration_low_);
  this->set_prop_(this->tx_asq_level_high_);
  this->set_prop_(this->tx_asq_duration_high_);
  // RDS
  // TODO: PropTxRds*, CmdTxRdsPs, CmdTxRdsBuff

  // publish

  this->publish_mute();
  this->publish_mono();
  this->publish_pre_emphasis();
  this->publish_tuner_enable();
  this->publish_tuner_frequency();
  this->publish_tuner_deviation();
  this->publish_tuner_power();
  this->publish_tuner_antcap();
  this->publish_analog_level();
  this->publish_analog_attenuation();
  this->publish_digital_sample_rate();
  this->publish_digital_sample_bits();
  this->publish_digital_channels();
  this->publish_digital_mode();
  this->publish_digital_clock_edge();
  this->publish_pilot_enable();
  this->publish_pilot_frequency();
  this->publish_pilot_deviation();
  this->publish_refclk_frequency();
  this->publish_refclk_source();
  this->publish_refclk_prescaler();
  this->publish_acomp_enable();
  this->publish_acomp_threshold();
  this->publish_acomp_attack();
  this->publish_acomp_release();
  this->publish_acomp_gain();
  this->publish_acomp_preset();
  this->publish_limiter_enable();
  this->publish_limiter_release_time();
  this->publish_asq_iall();
  this->publish_asq_ialh();
  this->publish_asq_overmod();
  this->publish_asq_level_low();
  this->publish_asq_duration_low();
  this->publish_asq_level_high();
  this->publish_asq_duration_high();
  this->publish_rds_enable();
  this->publish_rds_deviation();
  this->publish_rds_station();
  this->publish_rds_text();
  this->publish_output_gpio1();
  this->publish_output_gpio2();
  this->publish_output_gpio3();
  this->publish_chip_id_text_sensor();
  this->publish_frequency_sensor();
  this->publish_power_sensor();
  this->publish_antcap_sensor();
  this->publish_noise_level_sensor();
  this->publish_iall_binary_sensor();
  this->publish_ialh_binary_sensor();
  this->publish_overmod_binary_sensor();
  this->publish_inlevel_sensor();

  this->set_interval(1000, [this]() { this->rds_update_(); });
}

void Si4713Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Si4713:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "failed!");
  }
  ESP_LOGCONFIG(TAG, "  Chip: %s", this->chip_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Frequency: %.2f MHz", this->get_tuner_frequency());
  ESP_LOGCONFIG(TAG, "  RDS station: %s", this->rds_station_.c_str());
  ESP_LOGCONFIG(TAG, "  RDS text: %s", this->rds_text_.c_str());
  // TODO: ...and everything else...
  LOG_UPDATE_INTERVAL(this);
}

void Si4713Component::update() {
  // these might be changing too fast for loop()
  this->publish_noise_level_sensor();
  this->publish_inlevel_sensor();
}

void Si4713Component::loop() {
  ResGetIntStatus res;
  if (this->send_cmd_(CmdGetIntStatus(), res)) {
    if (res.STCINT == 1) {
      ESP_LOGV(TAG, "STCINT");
      if (this->send_cmd_(CmdTxTuneStatus(1), this->tune_status_)) {
        float f = (float) ((this->tune_status_.READFREQH << 8) | this->tune_status_.READFREQL) / 100;
        ESP_LOGD(TAG, "ResTxTuneStatus FREQ %.2f RFdBuV %d ANTCAP %d NL %d", f, this->tune_status_.READRFdBuV,
                 this->tune_status_.READANTCAP, this->tune_status_.RNL);
        this->publish_frequency_sensor();
        this->publish_power_sensor();
        this->publish_antcap_sensor();
        // this->publish_noise_level_sensor();
      }
    }
    if (res.ASQINT == 1) {
      if (this->send_cmd_(CmdTxAsqStatus(1), this->asq_status_)) {
        // ESP_LOGD(TAG, "ResTxAsqStatus IALL %d IALH %d OVERMOD %d INLEVEL %d",
        //          this->asq_status_.IALL, this->asq_status_.IALH, this->asq_status_.OVERMOD,
        //          this->asq_status_.INLEVEL);
        this->publish_iall_binary_sensor();
        this->publish_ialh_binary_sensor();
        this->publish_overmod_binary_sensor();
        // this->publish_inlevel_sensor();
      }
    }
    // TODO: if (res.RDSINT == 1) {}
  }
}

// config

template<typename T> T GET_ENUM_LAST(T value) { return T::LAST; }

#define CHECK_ENUM(value) \
  if ((value) >= GET_ENUM_LAST(value)) { \
    ESP_LOGE(TAG, "%s(%d) invalid", __func__, (int) (value)); \
    return; \
  }

#define CHECK_FLOAT_RANGE(value, min_value, max_value) \
  if (!((min_value) <= (value) && (value) <= (max_value))) { \
    ESP_LOGE(TAG, "%s(%.2f) invalid (%.2f - %.2f)", __func__, value, min_value, max_value); \
    return; \
  }

#define CHECK_INT_RANGE(value, min_value, max_value) \
  if (!((min_value) <= (value) && (value) <= (max_value))) { \
    ESP_LOGE(TAG, "%s(%d) invalid (%d - %d)", __func__, value, min_value, max_value); \
    return; \
  }

#define CHECK_TEXT_RANGE(value, max_size) \
  if ((value).size() > (max_size)) { \
    ESP_LOGW(TAG, "%s(%s) trimmed (max %d characters)", __func__, (value).c_str(), max_size); \
    (value).resize(max_size); \
  }

void Si4713Component::set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }

void Si4713Component::set_op_mode(OpMode value) { this->op_mode_ = (OpMode) value; }

void Si4713Component::set_mute(bool value) {
  this->tx_line_input_mute_.LIMUTE = value ? 1 : 0;
  this->tx_line_input_mute_.RIMUTE = value ? 1 : 0;
  this->set_prop_(this->tx_line_input_mute_);
  this->publish_mute();
}

bool Si4713Component::get_mute() {
  return this->tx_line_input_mute_.LIMUTE != 0 && this->tx_line_input_mute_.RIMUTE != 0;
}

void Si4713Component::set_mono(bool value) {
  // NOTE: analog/digital mono linked, easier to control this way
  this->tx_component_enable_.LMR = value ? 0 : 1;
  this->set_prop_(this->tx_component_enable_);
  this->publish_mono();
  this->digital_input_format_.IMONO = value ? 1 : 0;
  this->set_prop_(this->digital_input_format_);
  this->publish_digital_channels();
}

bool Si4713Component::get_mono() { return this->tx_component_enable_.LMR == 0; }

void Si4713Component::set_pre_emphasis(PreEmphasis value) {
  CHECK_ENUM(value)
  this->tx_pre_emphasis_.FMPE = (uint16_t) value;
  this->set_prop_(this->tx_pre_emphasis_);
  this->publish_pre_emphasis();
}

PreEmphasis Si4713Component::get_pre_emphasis() { return (PreEmphasis) tx_pre_emphasis_.FMPE; }

void Si4713Component::set_tuner_enable(bool value) {
  this->power_enable_ = value;
  this->tune_power_(this->power_, this->antcap_);
  // this->set_prop_(this->tx_component_enable_);
  this->publish_tuner_enable();
}

bool Si4713Component::get_tuner_enable() { return this->power_enable_; }

void Si4713Component::set_tuner_frequency(float value) {
  CHECK_FLOAT_RANGE(value, FREQ_MIN, FREQ_MAX)
  this->frequency_ = (uint16_t) clamp((int) std::lround(value * 20) * 5, FREQ_RAW_MIN, FREQ_RAW_MAX);
  this->tune_freq_(this->frequency_);
  this->publish_tuner_frequency();
}

float Si4713Component::get_tuner_frequency() { return (float) this->frequency_ / 100; }

void Si4713Component::set_tuner_deviation(float value) {
  CHECK_FLOAT_RANGE(value, TXADEV_MIN, TXADEV_MAX)
  this->tx_audio_deviation_.TXADEV = (uint16_t) clamp((int) std::lround(value * 100), TXADEV_RAW_MIN, TXADEV_RAW_MAX);
  this->set_prop_(this->tx_audio_deviation_);
  this->publish_tuner_deviation();
}

float Si4713Component::get_tuner_deviation() { return (float) this->tx_audio_deviation_.TXADEV / 100; }

void Si4713Component::set_tuner_power(int value) {
  CHECK_INT_RANGE(value, POWER_MIN, POWER_MAX)
  this->power_ = (uint8_t) value;
  this->tune_power_(this->power_, this->antcap_);
  this->publish_tuner_power();
}

int Si4713Component::get_tuner_power() { return (int) this->power_; }

void Si4713Component::set_tuner_antcap(float value) {
  CHECK_FLOAT_RANGE(value, ANTCAP_MIN, ANTCAP_MAX)
  this->antcap_ = (uint8_t) lround(value * 4);
  this->tune_power_(this->power_, this->antcap_);
  this->publish_tuner_antcap();
}

float Si4713Component::get_tuner_antcap() { return (float) this->antcap_ * 0.25f; }

void Si4713Component::set_analog_level(int value) {
  CHECK_INT_RANGE(value, LILEVEL_MIN, LILEVEL_MAX)
  this->tx_line_input_level_.LILEVEL = (uint16_t) value;
  this->set_prop_(this->tx_line_input_level_);
  this->publish_analog_level();
}

int Si4713Component::get_analog_level() { return (int) this->tx_line_input_level_.LILEVEL; }

void Si4713Component::set_analog_attenuation(LineAttenuation value) {
  CHECK_ENUM(value)
  this->tx_line_input_level_.LIATTEN = (uint16_t) value;
  this->set_prop_(this->tx_line_input_level_);
  this->publish_analog_attenuation();
}

LineAttenuation Si4713Component::get_analog_attenuation() {
  return (LineAttenuation) this->tx_line_input_level_.LIATTEN;
}

void Si4713Component::set_digital_sample_rate(int value) {
  CHECK_INT_RANGE(value, DISR_MIN, DISR_MAX)
  this->digital_input_sample_rate_.DISR = (uint16_t) value;
  this->set_prop_(this->digital_input_sample_rate_);
  this->publish_digital_sample_rate();
}

int Si4713Component::get_digital_sample_rate() { return (int) this->digital_input_sample_rate_.DISR; }

void Si4713Component::set_digital_sample_bits(SampleBits value) {
  CHECK_ENUM(value)
  this->digital_input_format_.ISIZE = (uint16_t) value;
  this->set_prop_(this->digital_input_format_);
  this->publish_digital_sample_bits();
}

SampleBits Si4713Component::get_digital_sample_bits() { return (SampleBits) this->digital_input_format_.ISIZE; }

void Si4713Component::set_digital_channels(SampleChannels value) {
  CHECK_ENUM(value)
  this->digital_input_format_.IMONO = (uint16_t) value;
  this->set_prop_(this->digital_input_format_);
  this->publish_digital_channels();
}

SampleChannels Si4713Component::get_digital_channels() { return (SampleChannels) this->digital_input_format_.IMONO; }

void Si4713Component::set_digital_mode(DigitalMode value) {
  CHECK_ENUM(value)
  switch (value) {
    case DigitalMode::IMODE_I2S:
      this->digital_input_format_.IMODE = 1;
      break;
    case DigitalMode::IMODE_LEFT_JUSTIFIED:
      this->digital_input_format_.IMODE = 7;
      break;
    case DigitalMode::IMODE_MSB_AT_1ST:
      this->digital_input_format_.IMODE = 13;
      break;
    case DigitalMode::IMODE_MSB_AT_2ND:
      this->digital_input_format_.IMODE = 9;
      break;
    case DigitalMode::IMODE_DEFAULT:
    default:
      this->digital_input_format_.IMODE = 0;
      break;
  }
  this->set_prop_(this->digital_input_format_);
  this->publish_digital_mode();
}

DigitalMode Si4713Component::get_digital_mode() {
  switch (this->digital_input_format_.IMODE) {
    case 1:
      return DigitalMode::IMODE_I2S;
    case 7:
      return DigitalMode::IMODE_LEFT_JUSTIFIED;
    case 13:
      return DigitalMode::IMODE_MSB_AT_1ST;
    case 9:
      return DigitalMode::IMODE_MSB_AT_2ND;
    case 0:
    default:
      return DigitalMode::IMODE_DEFAULT;
  }
}

void Si4713Component::set_digital_clock_edge(DigitalClockEdge value) {
  CHECK_ENUM(value)
  this->digital_input_format_.IFALL = (uint16_t) value;
  this->set_prop_(this->digital_input_format_);
  this->publish_digital_clock_edge();
}

DigitalClockEdge Si4713Component::get_digital_clock_edge() { return (DigitalClockEdge) digital_input_format_.IFALL; }

void Si4713Component::set_pilot_enable(bool value) {
  this->tx_component_enable_.PILOT = value ? 1 : 0;
  this->set_prop_(this->tx_component_enable_);
  this->publish_pilot_enable();
}

bool Si4713Component::get_pilot_enable() { return this->tx_component_enable_.PILOT != 0; }

void Si4713Component::set_pilot_frequency(float value) {
  CHECK_FLOAT_RANGE(value, PILOT_FREQ_MIN, PILOT_FREQ_MAX)
  this->tx_pilot_frequency_.FREQ =
      (uint16_t) clamp((int) std::lround(value * 1000), PILOT_FREQ_RAW_MIN, PILOT_FREQ_RAW_MAX);
  this->set_prop_(this->tx_pilot_frequency_);
  this->publish_pilot_frequency();
}

float Si4713Component::get_pilot_frequency() { return (float) this->tx_pilot_frequency_.FREQ / 1000; }

void Si4713Component::set_pilot_deviation(float value) {
  CHECK_FLOAT_RANGE(value, TXPDEV_MIN, TXPDEV_MAX)
  this->tx_pilot_deviation_.TXPDEV = (uint16_t) clamp((int) std::lround(value * 100), TXPDEV_RAW_MIN, TXPDEV_RAW_MAX);
  this->set_prop_(this->tx_pilot_deviation_);
  this->publish_pilot_deviation();
}

float Si4713Component::get_pilot_deviation() { return (float) this->tx_pilot_deviation_.TXPDEV / 100; }

void Si4713Component::set_refclk_frequency(int value) {
  CHECK_INT_RANGE(value, REFCLKF_MIN, REFCLKF_MAX)
  this->refclk_freq_.REFCLKF = (uint16_t) value;
  this->set_prop_(this->refclk_freq_);
  this->publish_refclk_frequency();
}

int Si4713Component::get_refclk_frequency() { return this->refclk_freq_.REFCLKF; }

void Si4713Component::set_refclk_source(RefClkSource value) {
  CHECK_ENUM(value)
  this->refclk_prescale_.RCLKSEL = (uint16_t) value;
  this->set_prop_(this->refclk_prescale_);
  this->publish_refclk_source();
}

RefClkSource Si4713Component::get_refclk_source() { return (RefClkSource) this->refclk_prescale_.RCLKSEL; }

void Si4713Component::set_refclk_prescaler(int value) {
  CHECK_INT_RANGE(value, RCLKP_MIN, RCLKP_MAX)
  this->refclk_prescale_.RCLKP = (uint16_t) value;
  this->set_prop_(this->refclk_freq_);
  this->publish_refclk_prescaler();
}

int Si4713Component::get_refclk_prescaler() { return (int) this->refclk_prescale_.RCLKP; }

void Si4713Component::set_acomp_enable(bool value) {
  this->tx_acomp_enable_.ACEN = value ? 1 : 0;
  this->set_prop_(this->tx_acomp_enable_);
  this->publish_acomp_enable();
}

bool Si4713Component::get_acomp_enable() { return this->tx_acomp_enable_.ACEN != 0; }

void Si4713Component::set_acomp_threshold(int value) {
  CHECK_INT_RANGE(value, ACOMP_THRESHOLD_MIN, ACOMP_THRESHOLD_MAX)
  this->tx_acomp_threshold_.THRESHOLD = (int16_t) value;
  this->set_prop_(this->tx_acomp_threshold_);
  this->publish_acomp_threshold();
}

int Si4713Component::get_acomp_threshold() { return (int) this->tx_acomp_threshold_.THRESHOLD; }

void Si4713Component::set_acomp_attack(AcompAttack value) {
  CHECK_ENUM(value)
  this->tx_acomp_attack_time_.ATTACK = (uint16_t) value;
  this->set_prop_(this->tx_acomp_attack_time_);
  this->publish_acomp_attack();
}

AcompAttack Si4713Component::get_acomp_attack() { return (AcompAttack) tx_acomp_attack_time_.ATTACK; }

void Si4713Component::set_acomp_release(AcompRelease value) {
  CHECK_ENUM(value)
  this->tx_acomp_release_time_.RELEASE = (uint16_t) value;
  this->set_prop_(this->tx_acomp_release_time_);
  this->publish_acomp_release();
}

AcompRelease Si4713Component::get_acomp_release() { return (AcompRelease) this->tx_acomp_release_time_.RELEASE; }

void Si4713Component::set_acomp_gain(int value) {
  CHECK_INT_RANGE(value, ACOMP_GAIN_MIN, ACOMP_GAIN_MAX)
  this->tx_acomp_gain_.GAIN = (int16_t) value;
  this->set_prop_(this->tx_acomp_gain_);
  this->publish_acomp_gain();
}

int Si4713Component::get_acomp_gain() { return (int) this->tx_acomp_gain_.GAIN; }

void Si4713Component::set_acomp_preset(AcompPreset value) {
  CHECK_ENUM(value)
  this->tx_acomp_preset_ = value;
  switch (value) {
    case AcompPreset::ACOMP_MINIMAL:
      this->set_acomp_threshold(-40);
      this->set_acomp_attack(AcompAttack::ATTACK_50MS);
      this->set_acomp_release(AcompRelease::RELEASE_100MS);
      this->set_acomp_gain(15);
      break;
    case AcompPreset::ACOMP_AGGRESSIVE:
      this->set_acomp_threshold(-15);
      this->set_acomp_attack(AcompAttack::ATTACK_05MS);
      this->set_acomp_release(AcompRelease::RELEASE_1000MS);
      this->set_acomp_gain(5);
      break;
    case AcompPreset::ACOMP_CUSTOM:
    default:
      break;
  }
  this->publish_acomp_preset();
}

AcompPreset Si4713Component::get_acomp_preset() { return this->tx_acomp_preset_; }

void Si4713Component::set_limiter_enable(bool value) {
  this->tx_acomp_enable_.LIMITEN = value ? 1 : 0;
  this->set_prop_(this->tx_acomp_enable_);
  this->publish_limiter_enable();
}

bool Si4713Component::get_limiter_enable() { return this->tx_acomp_enable_.LIMITEN != 0; }

void Si4713Component::set_limiter_release_time(float value) {
  CHECK_FLOAT_RANGE(value, LMITERTC_MIN, LMITERTC_MAX)
  this->tx_limiter_releasee_time_.LMITERTC =
      (uint16_t) clamp((int) std::lround(512.0f / value), LMITERTC_RAW_MIN, LMITERTC_RAW_MAX);
  this->set_prop_(this->tx_limiter_releasee_time_);
  this->publish_limiter_release_time();
}

float Si4713Component::get_limiter_release_time() { return (float) 512.0f / this->tx_limiter_releasee_time_.LMITERTC; }

void Si4713Component::set_asq_iall(bool value) {
  this->tx_asq_interrupt_source_.IALLIEN = value ? 1 : 0;
  this->set_prop_(this->tx_asq_interrupt_source_);
  this->publish_asq_iall();
}

bool Si4713Component::get_asq_iall() { return this->tx_asq_interrupt_source_.IALLIEN != 0; }

void Si4713Component::set_asq_ialh(bool value) {
  this->tx_asq_interrupt_source_.IALHIEN = value ? 1 : 0;
  this->set_prop_(this->tx_asq_interrupt_source_);
  this->publish_asq_ialh();
}

bool Si4713Component::get_asq_ialh() { return this->tx_asq_interrupt_source_.IALHIEN != 0; }

void Si4713Component::set_asq_overmod(bool value) {
  this->tx_asq_interrupt_source_.OVERMODIEN = value ? 1 : 0;
  this->set_prop_(this->tx_asq_interrupt_source_);
  this->publish_asq_overmod();
}

bool Si4713Component::get_asq_overmod() { return this->tx_asq_interrupt_source_.OVERMODIEN != 0; }

void Si4713Component::set_asq_level_low(int value) {
  CHECK_INT_RANGE(value, IALTH_MIN, IALTH_MAX)
  this->tx_asq_level_low_.IALLTH = (int8_t) value;
  this->set_prop_(this->tx_asq_level_low_);
  this->publish_asq_level_low();
}

int Si4713Component::get_asq_level_low() { return (int) this->tx_asq_level_low_.IALLTH; }

void Si4713Component::set_asq_duration_low(int value) {
  CHECK_INT_RANGE(value, IALDUR_MIN, IALDUR_MAX)
  this->tx_asq_duration_low_.IALLDUR = (uint16_t) value;
  this->set_prop_(this->tx_asq_duration_low_);
  this->publish_asq_duration_low();
}

int Si4713Component::get_asq_duration_low() { return (int) this->tx_asq_duration_low_.IALLDUR; }

void Si4713Component::set_asq_level_high(int value) {
  CHECK_INT_RANGE(value, IALTH_MIN, IALTH_MAX)
  this->tx_asq_level_high_.IALHTH = (int8_t) value;
  this->set_prop_(this->tx_asq_level_high_);
  this->publish_asq_level_high();
}

int Si4713Component::get_asq_level_high() { return (int) this->tx_asq_level_high_.IALHTH; }

void Si4713Component::set_asq_duration_high(int value) {
  CHECK_INT_RANGE(value, IALDUR_MIN, IALDUR_MAX)
  this->tx_asq_duration_high_.IALHDUR = (uint16_t) value;
  this->set_prop_(this->tx_asq_duration_high_);
  this->publish_asq_duration_high();
}

int Si4713Component::get_asq_duration_high() { return (int) this->tx_asq_duration_high_.IALHDUR; }

void Si4713Component::set_rds_enable(bool value) {
  this->tx_component_enable_.RDS = value ? 1 : 0;
  this->set_prop_(this->tx_component_enable_);
  this->publish_rds_enable();
}

bool Si4713Component::get_rds_enable() { return this->tx_component_enable_.RDS != 0; }

void Si4713Component::set_rds_deviation(float value) {
  CHECK_FLOAT_RANGE(value, TXRDEV_MIN, TXRDEV_MAX)
  this->tx_rds_deviation_.TXRDEV = (uint16_t) clamp((int) std::lround(value * 100), TXRDEV_RAW_MIN, TXRDEV_RAW_MAX);
  this->set_prop_(this->tx_rds_deviation_);
  this->publish_rds_deviation();
}

float Si4713Component::get_rds_deviation() { return (float) this->tx_rds_deviation_.TXRDEV / 100; }

void Si4713Component::set_rds_station(const std::string &value) {
  this->rds_station_ = value;
  // this->rds_station_pos_ = 0;
  CHECK_TEXT_RANGE(this->rds_station_, RDS_STATION_MAX)
  this->publish_rds_station();
}

std::string Si4713Component::get_rds_station() { return this->rds_station_; }

void Si4713Component::set_rds_text(const std::string &value) {
  this->rds_text_ = value;
  CHECK_TEXT_RANGE(this->rds_text_, RDS_TEXT_MAX)
  this->publish_rds_text();
}

std::string Si4713Component::get_rds_text() { return this->rds_text_; }

void Si4713Component::set_output_gpio(uint8_t pin, bool value) {
  if (pin > 2) {
    ESP_LOGE(TAG, "%s(%d, %d) invalid pin number (0 to 2)", __func__, pin, value);
    return;
  }
  this->gpio_[pin] = value ? 1 : 0;
  this->send_cmd_(CmdGpioSet(this->gpio_[0], this->gpio_[1], this->gpio_[2]));
  this->publish_output_gpio(pin);
}

bool Si4713Component::get_output_gpio(uint8_t pin) {
  if (pin > 2) {
    ESP_LOGE(TAG, "%s(%d) invalid pin number (0 to 2)", __func__, pin);
    return false;
  }
  return this->gpio_[pin] != 0;
}

void Si4713Component::set_output_gpio1(bool value) {
  this->gpio_[0] = value ? 1 : 0;
  this->send_cmd_(CmdGpioSet(this->gpio_[0], this->gpio_[1], this->gpio_[2]));
  this->publish_output_gpio1();
}

bool Si4713Component::get_output_gpio1() { return this->gpio_[0] != 0; }

void Si4713Component::set_output_gpio2(bool value) {
  this->gpio_[1] = value ? 1 : 0;
  this->send_cmd_(CmdGpioSet(this->gpio_[0], this->gpio_[1], this->gpio_[2]));
  this->publish_output_gpio2();
}

bool Si4713Component::get_output_gpio2() { return this->gpio_[1] != 0; }

void Si4713Component::set_output_gpio3(bool value) {
  this->gpio_[2] = value ? 1 : 0;
  this->send_cmd_(CmdGpioSet(this->gpio_[0], this->gpio_[1], this->gpio_[2]));
  this->publish_output_gpio2();
}

bool Si4713Component::get_output_gpio3() { return this->gpio_[2] != 0; }

std::string Si4713Component::get_chip_id_text_sensor() { return this->chip_id_; }

float Si4713Component::get_frequency_sensor() {
  return (float) ((this->tune_status_.READFREQH << 8) | this->tune_status_.READFREQL) / 100;
}

float Si4713Component::get_power_sensor() { return (float) this->tune_status_.READRFdBuV; }

float Si4713Component::get_antcap_sensor() { return (float) this->tune_status_.READANTCAP; }

float Si4713Component::get_noise_level_sensor() { return (float) this->tune_status_.RNL; }

bool Si4713Component::get_iall_binary_sensor() { return this->asq_status_.IALL != 0; }

bool Si4713Component::get_ialh_binary_sensor() { return this->asq_status_.IALH != 0; }

bool Si4713Component::get_overmod_binary_sensor() { return this->asq_status_.OVERMOD != 0; }

float Si4713Component::get_inlevel_sensor() { return (float) this->asq_status_.INLEVEL; }

// publish

void Si4713Component::publish_output_gpio(uint8_t pin) {
  switch (pin) {
    case 0:
      this->publish_switch(this->output_gpio1_switch_, this->gpio_[pin] != 0);
      break;
    case 1:
      this->publish_switch(this->output_gpio2_switch_, this->gpio_[pin] != 0);
      break;
    case 2:
      this->publish_switch(this->output_gpio3_switch_, this->gpio_[pin] != 0);
      break;
    default:
      return;
  }
}

template<class S, class T> void Si4713Component::publish(S *s, T state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void Si4713Component::publish_switch(switch_::Switch *s, bool state) {
  if (s != nullptr) {
    if (s->state != state) {  // ?
      s->publish_state(state);
    }
  }
}

void Si4713Component::publish_select(select::Select *s, size_t index) {
  if (s != nullptr) {
    if (auto state = s->at(index)) {
      if (!s->has_state() || s->state != *state) {
        s->publish_state(*state);
      }
    }
  }
}

void Si4713Component::measure_freq(float value) {
  uint16_t f = (uint16_t) clamp((int) std::lround(value * 20) * 5, FREQ_RAW_MIN, FREQ_RAW_MAX);
  if (!this->send_cmd_(CmdTxTuneMeasure(f))) {
    return;
  }

  // this->stc_wait(); // does not work, locks up, hmm
}

}  // namespace si4713_i2c
}  // namespace esphome
