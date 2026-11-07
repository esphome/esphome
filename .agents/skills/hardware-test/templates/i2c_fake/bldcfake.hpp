#pragma once
// Fake M5Unit-BLDC units on an in-firmware I2C bus with simple motor physics.
#include <cmath>
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace bldcfake {
static const char *const TAG = "bldcfake";
struct Unit {
  uint8_t addr;
  bool stuck;  // the motor never spins down
  uint8_t mode{0}, dir_reg{0}, dir_latched{0}, model{0}, poles{0};
  uint16_t pwm{0};
  float set_rpm{0}, rpm{0};
  int32_t pid[3]{0, 0, 0};
  bool started{false};  // last_ms is seeded on the first transfer, not at static init
  uint32_t last_ms{0}, saves{0}, writes{0};
  uint32_t bad_writes{0}, bad_reads{0};  // unknown register or wrong length, NACKed
  uint8_t reg{0};
  void step() {
    uint32_t now = (esphome::millis) ();
    if (!this->started) {
      this->started = true;
      this->last_ms = now;
    }
    float dt = (now - this->last_ms) / 1000.0f;
    this->last_ms = now;
    float target = this->mode ? this->set_rpm : this->pwm * 2.0f;
    float d = 4000.0f * dt;
    if (fabsf(target - this->rpm) <= d)
      this->rpm = target;
    else
      this->rpm += target > this->rpm ? d : -d;
    if (this->stuck && target == 0 && this->rpm < 300)
      this->rpm = 300;
    if (this->rpm == 0 && this->dir_latched != this->dir_reg) {
      this->dir_latched = this->dir_reg;
      ESP_LOGI(TAG, "0x%02X direction latched %u", this->addr, this->dir_latched);
    }
  }
};
// Register width in bytes, 0 when the fake does not take that direction for it
inline size_t write_width(uint8_t reg) {
  switch (reg) {
    case 0x00:
    case 0x60:
    case 0xF0:
      return 1;
    case 0x10:
    case 0x70:
      return 2;
    case 0x40:
      return 4;
    case 0x50:
      return 12;
    default:
      return 0;
  }
}
inline size_t read_width(uint8_t reg) {
  switch (reg) {
    case 0x00:
    case 0x60:
    case 0x80:
      return 1;
    case 0x10:
      return 2;
    case 0x20:
    case 0x30:
    case 0x40:
      return 4;
    default:
      return 0;  // PID and motor config are not modelled, the driver only writes them
  }
}
struct Bus final : public esphome::i2c::I2CBus {
  Unit units[3] = {{0x65, false}, {0x66, false}, {0x67, true}};  // C never stops
  Bus() {
    // Unit A was left running backward at 800 PWM before this boot
    this->units[0].pwm = 800;
    this->units[0].rpm = 1600;
    this->units[0].dir_reg = this->units[0].dir_latched = 1;
  }
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    Unit *u = nullptr;
    for (auto &x : units)
      if (x.addr == address)
        u = &x;
    if (u == nullptr)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    u->step();
    if (size >= 1) {
      u->reg = data[0];
      const uint8_t *p = data + 1;
      size_t n = size - 1;
      if (n > 0 && n != write_width(u->reg)) {
        u->bad_writes++;
        ESP_LOGW(TAG, "0x%02X W reg 0x%02X %u bytes BAD", u->addr, u->reg, (unsigned) n);
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      if (n > 0) {
        u->writes++;
        switch (u->reg) {
          case 0x00:
            u->mode = p[0];
            break;
          case 0x10:
            u->pwm = p[0] | (p[1] << 8);
            break;
          case 0x40:
            memcpy(&u->set_rpm, p, 4);
            break;
          case 0x50:
            memcpy(u->pid, p, 12);
            break;
          case 0x60:
            u->dir_reg = p[0];
            break;
          case 0x70:
            u->model = p[0];
            u->poles = p[1];
            break;
          case 0xF0:
            u->saves++;
            break;
        }
        ESP_LOGI(TAG, "0x%02X W reg 0x%02X %s", u->addr, u->reg,
                 u->reg == 0x10   ? "pwm"
                 : u->reg == 0x40 ? "set_rpm"
                 : u->reg == 0x60 ? "direction"
                                  : "");
        if (u->reg == 0x10)
          ESP_LOGI(TAG, "0x%02X   pwm=%u", u->addr, u->pwm);
        if (u->reg == 0x40)
          ESP_LOGI(TAG, "0x%02X   set_rpm=%.1f", u->addr, u->set_rpm);
        if (u->reg == 0x60)
          ESP_LOGI(TAG, "0x%02X   direction=%u (rpm now %.0f)", u->addr, u->dir_reg, u->rpm);
      }
    }
    if (out_size && (size == 0 || out_size != read_width(u->reg))) {
      // a read without a register, of one the fake does not model, or of the wrong length
      memset(out, 0, out_size);
      u->bad_reads++;
      ESP_LOGW(TAG, "0x%02X R reg 0x%02X %u bytes BAD", u->addr, u->reg, (unsigned) out_size);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (out_size) {
      float freq = u->rpm * u->poles / 60.0f;
      uint8_t st = u->rpm > 0 ? 1 : 0;
      switch (u->reg) {
        case 0x00:
          out[0] = u->mode;
          break;
        case 0x10:
          out[0] = u->pwm & 0xFF;
          out[1] = u->pwm >> 8;
          break;
        case 0x20:
          memcpy(out, &u->rpm, 4);
          break;
        case 0x30:
          memcpy(out, &freq, 4);
          break;
        case 0x40:
          memcpy(out, &u->set_rpm, 4);
          break;
        case 0x60:
          out[0] = u->dir_reg;
          break;
        case 0x80:
          out[0] = st;
          break;
      }
    }
    return esphome::i2c::ERROR_OK;
  }
  void report() {
    for (auto &u : units)
      ESP_LOGI(TAG,
               "0x%02X mode=%u model=%u poles=%u pid=%d/%d/%d pwm=%u set_rpm=%.0f rpm=%.0f dir=%u/%u saves=%u "
               "bad_writes=%u bad_reads=%u",
               u.addr, u.mode, u.model, u.poles, (int) u.pid[0], (int) u.pid[1], (int) u.pid[2], u.pwm, u.set_rpm,
               u.rpm, u.dir_reg, u.dir_latched, (unsigned) u.saves, (unsigned) u.bad_writes, (unsigned) u.bad_reads);
  }
};
inline Bus bus;
}  // namespace bldcfake
