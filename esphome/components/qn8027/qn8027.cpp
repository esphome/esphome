#include "qn8027.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <stdio.h>

namespace esphome {
namespace qn8027 {

static const char *const TAG = "qn8027";

QN8027Component::QN8027Component() {
  this->reset_ = false;
  memset(&this->state_, 0, sizeof(this->state_));
  this->rds_station_pos_ = 0;
  this->rds_text_pos_ = 0;
  this->rds_upd_ = 0xff;        // set defaults according to the datasheet
  this->state_.CH_UPPER = 1;    // 76 + [256] * 0.05MHz (88.8MHz)
  this->state_.GAIN_TXPLT = 9;  // [9%] * 75KHz
  this->state_.t1m_sel = 2;     // 60s
  this->state_.TC = 1;          // pre-emphasis time 75us
  this->state_.XISEL = 16;      // crystal oscillator current 6.25uA * [16] (100uA)
  this->state_.RIN = 2;         // input inpedance 20kOhm
  this->state_.GVGA = 3;        // input buffer gain
  this->state_.XSEL = 1;        // crystal 24MHz
  this->state_.PA_TRGT = 127;   // pa output power target 0.62 * [127] + 71dBuV (149.74dBuV)
  this->state_.TX_FDEV = 129;   // tx frequency deviation 0.58KHz * [129] (74.82KHz)
  this->state_.RDSFDEV = 6;     // rds frequency deviation = 0.35KHz * [6] (2.1KHz)
  // our defaults
  this->state_.TXREQ = 1;     // start with tx enabled
  this->state_.PA_TRGT = 75;  // max valid value
}

void QN8027Component::write_reg_(uint8_t addr) {
  uint8_t mask = 0xff;
  size_t count = 1;
  switch (addr) {
    case REG_SYSTEM_ADDR:
      if (this->state_.SWRST)
        this->reset_ = true;
      if (this->state_.SWRST)
        mask = 0b11000000;  // 0x80;
      if (this->state_.RECAL)
        mask = 0b11000000;  // 0x40;
      break;
    case REG_CH1_ADDR:
      break;
    case REG_GPLT_ADDR:
      break;
    case REG_XTL_ADDR:
      break;
    case REG_VGA_ADDR:
      break;
    case REG_RDSD_ADDR:
      count = 8;
      break;
    case REG_PAC_ADDR:
      break;
    case REG_FDEV_ADDR:
      break;
    case REG_RDS_ADDR:
      break;
    default:
      ESP_LOGE(TAG, "write_reg_(0x%02X) trying to write invalid register", addr);
      return;
  }

  if (this->reset_) {
    char buff[3];
    std::string s;
    for (size_t i = 0; i < count; i++) {
      snprintf(buff, sizeof(buff), "%02X", this->regs_[addr + i] & mask);
      s += std::string(buff);
    }
    ESP_LOGV(TAG, "write_reg_(0x%02X/%d) = %s", addr, count, s.c_str());
    for (size_t i = 0; i < count; i++) {
      uint8_t value = this->regs_[addr + i] & mask;
      //      ESP_LOGV(TAG, "write_reg_(0x%02X) = 0x%02X", addr + i, value);
      this->write_byte(addr + i, value);
    }
  } else {
    if (this->get_component_state() & COMPONENT_STATE_LOOP) {
      ESP_LOGE(TAG, "write_reg_(0x%02X) device was not reset", addr);
    }
  }

  if (addr == REG_SYSTEM_ADDR) {
    this->state_.SWRST = 0;
    this->state_.RECAL = 0;
  }
}

bool QN8027Component::read_reg_(uint8_t addr) {
  uint8_t mask = 0xff;

  switch (addr) {
    case REG_SYSTEM_ADDR:
      mask = 0x80;  // can only read the reset bit
      break;
    case REG_CID1_ADDR:
      mask = 0x1f;  // the lower 5 bits are valid, upper 3 bits are reserved
      break;
    case REG_CID2_ADDR:
      break;
    case REG_STATUS_ADDR:
      break;
    default:
      ESP_LOGE(TAG, "read_reg_(0x%02X) trying to read invalid register", addr);
      return false;
  }

  if (auto b = this->read_byte(addr)) {
    this->regs_[addr] = (this->regs_[addr] & ~mask) | (*b & mask);
    return true;
  }

  ESP_LOGE(TAG, "read_reg_(0x%02X) cannot read register", addr);
  return false;
}

void QN8027Component::rds_update_() {
  // TODO: cannot send during FSM == IDLE and possibly other states
  if (!this->state_.RDSEN || this->rds_station_.empty() && this->rds_text_.empty()) {
    return;  // nothing to do
  }

  ESP_LOGV(TAG, "rds_update station: '%s' [%d/%d] text: '%s' [%d/%d]", this->rds_station_.c_str(),
           this->rds_station_pos_, this->rds_station_.size(), this->rds_text_.c_str(), this->rds_text_pos_,
           this->rds_text_.size());

  this->state_.RDSRDY ^= 1;
  this->write_reg_(REG_SYSTEM_ADDR);

  if (!this->read_reg_(REG_STATUS_ADDR)) {
    ESP_LOGE(TAG, "rds_update cannot read the status register");
    return;
  }

  if (this->rds_upd_ == this->state_.RDS_UPD) {
    ESP_LOGV(TAG, "rds_update still waiting for last rds send... %d == %d", this->rds_upd_, this->state_.RDS_UPD);
    return;
  }

  this->rds_upd_ = this->state_.RDS_UPD;

  if (this->rds_station_pos_ < this->rds_station_.size()) {
    const std::string &s = this->rds_station_;
    size_t i = this->rds_station_pos_;
    this->state_.REG_RDSD[0] = 0x64;
    this->state_.REG_RDSD[1] = 0x00;
    this->state_.REG_RDSD[2] = 0x02;
    this->state_.REG_RDSD[3] = 0x68 + (i >> 1);
    this->state_.REG_RDSD[4] = 0xE0;
    this->state_.REG_RDSD[5] = 0xCD;
    this->state_.REG_RDSD[6] = i + 0 < s.size() ? s[i + 0] : 0x00;
    this->state_.REG_RDSD[7] = i + 1 < s.size() ? s[i + 1] : 0x00;
    this->write_reg_(REG_RDSD_ADDR);
    this->rds_station_pos_ += 2;
  } else if (this->rds_text_pos_ < this->rds_text_.size()) {
    const std::string &s = this->rds_text_;
    size_t i = this->rds_text_pos_;
    this->state_.REG_RDSD[0] = 0x64;
    this->state_.REG_RDSD[1] = 0x00;
    this->state_.REG_RDSD[2] = 0x22;
    this->state_.REG_RDSD[3] = 0x60 + (i >> 2);
    this->state_.REG_RDSD[4] = i + 0 < s.size() ? s[i + 0] : 0x00;
    this->state_.REG_RDSD[5] = i + 1 < s.size() ? s[i + 1] : 0x00;
    this->state_.REG_RDSD[6] = i + 2 < s.size() ? s[i + 2] : 0x00;
    this->state_.REG_RDSD[7] = i + 3 < s.size() ? s[i + 3] : 0x00;
    this->write_reg_(REG_RDSD_ADDR);
    this->rds_text_pos_ += 4;
  } else {  // start all over
    this->rds_station_pos_ = 0;
    this->rds_text_pos_ = 0;
  }
}

// overrides

void QN8027Component::setup() {
  if (!this->read_reg_(REG_CID1_ADDR) || !this->read_reg_(REG_CID2_ADDR)) {
    ESP_LOGE(TAG, "setup cannot read the CID registers");
    this->mark_failed();
    return;
  }

  if (this->state_.CID1 != CID1_FM || this->state_.CID3 != CID3_QN8027) {
    ESP_LOGE(TAG, "setup cannot identify chip as QN8027 (CID1 %02x, CID3 %02x)", this->state_.CID1, this->state_.CID3);
    this->mark_failed();
    return;
  }

  // reset
  this->state_.SWRST = 1;
  this->write_reg_(REG_SYSTEM_ADDR);
  // TODO: add delay?
  delay_microseconds_safe(10000);
  // calibrate (TODO: single step with reset?)
  this->state_.RECAL = 1;
  this->write_reg_(REG_SYSTEM_ADDR);
  // TODO: add delay?
  delay_microseconds_safe(10000);
  // update the rest of the system register (may not be needed again)
  this->write_reg_(REG_SYSTEM_ADDR);
  // frequency lower bits and the rest of the settings
  this->write_reg_(REG_CH1_ADDR);
  this->write_reg_(REG_GPLT_ADDR);
  this->write_reg_(REG_XTL_ADDR);
  this->write_reg_(REG_VGA_ADDR);
  this->write_reg_(REG_PAC_ADDR);
  this->write_reg_(REG_FDEV_ADDR);
  this->write_reg_(REG_RDS_ADDR);

  this->publish_aud_pk_();
  this->publish_fsm_();
  this->publish_frequency_();
  this->publish_frequency_deviation_();
  this->publish_mute_();
  this->publish_mono_();
  this->publish_tx_enable_();
  this->publish_tx_pilot_();
  this->publish_t1m_sel_();

  if (this->get_update_interval() != SCHEDULER_DONT_RUN) {
    this->set_interval(this->get_update_interval(), [this]() {
      this->state_.TXPD_CLR ^= 1;
      this->write_reg_(REG_PAC_ADDR);
    });
  }

  this->set_interval(1000, [this]() { this->rds_update_(); });
}

void QN8027Component::dump_config() {
  ESP_LOGCONFIG(TAG, "QN8027:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "failed!");
  }
  ESP_LOGCONFIG(TAG, "  Chip: %s", this->state_.CID3 == CID3_QN8027 ? "QN8027" : "N/A");
  ESP_LOGCONFIG(TAG, "  Family: %s", this->state_.CID1 == CID1_FM ? "FM" : "N/A");
  ESP_LOGCONFIG(TAG, "  Revision: %d.%d", this->state_.CID4, this->state_.CID2);
  ESP_LOGCONFIG(TAG, "  Frequency: %.2f MHz", this->get_frequency());
  ESP_LOGCONFIG(TAG, "  RDS station: %s", this->rds_station_.c_str());
  ESP_LOGCONFIG(TAG, "  RDS text: %s", this->rds_text_.c_str());
  // TODO: ...and everything else...
  LOG_UPDATE_INTERVAL(this);
  LOG_NUMBER("  ", "Frequency", this->frequency_number_);
}

void QN8027Component::update() {
  if (this->read_reg_(REG_STATUS_ADDR)) {
    this->publish_aud_pk_();
    this->publish_fsm_();
  } else {
    ESP_LOGE(TAG, "update cannot read the status register");
  }
}

void QN8027Component::loop() {
  // TODO: check how often this runs, maybe do this in set_interval(1000, ... above
  if (this->read_reg_(REG_STATUS_ADDR)) {
    this->publish_fsm_();
  }
}

// config

void QN8027Component::set_frequency(float value) {
  if (!(CH_FREQ_MIN <= value && value <= CH_FREQ_MAX)) {
    ESP_LOGE(TAG, "frequency invalid %.2f (%.2f - %.2f)", value, CH_FREQ_MIN, CH_FREQ_MAX);
    return;
  }

  // avoid float to int conversion inaccuracies with round(x * 20)
  int f = std::max(std::min((int) round(value * 20) - 76 * 20, CH_FREQ_RAW_MAX), CH_FREQ_RAW_MIN);
  this->state_.CH_UPPER = (uint8_t) (f >> 8);
  this->state_.CH_LOWER = (uint8_t) (f & 0xff);
  this->write_reg_(REG_SYSTEM_ADDR);
  this->write_reg_(REG_CH1_ADDR);

  this->publish_frequency_();
}

float QN8027Component::get_frequency() {
  uint16_t CH = ((uint16_t) this->state_.CH_UPPER << 8) | this->state_.CH_LOWER;
  return (float) CH / 20 + 76;
}

void QN8027Component::set_frequency_deviation(float value) {
  if (!(TX_FDEV_MIN <= value && value <= TX_FDEV_MAX)) {
    ESP_LOGE(TAG, "frequency deviation invalid %.2f (%.2f - %.2f)", value, TX_FDEV_MIN, TX_FDEV_MAX);
    return;
  }

  this->state_.TX_FDEV = (uint8_t) std::max(std::min((int) (value / 0.58f), TX_FDEV_RAW_MAX), TX_FDEV_RAW_MIN);
  this->write_reg_(REG_FDEV_ADDR);

  this->publish_frequency_deviation_();
}

float QN8027Component::get_frequency_deviation() { return 0.58f * this->state_.TX_FDEV; }

void QN8027Component::set_mute(bool value) {
  this->state_.MUTE = value ? 1 : 0;
  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_mute_();
}

bool QN8027Component::get_mute() { return this->state_.MUTE == 1; }

void QN8027Component::set_mono(bool value) {
  this->state_.MONO = value ? 1 : 0;
  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_mono_();
}

bool QN8027Component::get_mono() { return this->state_.MONO == 1; }

void QN8027Component::set_tx_enable(bool value) {
  this->state_.TXREQ = value ? 1 : 0;
  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_tx_enable_();
}

bool QN8027Component::get_tx_enable() { return this->state_.TXREQ == 1; }

void QN8027Component::set_tx_pilot(uint8_t value) {
  if (!(GAIN_TXPLT_MIN <= value && value <= GAIN_TXPLT_MAX)) {
    ESP_LOGE(TAG, "tx_pilot invalid %d, must be 7-15", value);
    return;
  }

  this->state_.GAIN_TXPLT = value;
  this->write_reg_(REG_GPLT_ADDR);

  this->publish_tx_pilot_();
}

uint8_t QN8027Component::get_tx_pilot() { return (uint8_t) this->state_.GAIN_TXPLT; }

void QN8027Component::set_t1m_sel(T1mSel value) {
  if (value >= T1mSel::LAST) {
    ESP_LOGE(TAG, "t1m_sel invalid %d, must be 58/59/60 or 0 (never)", value);
    return;
  }

  this->state_.t1m_sel = (uint8_t) value;
  this->write_reg_(REG_GPLT_ADDR);

  this->publish_t1m_sel_();
}

void QN8027Component::set_t1m_sel(uint8_t value) {
  if (!(value == 0 || 58 <= value && value <= 60)) {
    ESP_LOGE(TAG, "t1m_sel invalid %d, must be 58/59/60 or 0 (never)", value);
    return;
  }

  if (value == 0) {
    this->set_t1m_sel(T1mSel::T1M_SEL_NEVER);
  } else {
    this->set_t1m_sel((T1mSel) (value - 58));
  }
}

T1mSel QN8027Component::get_t1m_sel() { return (T1mSel) this->state_.t1m_sel; }

void QN8027Component::set_privacy_mode(bool b) {
  this->state_.priv_en = b ? 1 : 0;
  this->write_reg_(REG_GPLT_ADDR);
}

void QN8027Component::set_pre_emphasis(PreEmphasis us) {
  if (us >= PreEmphasis::LAST) {
    ESP_LOGE(TAG, "pre-emphasis invalid %d, must be 50/75", us);
    return;
  }

  this->state_.TC = (uint8_t) us;
  this->write_reg_(REG_GPLT_ADDR);
}

void QN8027Component::set_xtal_source(XtalSource source) {
  if (source >= XtalSource::LAST) {
    ESP_LOGE(TAG, "xtal source invalid %d", source);
    return;
  }

  this->state_.XINJ = (uint8_t) source;
  this->write_reg_(REG_XTL_ADDR);
}

void QN8027Component::set_xtal_current(uint16_t uamps) {
  this->state_.XISEL = uamps >= XISEL_MAX ? 0b00111111 : uamps * 100 / 625;
  if (this->state_.XINJ != (uint8_t) XtalSource::USE_CRYSTAL_ON_XTAL12) {
    ESP_LOGW(TAG, "xtal source should be %d", XtalSource::USE_CRYSTAL_ON_XTAL12);
  }

  this->write_reg_(REG_XTL_ADDR);
}

void QN8027Component::set_xtal_frequency(XtalFrequency mhz) {
  if (mhz >= XtalFrequency::LAST) {
    ESP_LOGE(TAG, "crystal frequency invalid %d, must be 12/24", mhz);
    return;
  }

  this->state_.XSEL = (uint8_t) mhz;
  this->write_reg_(REG_VGA_ADDR);
}

void QN8027Component::set_input_impedance(InputImpedance kohms) {
  if (kohms >= InputImpedance::LAST) {
    ESP_LOGE(TAG, "input impedance invalid %d, must be 5/10/20/40", kohms);
    return;
  }

  this->state_.RIN = (uint8_t) kohms;
  this->write_reg_(REG_VGA_ADDR);
}

void QN8027Component::set_input_gain(uint8_t dB) {
  if (dB > GVGA_MAX) {
    ESP_LOGE(TAG, "input gain invalid %d, max %ddB", dB, GVGA_MAX);
    return;
  }

  this->state_.GVGA = dB;
  this->write_reg_(REG_VGA_ADDR);
}

void QN8027Component::set_digital_gain(uint8_t dB) {
  if (dB > GDB_MAX) {
    ESP_LOGE(TAG, "digital gain invalid %d, max %ddB", dB, GDB_MAX);
    return;
  }

  this->state_.GDB = dB;
  this->write_reg_(REG_VGA_ADDR);
}

void QN8027Component::set_power_target(float dBuV) {
  if (!(PA_TRGT_MIN <= dBuV && dBuV <= PA_TRGT_MAX)) {
    ESP_LOGE(TAG, "power target invalid %.2f, must be between %.2f and %.2f", dBuV, PA_TRGT_MIN, PA_TRGT_MAX);
    return;
  }

  this->state_.PA_TRGT = (uint8_t) ((std::max(std::min(dBuV, PA_TRGT_MAX), PA_TRGT_MIN) - 71) / 0.62f);
  this->write_reg_(REG_PAC_ADDR);
}

void QN8027Component::set_rds_enable(bool b) {
  this->state_.RDSEN = b ? 1 : 0;
  this->write_reg_(REG_RDS_ADDR);
}

void QN8027Component::set_rds_frequency_deviation(float khz) {
  this->state_.RDSFDEV = (uint8_t) std::max(std::min((int) (khz / 0.35f), 127), 0);
  this->write_reg_(REG_RDS_ADDR);
}

void QN8027Component::set_rds_station(const std::string &s) {
  this->rds_station_ = s;
  this->rds_station_pos_ = 0;
  if (this->rds_station_.size() > RDS_STATION_MAX_SIZE) {
    ESP_LOGW(TAG, "rds station too long '%s' (max %d characters)", s.c_str(), RDS_STATION_MAX_SIZE);
    this->rds_station_.resize(RDS_STATION_MAX_SIZE);
  }
}

void QN8027Component::set_rds_text(const std::string &s) {
  this->rds_text_ = s;
  this->rds_text_pos_ = 0;
  if (this->rds_text_.size() > RDS_TEXT_MAX_SIZE) {
    ESP_LOGW(TAG, "rds text to long '%s' (max %d characters)", s.c_str(), RDS_TEXT_MAX_SIZE);
    this->rds_text_.resize(RDS_TEXT_MAX_SIZE);
  }
}

// publish

void QN8027Component::publish_aud_pk_() {
  if (this->aud_pk_sensor_ != nullptr) {
    float value = 45.0f * this->state_.aud_pk;
    if (!this->aud_pk_sensor_->has_state() || this->aud_pk_sensor_->state != value) {
      this->aud_pk_sensor_->publish_state(value);
    }
  }
}

void QN8027Component::publish_fsm_() {
  if (this->fsm_text_sensor_ != nullptr) {
    const char *value = NULL;
    switch (this->state_.FSM) {
      case 0:
        value = "RESET";
        break;
      case 1:
        value = "CALI";
        break;
      case 2:
        value = "IDLE";
        break;
      case 3:
        value = "TX_RSTB";
        break;
      case 4:
        value = "PA_CALIB";
        break;
      case 5:
        value = "TRANSMIT";
        break;
      case 6:
        value = "PA_OFF";
        break;
      case 7:
        value = "RESERVED";
        break;
      default:
        return;
    }
    if (!this->fsm_text_sensor_->has_state() || this->fsm_text_sensor_->state != value) {
      this->fsm_text_sensor_->publish_state(value);
    }
  }
}

void QN8027Component::publish_frequency_() {
  if (this->frequency_number_ != nullptr) {
    float value = this->get_frequency();
    if (!this->frequency_number_->has_state() || this->frequency_number_->state != value) {
      this->frequency_number_->publish_state(value);
    }
  }
}

void QN8027Component::publish_frequency_deviation_() {
  if (this->frequency_deviation_number_ != nullptr) {
    float value = this->get_frequency_deviation();
    if (!this->frequency_deviation_number_->has_state() || this->frequency_deviation_number_->state != value) {
      this->frequency_deviation_number_->publish_state(value);
    }
  }
}

void QN8027Component::publish_mute_() {
  if (this->mute_switch_ != nullptr) {
    bool value = this->get_mute();
    this->mute_switch_->publish_state(value);
  }
}

void QN8027Component::publish_mono_() {
  if (this->mono_switch_ != nullptr) {
    bool value = this->get_mono();
    this->mono_switch_->publish_state(value);
  }
}

void QN8027Component::publish_tx_enable_() {
  if (this->tx_enable_switch_ != nullptr) {
    bool value = this->get_tx_enable();
    this->tx_enable_switch_->publish_state(value);
  }
}

void QN8027Component::publish_tx_pilot_() {
  if (this->tx_pilot_number_ != nullptr) {
    float value = (float) this->get_tx_pilot();
    if (!this->tx_pilot_number_->has_state() || this->tx_pilot_number_->state != value) {
      this->tx_pilot_number_->publish_state(value);
    }
  }
  if (this->tx_pilot_select_ != nullptr) {
    std::string value = std::to_string(this->get_tx_pilot()) + "%";
    if (!this->tx_pilot_select_->has_state() || this->tx_pilot_select_->state != value) {
      this->tx_pilot_select_->publish_state(value);
    }
  }
}

void QN8027Component::publish_t1m_sel_() {
  if (this->t1m_sel_number_ != nullptr) {
    float value;
    switch (this->get_t1m_sel()) {
      case T1mSel::T1M_SEL_58S:
        value = 58;
        break;
      case T1mSel::T1M_SEL_59S:
        value = 59;
        break;
      case T1mSel::T1M_SEL_60S:
        value = 60;
        break;
      case T1mSel::T1M_SEL_NEVER:
        value = 0;
        break;
      default:
        return;
    }
    if (!this->t1m_sel_number_->has_state() || this->t1m_sel_number_->state != value) {
      this->t1m_sel_number_->publish_state(value);
    }
  }
  if (this->t1m_sel_select_ != nullptr) {
    std::string value;
    switch (this->get_t1m_sel()) {
      case T1mSel::T1M_SEL_58S:
        value = "58s";
        break;
      case T1mSel::T1M_SEL_59S:
        value = "59s";
        break;
      case T1mSel::T1M_SEL_60S:
        value = "60s";
        break;
      case T1mSel::T1M_SEL_NEVER:
        value = "Never";
        break;
      default:
        return;
    }
    if (!this->t1m_sel_select_->has_state() || this->t1m_sel_select_->state != value) {
      this->t1m_sel_select_->publish_state(value);
    }
  }
}

}  // namespace qn8027
}  // namespace esphome