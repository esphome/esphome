#include "si4713.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cmath>

namespace esphome {
namespace si4713 {

// TODO: std::clamp isn't here yet
#define clamp(v, lo, hi) std::max(std::min(v, hi), lo)

static const char *const TAG = "si4713";

Si4713Component::Si4713Component() {
  this->reset_pin_ = nullptr;
  this->reset_ = false;
  // memset(&this->state_, 0, sizeof(this->state_));
  this->rds_station_pos_ = 0;
  this->rds_text_pos_ = 0;
  //  this->state_. = ;
  // our defaults
  //  this->state_. = ;     // start with tx enabled
}
/*
void Si4713Component::write_reg_(uint8_t addr) {
  switch (addr) {
    case 0x00: // REG_..
      break;
    default:
      ESP_LOGE(TAG, "write_reg_(0x%02X) invalid register address", addr);
      return;
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

bool Si4713Component::read_reg_(uint8_t addr) {
  switch (addr) {
    case 0x00: // REG_..
      break;
    default:
      ESP_LOGE(TAG, "read_reg_(0x%02X) trying to read invalid register", addr);
      return false;
  }

  if (auto b = this->read_byte(addr)) {
    this->regs_[addr] = *b;
    return true;
  }

  ESP_LOGE(TAG, "read_reg_(0x%02X) cannot read register", addr);
  return false;
}
*/
bool Si4713Component::send_cmd(const void *cmd, size_t cmd_size, void *res, size_t res_size) {
  const uint8_t *buff = (const uint8_t *) cmd;

  if (!this->reset_) {
    if (this->get_component_state() & COMPONENT_STATE_LOOP) {
      ESP_LOGE(TAG, "send_cmd(0x%02X, %d) device was not reset", buff[0], cmd_size);
    }
    return false;
  }

  i2c::ErrorCode err = this->write(buff, cmd_size);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "send_cmd(0x%02X, %d) write error", buff[0], cmd_size);
    this->mark_failed();
    return false;
  }

  uint8_t status = 0;
  while (!(status & SI4710_STATUS_CTS)) {
    err = this->read(&status, 1);  // TODO: read res_size into res here?
    if (err != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "send_cmd(0x%02X, %d) read status error", buff[0], cmd_size);
      this->mark_failed();
      return false;
    }
  }

  if (res != nullptr) {
    //((uint8_t*) res)[0] = status;
    err = this->read((uint8_t *) res, res_size);
    if (err != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "send_cmd(0x%02X, %d) read response error", buff[0], cmd_size);
      this->mark_failed();
      return false;
    }
  }

  return true;
}

bool Si4713Component::power_up() {
  // Note: In FMTX component 1.0 and 2.0, a reset is required when the system controller writes a command other than
  // POWER_UP when in powerdown mode
  this->reset_pin_->digital_write(true);
  delay_microseconds_safe(10);
  this->reset_pin_->digital_write(false);
  delay_microseconds_safe(10);
  this->reset_pin_->digital_write(true);

  CmdPowerUp cmd;
  cmd.FUNC = 2;
  cmd.XOSCEN = 1;     // TODO: external oscillator
  cmd.OPMODE = 0x50;  // TODO: digital
  cmd.GPO2OEN = 0;    // TODO: GPIO2 enable
  return this->send_cmd(cmd);
}

bool Si4713Component::power_down() { return this->send_cmd(CmdPowerDown()); }

bool Si4713Component::detect_chip_id() {
  ResGetRev res;
  if (!this->send_cmd(CmdGetRev(), res)) {
    return false;
  }

  char buff[32] = {0};
  snprintf(buff, sizeof(buff), "Si47%02d Rev %d", res.PN, res.CHIPREV);
  this->chip_id_ = buff;

  // TODO: support all transmitters 10/11/12/13/20/21

  if (res.PN != 10 && res.PN != 11 && res.PN != 12 && res.PN != 13) {
    ESP_LOGE(TAG, "Si47%02d is not supported", res.PN);
    this->mark_failed();
    return false;
  }

  return true;
}

bool Si4713Component::tune_power(uint8_t power, uint8_t antcap) {
  if (!this->send_cmd(CmdTxTunePower(power, antcap))) {
    return false;
  }
  return this->tune_ready();
}

bool Si4713Component::tune_freq(uint16_t freq) {
  if (!this->send_cmd(CmdTxTuneFreq(freq))) {
    return false;
  }
  return this->tune_ready();
}

bool Si4713Component::tune_ready() {
  ResGetIntStatus res;
  while (res.CTS != 1 || res.STCINT != 1) {
    if (!this->send_cmd(CmdGetIntStatus(), res)) {
      return false;
    }
  }
  return true;
}

void Si4713Component::rds_update_() {}

// overrides

void Si4713Component::setup() {
  if (this->reset_pin_ == nullptr) {
    ESP_LOGE(TAG, "setup cannot reset device, reset pin is not set");
    this->mark_failed();
    return;
  }

  // reset

  this->reset_ = true;
  this->reset_pin_->setup();

  if (!this->power_up()) {
    return;
  }

  if (!this->detect_chip_id()) {
    return;
  }

  this->set_prop(PropRefClkFreq(32768));
  this->set_prop(PropTxPreEmphasis());
  /*
    // configuration! see page 254
    setProperty(SI4713_PROP_REFCLK_FREQ, 32768); // crystal is 32.768
    setProperty(SI4713_PROP_TX_PREEMPHASIS, 0);  // 74uS pre-emph (USA std)
    setProperty(SI4713_PROP_TX_ACOMP_GAIN, 10);  // max gain?
    // setProperty(SI4713_PROP_TX_ACOMP_ENABLE, 0x02); // turn on limiter, but no
    // dynamic ranging
    setProperty(SI4713_PROP_TX_ACOMP_ENABLE, 0x0); // turn on limiter and AGC
  */

  // TODO: set properties

  this->tune_power(115);
  this->tune_freq(8750);

  //

  this->publish_frequency();
  this->publish_mute();
  this->publish_mono();
  this->publish_rds_enable();
  this->publish_rds_station();
  this->publish_rds_text();

  this->set_interval(1000, [this]() { this->rds_update_(); });
}

void Si4713Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Si4713:");
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

void Si4713Component::update() {
  /*
    if (this->read_reg_(REG_)) {
      this->publish_();
    } else {
      ESP_LOGE(TAG, "update cannot read the status register");
    }
  */
}

void Si4713Component::loop() {}

// config

void Si4713Component::set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }

void Si4713Component::set_frequency(float value) {
  if (!(CH_FREQ_MIN <= value && value <= CH_FREQ_MAX)) {
    ESP_LOGE(TAG, "set_frequency(%.2f) invalid (%.2f - %.2f)", value, CH_FREQ_MIN, CH_FREQ_MAX);
    return;
  }

  //  int f = clamp((int) std::lround((value - 76) * 20), CH_FREQ_RAW_MIN, CH_FREQ_RAW_MAX);
  //  this->state_.CH_UPPER = (uint8_t) (f >> 8);
  //  this->state_.CH_LOWER = (uint8_t) (f & 0xff);
  //  this->write_reg_(REG_SYSTEM_ADDR);
  //  this->write_reg_(REG_CH1_ADDR);

  this->publish_frequency();
}

float Si4713Component::get_frequency() {
  //  uint16_t ch = ((uint16_t) this->state_.CH_UPPER << 8) | this->state_.CH_LOWER;
  //  return (float) ch / 20 + 76;
  return 0;
}

void Si4713Component::set_mute(bool value) {
  //  this->state_.MUTE = value ? 1 : 0;
  //  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_mute();
}

bool Si4713Component::get_mute() {
  //  return this->state_.MUTE == 1;
  return false;
}

void Si4713Component::set_mono(bool value) {
  //  this->state_.MONO = value ? 1 : 0;
  //  this->write_reg_(REG_SYSTEM_ADDR);

  this->publish_mono();
}

bool Si4713Component::get_mono() {
  //  return this->state_.MONO == 1;
  return false;
}

void Si4713Component::set_rds_enable(bool value) {
  // TODO

  this->publish_rds_enable();
}

bool Si4713Component::get_rds_enable() {
  // return this->state_.RDSEN == 1;
  return false;
}

void Si4713Component::set_rds_station(const std::string &value) {
  this->rds_station_ = value;
  this->rds_station_pos_ = 0;
  if (this->rds_station_.size() > RDS_STATION_MAX) {
    ESP_LOGW(TAG, "rds station too long '%s' (max %d characters)", value.c_str(), RDS_STATION_MAX);
    this->rds_station_.resize(RDS_STATION_MAX);
  }

  this->publish_rds_station();
}

void Si4713Component::set_rds_text(const std::string &value) {
  this->rds_text_ = value;
  this->rds_text_pos_ = 0;
  if (this->rds_text_.size() > RDS_TEXT_MAX) {
    ESP_LOGW(TAG, "rds text to long '%s' (max %d characters)", value.c_str(), RDS_TEXT_MAX);
    this->rds_text_.resize(RDS_TEXT_MAX);
  }

  this->publish_rds_text();
}

// publish

void Si4713Component::publish_() {
  //  this->publish(this->_sensor_, this->state_.);
}

void Si4713Component::publish_chip_id() { this->publish(this->chip_id_text_sensor_, this->chip_id_); }

void Si4713Component::publish_frequency() { this->publish(this->frequency_number_, this->get_frequency()); }

void Si4713Component::publish_mute() { this->publish(this->mute_switch_, this->get_mute()); }

void Si4713Component::publish_mono() { this->publish(this->mono_switch_, this->get_mono()); }

void Si4713Component::publish_rds_enable() { this->publish(this->rds_enable_switch_, this->get_rds_enable()); }

void Si4713Component::publish_rds_station() { this->publish(this->rds_station_text_, this->rds_station_); }

void Si4713Component::publish_rds_text() { this->publish(this->rds_text_text_, this->rds_text_); }

void Si4713Component::publish(text_sensor::TextSensor *s, const std::string &state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void Si4713Component::publish(sensor::Sensor *s, float state) {
  if (s != nullptr) {
    if (!s->has_state() || s->state != state) {
      s->publish_state(state);
    }
  }
}

void Si4713Component::publish(number::Number *n, float state) {
  if (n != nullptr) {
    if (!n->has_state() || n->state != state) {
      n->publish_state(state);
    }
  }
}

void Si4713Component::publish(switch_::Switch *s, bool state) {
  if (s != nullptr) {
    if (s->state != state) {  // ?
      s->publish_state(state);
    }
  }
}

void Si4713Component::publish(select::Select *s, size_t index) {
  if (s != nullptr) {
    if (auto state = s->at(index)) {
      if (!s->has_state() || s->state != *state) {
        s->publish_state(*state);
      }
    }
  }
}

void Si4713Component::publish(text::Text *t, const std::string &state) {
  if (t != nullptr) {
    if (!t->has_state() || t->state != state) {
      t->publish_state(state);
    }
  }
}

}  // namespace si4713
}  // namespace esphome
