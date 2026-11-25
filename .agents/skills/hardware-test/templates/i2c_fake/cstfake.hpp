#pragma once
// Fake CST816 at 0x15 on an in-firmware I2C bus: plays scripted 13 byte status frames, one step per stage.
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

namespace cstfake {
static const char *const TAG = "cstfake";

struct Step {
  const char *name;
  uint8_t count;  // REG_TOUCH_NUM byte; 0xFF selects the all 0xFF idle frame
  uint16_t x, y;
};
// Every stimulus step is followed by an idle (count 0) step so a release can be attributed to it.
static const Step STEPS[] = {
    {"idle_zero", 0, 0, 0},
    {"idle_ff", 0xFF, 0, 0},
    {"rel", 0, 0, 0},
    {"one_finger", 1, 100, 150},
    {"one_finger_move", 1, 120, 160},
    {"rel", 0, 0, 0},
    {"two_fingers", 2, 50, 60},
    {"rel", 0, 0, 0},
    {"count3", 3, 70, 80},
    {"rel", 0, 0, 0},
    {"count5", 5, 90, 100},
    {"rel", 0, 0, 0},
    {"sentinel_x", 1, 0xFFF, 100},
    {"rel", 0, 0, 0},
    {"sentinel_y", 1, 100, 0xFFF},
    {"rel", 0, 0, 0},
    {"out_of_range", 1, 1000, 2000},
    {"rel", 0, 0, 0},
    {"one_finger_again", 1, 200, 300},
    {"rel", 0, 0, 0},
};
static constexpr size_t NSTEPS = sizeof(STEPS) / sizeof(STEPS[0]);

struct Bus final : public esphome::i2c::I2CBus {
  size_t step{0};
  uint8_t reg{0};
  uint32_t status_reads{0}, other_reads{0}, writes{0}, touches[NSTEPS]{}, releases[NSTEPS]{}, updates[NSTEPS]{};
  int16_t tx[NSTEPS], ty[NSTEPS], rx[NSTEPS], ry[NSTEPS];
  Bus() {
    for (size_t i = 0; i < NSTEPS; i++)
      tx[i] = ty[i] = rx[i] = ry[i] = -1;
  }

  void frame_(uint8_t *f) const {
    const Step &s = STEPS[this->step];
    if (s.count == 0xFF) {
      memset(f, 0xFF, 13);
      return;
    }
    memset(f, 0, 13);
    f[2] = s.count;
    if (s.count) {
      f[3] = (s.x >> 8) & 0x0F;  // event flag bits stay 0 (press down)
      f[4] = s.x & 0xFF;
      f[5] = (s.y >> 8) & 0x0F;
      f[6] = s.y & 0xFF;
    }
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x15)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size > 0) {
      this->reg = data[0];
      if (size > 1) {
        this->writes++;
        ESP_LOGI(TAG, "write reg 0x%02X = 0x%02X", data[0], data[1]);
      }
    }
    if (out_size == 0)
      return esphome::i2c::ERROR_OK;
    uint8_t f[256];
    memset(f, 0, sizeof f);
    if (this->reg == 0x00) {
      this->frame_(f);
      this->status_reads++;
    } else {
      this->other_reads++;
      if (this->reg == 0xA7)
        f[0] = 0xB5;  // CST816T
    }
    memcpy(out, f, out_size > 13 && this->reg == 0 ? 13 : (out_size > 256 ? 256 : out_size));
    return esphome::i2c::ERROR_OK;
  }
};
inline Bus bus;
}  // namespace cstfake
