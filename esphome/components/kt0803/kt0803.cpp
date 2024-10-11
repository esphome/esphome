#include "kt0803.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cmath>

namespace esphome {
namespace kt0803 {

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

  for (size_t addr = 0; addr < sizeof(this->state_); addr++) {
    if (addr != 0x0F && this->check_reg_(addr)) {
      this->write_reg_(addr);
    }
  }

  this->publish_pw_ok();
  this->publish_slncid();
  this->publish_frequency();
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
  if (this->chip_id_ != ChipId::KT0803) {
    ch |= (uint16_t) this->state_.CHSEL0 << 0;
  }
  return (float) ch / 20;
}

// publish

void KT0803Component::publish_pw_ok() { this->publish(this->pw_ok_binary_sensor_, this->state_.PW_OK == 1); }

void KT0803Component::publish_slncid() { this->publish(this->slncid_binary_sensor_, this->state_.SLNCID == 1); }

void KT0803Component::publish_frequency() { this->publish(this->frequency_number_, this->get_frequency()); }

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
