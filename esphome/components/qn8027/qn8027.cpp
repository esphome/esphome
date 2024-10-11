#include "qn8027.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cmath>

namespace esphome {
namespace qn8027 {

// TODO: std::clamp isn't here yet
#define clamp(v, lo, hi) std::max(std::min(v, hi), lo)

static const char *const TAG = "qn8027";

QN8027Component::QN8027Component() {
  this->reset_ = false;
  memset(&this->state_, 0, sizeof(this->state_));
  this->reg30_ = 0;
  this->rds_station_pos_ = 0;
  this->rds_text_pos_ = 0;
  this->rds_upd_ = 0xff;        // set defaults according to the datasheet
  this->state_.CH_UPPER = 1;    // 76 + [256] * 0.05MHz (88.8MHz)
  this->state_.GAIN_TXPLT = 9;  // [9%] * 75KHz
  this->state_.t1m_sel = 2;     // 60s
  this->state_.TC = 1;          // pre-emphasis time 75us
  this->state_.XISEL = 16;      // crystal oscillator current 6.25uA * [16] (100uA)
  this->state_.RIN = 2;         // input impedance 20kOhm
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
    case REG_GPLT_ADDR:
    case REG_XTL_ADDR:
    case REG_VGA_ADDR:
      break;
    case REG_RDSD_ADDR:
      count = 8;
      break;
    case REG_PAC_ADDR:
    case REG_FDEV_ADDR:
    case REG_RDS_ADDR:
      break;
    default:
      ESP_LOGE(TAG, "write_reg_(0x%02X) invalid register address", addr);
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
      // ESP_LOGV(TAG, "write_reg_(0x%02X) = 0x%02X", addr + i, value);
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
  if (!this->state_.RDSEN || (this->rds_station_.empty() && this->rds_text_.empty())) {
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

  char buff[32] = {0};
  snprintf(buff, sizeof(buff), "%s %s %d.%d", this->state_.CID3 == CID3_QN8027 ? "QN8027" : "N/A",
           this->state_.CID1 == CID1_FM ? "FM" : "N/A", 1 + this->state_.CID4, 1 + this->state_.CID2);
  chip_id_ = buff;

  // reset
  this->state_.SWRST = 1;
  this->write_reg_(REG_SYSTEM_ADDR);
  delay_microseconds_safe(10000);  // TODO: add delay?
  // calibrate (TODO: single step with reset?)
  this->state_.RECAL = 1;
  this->write_reg_(REG_SYSTEM_ADDR);
  delay_microseconds_safe(10000);  // TODO: add delay?
  // write PA_TRGT first, it needs an IDLE cycle, IDLE is the default after RESET
  this->write_reg_(REG_PAC_ADDR);
  // update the rest of the system register, TXREQ == 1 will use PA_TRGT hopefully
  this->write_reg_(REG_SYSTEM_ADDR);
  // frequency lower bits and the rest of the settings
  this->write_reg_(REG_CH1_ADDR);
  this->write_reg_(REG_GPLT_ADDR);
  this->write_reg_(REG_XTL_ADDR);
  this->write_reg_(REG_VGA_ADDR);
  this->write_reg_(REG_FDEV_ADDR);
  this->write_reg_(REG_RDS_ADDR);

  this->publish_aud_pk();
  this->publish_fsm();
  this->publish_chip_id();
  this->publish_reg30();
  this->publish_frequency();
  this->publish_frequency_deviation();
  this->publish_mute();
  this->publish_mono();
  this->publish_tx_enable();
  this->publish_tx_pilot();
  this->publish_t1m_sel();
  this->publish_priv_en();
  this->publish_pre_emphasis();
  this->publish_xtal_source();
  this->publish_xtal_current();
  this->publish_xtal_frequency();
  this->publish_input_impedance();
  this->publish_input_gain();
  this->publish_digital_gain();
  this->publish_power_target();
  this->publish_rds_enable();
  this->publish_rds_frequency_deviation();
  this->publish_rds_station();
  this->publish_rds_text();

  this->set_interval(1000, [this]() { this->rds_update_(); });
}

void QN8027Component::dump_config() {
  ESP_LOGCONFIG(TAG, "QN8027:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "failed!");
  }
  ESP_LOGCONFIG(TAG, "  Chip: %s", this->chip_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Frequency: %.2f MHz", this->get_frequency());
  ESP_LOGCONFIG(TAG, "  RDS station: %s", this->rds_station_.c_str());
  ESP_LOGCONFIG(TAG, "  RDS text: %s", this->rds_text_.c_str());
  // TODO: ...and everything else...
  LOG_UPDATE_INTERVAL(this);
}

void QN8027Component::update() {
  if (this->read_reg_(REG_STATUS_ADDR)) {
    this->publish_aud_pk();
    this->publish_fsm();
    // reset aud_pk
    this->state_.TXPD_CLR ^= 1;
    this->write_reg_(REG_PAC_ADDR);
  } else {
    ESP_LOGE(TAG, "update cannot read the status register");
  }

  if (auto b = this->read_byte(REG_REG30)) {
    this->reg30_ = *b;
    this->publish_reg30();
  }
}

void QN8027Component::loop() {
  // TODO: check how often this runs, maybe do this in set_interval(1000, ... above
  if (this->read_reg_(REG_STATUS_ADDR)) {
    this->publish_fsm();
  }
}

// config

void QN8027Component::set_frequency(float value) {
  if (!(CH_FREQ_MIN <= value && value <= CH_FREQ_MAX)) {
    ESP_LOGE(TAG, "set_frequency(%.2f) invalid (%.2f - %.2f)", value, CH_FREQ_MIN, CH_FREQ_MAX);
    return;
  }

  int f = clamp((int) std::lround((value - 76) * 20), CH_FREQ_RAW_MIN, CH_FREQ_RAW_MAX);
  this->state_.CH_UPPER = (uint8_t) (f >> 8);
  this->state_.CH_LOWER = (uint8_t) (f & 0xff);
  this->write_reg_(REG_SYSTEM_ADDR);
  this->write_reg_(REG_CH1_ADDR);

  this->publish_frequency();
}

float QN8027Component::get_frequency() {
  uint16_t ch = ((uint16_t) this->state_.CH_UPPER << 8) | this->state_.CH_LOWER;
  return (float) ch / 20 + 76;
}

void QN8027Component::set_frequency_deviation(float value) {
  if (!(TX_FDEV_MIN <= value && value <= TX_FDEV_MAX)) {
    ESP_LOGE(TAG, "set_frequency_deviation(%.2f) invalid (%.2f - %.2f)", value, TX_FDEV_MIN, TX_FDEV_MAX);
    return;
  }

  this->state_.TX_FDEV = (uint8_t) clamp((int) std::lround(value / 0.58f), TX_FDEV_RAW_MIN, TX_FDEV_RAW_MAX);
  this->write_reg_(REG_FDEV_ADDR);

  this->publish_frequency_deviation();
}

float QN8027Component::get_frequency_deviation() { return 0.58f * this->state_.TX_FDEV; }

void QN8027Component::set_mute(bool value) {
  this->state_.MUTE = value ? 1 : 0;
  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_mute();
}

bool QN8027Component::get_mute() { return this->state_.MUTE == 1; }

void QN8027Component::set_mono(bool value) {
  this->state_.MONO = value ? 1 : 0;
  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_mono();
}

bool QN8027Component::get_mono() { return this->state_.MONO == 1; }

void QN8027Component::set_tx_enable(bool value) {
  this->state_.TXREQ = value ? 1 : 0;
  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_tx_enable();
}

bool QN8027Component::get_tx_enable() { return this->state_.TXREQ == 1; }

void QN8027Component::set_tx_pilot(uint8_t value) {
  if (!(GAIN_TXPLT_MIN <= value && value <= GAIN_TXPLT_MAX)) {
    ESP_LOGE(TAG, "tx_pilot invalid %d, must be 7-15", value);
    return;
  }

  this->state_.GAIN_TXPLT = value;
  this->write_reg_(REG_GPLT_ADDR);

  this->publish_tx_pilot();
}

uint8_t QN8027Component::get_tx_pilot() { return (uint8_t) this->state_.GAIN_TXPLT; }

void QN8027Component::set_t1m_sel(T1mSel value) {
  if (value >= T1mSel::LAST) {
    ESP_LOGE(TAG, "t1m_sel invalid %d, must be 58/59/60 or 0 (never)", (int) value);
    return;
  }

  this->state_.t1m_sel = (uint8_t) value;
  this->write_reg_(REG_GPLT_ADDR);

  this->publish_t1m_sel();
}

T1mSel QN8027Component::get_t1m_sel() { return (T1mSel) this->state_.t1m_sel; }

void QN8027Component::set_priv_en(bool value) {
  this->state_.priv_en = value ? 1 : 0;
  this->write_reg_(REG_GPLT_ADDR);

  this->publish_priv_en();
}

bool QN8027Component::get_priv_en() { return this->state_.priv_en == 1; }

void QN8027Component::set_pre_emphasis(PreEmphasis value) {
  if (value >= PreEmphasis::LAST) {
    ESP_LOGE(TAG, "pre-emphasis invalid %d, must be 50/75", (int) value);
    return;
  }

  this->state_.TC = (uint8_t) value;
  this->write_reg_(REG_GPLT_ADDR);

  this->publish_pre_emphasis();
}

PreEmphasis QN8027Component::get_pre_emphasis() { return (PreEmphasis) this->state_.TC; }

void QN8027Component::set_xtal_source(XtalSource value) {
  if (value >= XtalSource::LAST) {
    ESP_LOGE(TAG, "xtal source invalid %d", (int) value);
    return;
  }

  this->state_.XINJ = (uint8_t) value;
  this->write_reg_(REG_XTL_ADDR);

  this->publish_xtal_source();
}

XtalSource QN8027Component::get_xtal_source() { return (XtalSource) this->state_.XINJ; }

void QN8027Component::set_xtal_current(float value) {
  this->state_.XISEL = value >= XISEL_MAX ? 0b00111111 : (uint16_t) (value / 6.25f);
  if (this->state_.XINJ != (uint8_t) XtalSource::USE_CRYSTAL_ON_XTAL12) {
    ESP_LOGW(TAG, "xtal source should be %d", (int) XtalSource::USE_CRYSTAL_ON_XTAL12);
  }

  this->write_reg_(REG_XTL_ADDR);

  this->publish_xtal_current();
}

float QN8027Component::get_xtal_current() { return 6.25f * this->state_.XISEL; }

void QN8027Component::set_xtal_frequency(XtalFrequency value) {
  if (value >= XtalFrequency::LAST) {
    ESP_LOGE(TAG, "crystal frequency invalid %d, must be 12/24", (int) value);
    return;
  }

  this->state_.XSEL = (uint8_t) value;
  this->write_reg_(REG_VGA_ADDR);

  this->publish_xtal_frequency();
}

XtalFrequency QN8027Component::get_xtal_frequency() { return (XtalFrequency) this->state_.XSEL; }

void QN8027Component::set_input_impedance(InputImpedance value) {
  if (value >= InputImpedance::LAST) {
    ESP_LOGE(TAG, "input impedance invalid %d, must be 5/10/20/40", (int) value);
    return;
  }

  this->state_.RIN = (uint8_t) value;
  this->write_reg_(REG_VGA_ADDR);

  this->publish_input_impedance();
}

InputImpedance QN8027Component::get_input_impedance() { return (InputImpedance) this->state_.RIN; }

void QN8027Component::set_input_gain(uint8_t value) {
  if (value > GVGA_MAX) {
    ESP_LOGE(TAG, "input gain invalid %d, max %ddB", value, GVGA_MAX);
    return;
  }

  this->state_.GVGA = value;
  this->write_reg_(REG_VGA_ADDR);

  this->publish_input_gain();
}

uint8_t QN8027Component::get_input_gain() { return this->state_.GVGA; }

void QN8027Component::set_digital_gain(uint8_t value) {
  if (value > GDB_MAX) {
    ESP_LOGE(TAG, "digital gain invalid %d, max %ddB", value, GDB_MAX);
    return;
  }

  this->state_.GDB = value;
  this->write_reg_(REG_VGA_ADDR);

  this->publish_digital_gain();
}

uint8_t QN8027Component::get_digital_gain() { return this->state_.GDB; }

void QN8027Component::set_power_target(float value) {
  if (!(PA_TRGT_MIN <= value && value <= PA_TRGT_MAX)) {
    ESP_LOGE(TAG, "set_power_target(%.2f) invalid (%.2f and %.2f)", value, PA_TRGT_MIN, PA_TRGT_MAX);
    return;
  }

  this->state_.PA_TRGT = (uint8_t) clamp((int) std::lround((value - 71) / 0.62f), PA_TRGT_RAW_MIN, PA_TRGT_RAW_MAX);
  this->write_reg_(REG_PAC_ADDR);

  this->publish_power_target();

  // From the datasheet:
  //
  // PA output power setting will not efficient immediately, it need to enter IDLE mode and re-enter TX mode,
  // or when the frequency changed the PA output power setting will take effect

  if (this->state_.FSM == FSM_STATUS_TRANSMIT) {
    uint8_t txreq = this->state_.TXREQ;
    this->state_.TXREQ = 0;
    this->write_reg_(REG_SYSTEM_ADDR);
    int tries = 10;
    while (this->read_reg_(REG_STATUS_ADDR)) {
      if (this->state_.FSM == FSM_STATUS_IDLE || tries-- <= 0)
        break;
      delay_microseconds_safe(1);
    }
    if (tries <= 0) {
      ESP_LOGE(TAG, "set_power_target(%.2f) could not reach IDLE status in time", value);
    }
    if (txreq) {
      this->state_.TXREQ = txreq;
      this->write_reg_(REG_SYSTEM_ADDR);
    }
  }
}

float QN8027Component::get_power_target() { return 0.62f * this->state_.PA_TRGT + 71; }

void QN8027Component::set_rds_enable(bool value) {
  this->state_.RDSEN = value ? 1 : 0;
  this->write_reg_(REG_RDS_ADDR);

  this->publish_rds_enable();
}

bool QN8027Component::get_rds_enable() { return this->state_.RDSEN == 1; }

void QN8027Component::set_rds_frequency_deviation(float value) {
  if (!(RDSFDEV_MIN <= value && value <= RDSFDEV_MAX)) {
    ESP_LOGE(TAG, "set_rds_frequency_deviation(%.2f) invalid (%.2f and %.2f)", value, RDSFDEV_MIN, RDSFDEV_MAX);
    return;
  }

  this->state_.RDSFDEV = (uint8_t) clamp((int) std::lround(value / 0.35f), RDSFDEV_RAW_MIN, RDSFDEV_RAW_MAX);
  this->write_reg_(REG_RDS_ADDR);

  this->publish_rds_frequency_deviation();
}

float QN8027Component::get_rds_frequency_deviation() { return 0.35f * this->state_.RDSFDEV; }

void QN8027Component::set_rds_station(const std::string &value) {
  this->rds_station_ = value;
  this->rds_station_pos_ = 0;
  if (this->rds_station_.size() > RDS_STATION_MAX_SIZE) {
    ESP_LOGW(TAG, "rds station too long '%s' (max %d characters)", value.c_str(), RDS_STATION_MAX_SIZE);
    this->rds_station_.resize(RDS_STATION_MAX_SIZE);
  }

  this->publish_rds_station();
}

void QN8027Component::set_rds_text(const std::string &value) {
  this->rds_text_ = value;
  this->rds_text_pos_ = 0;
  if (this->rds_text_.size() > RDS_TEXT_MAX_SIZE) {
    ESP_LOGW(TAG, "rds text to long '%s' (max %d characters)", value.c_str(), RDS_TEXT_MAX_SIZE);
    this->rds_text_.resize(RDS_TEXT_MAX_SIZE);
  }

  this->publish_rds_text();
}

// publish

void QN8027Component::publish_aud_pk() { this->publish(this->aud_pk_sensor_, 45.0f * this->state_.aud_pk); }

void QN8027Component::publish_fsm() {
  const char *value = nullptr;
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
  this->publish(this->fsm_text_sensor_, value);
}

void QN8027Component::publish_chip_id() { this->publish(this->chip_id_text_sensor_, this->chip_id_); }

void QN8027Component::publish_reg30() { this->publish(this->reg30_sensor_, (float) this->reg30_); }

void QN8027Component::publish_frequency() { this->publish(this->frequency_number_, this->get_frequency()); }

void QN8027Component::publish_frequency_deviation() {
  this->publish(this->frequency_deviation_number_, this->get_frequency_deviation());
}

void QN8027Component::publish_mute() { this->publish(this->mute_switch_, this->get_mute()); }

void QN8027Component::publish_mono() { this->publish(this->mono_switch_, this->get_mono()); }

void QN8027Component::publish_tx_enable() { this->publish(this->tx_enable_switch_, this->get_tx_enable()); }

void QN8027Component::publish_tx_pilot() { this->publish(this->tx_pilot_number_, this->get_tx_pilot()); }

void QN8027Component::publish_t1m_sel() { this->publish(this->t1m_sel_select_, (size_t) this->get_t1m_sel()); }

void QN8027Component::publish_priv_en() { this->publish(this->priv_en_switch_, this->get_priv_en()); }

void QN8027Component::publish_pre_emphasis() {
  this->publish(this->pre_emphasis_select_, (size_t) this->get_pre_emphasis());
}

void QN8027Component::publish_xtal_source() {
  this->publish(this->xtal_source_select_, (size_t) this->get_xtal_source());
}

void QN8027Component::publish_xtal_current() { this->publish(this->xtal_current_number_, this->get_xtal_current()); }

void QN8027Component::publish_xtal_frequency() {
  this->publish(this->xtal_frequency_select_, (size_t) this->get_xtal_frequency());
}

void QN8027Component::publish_input_impedance() {
  this->publish(this->input_impedance_select_, (size_t) this->get_input_impedance());
}

void QN8027Component::publish_input_gain() { this->publish(this->input_gain_number_, (float) this->get_input_gain()); }

void QN8027Component::publish_digital_gain() {
  this->publish(this->digital_gain_number_, (float) this->get_digital_gain());
}

void QN8027Component::publish_power_target() {
  this->publish(this->power_target_number_, (float) this->get_power_target());
}

void QN8027Component::publish_rds_enable() { this->publish(this->rds_enable_switch_, this->get_rds_enable()); }

void QN8027Component::publish_rds_frequency_deviation() {
  this->publish(this->rds_frequency_deviation_number_, this->get_rds_frequency_deviation());
}

void QN8027Component::publish_rds_station() { this->publish(this->rds_station_text_, this->rds_station_); }

void QN8027Component::publish_rds_text() { this->publish(this->rds_text_text_, this->rds_text_); }

void QN8027Component::publish(text_sensor::TextSensor *s, const std::string &state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void QN8027Component::publish(sensor::Sensor *s, float state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void QN8027Component::publish(number::Number *n, float state) {
  if (n != nullptr) {
    if (!n->has_state() || n->state != state) {
      n->publish_state(state);
    }
  }
}

void QN8027Component::publish(switch_::Switch *s, bool state) {
  if (s != nullptr) {
    if (s->state != state) {  // ?
      s->publish_state(state);
    }
  }
}

void QN8027Component::publish(select::Select *s, size_t index) {
  if (s != nullptr) {
    if (auto state = s->at(index)) {
      if (!s->has_state() || s->state != *state) {
        s->publish_state(*state);
      }
    }
  }
}

void QN8027Component::publish(text::Text *t, const std::string &state) {
  if (t != nullptr) {
    if (!t->has_state() || t->state != state) {
      t->publish_state(state);
    }
  }
}

}  // namespace qn8027
}  // namespace esphome
