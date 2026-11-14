// Register and PATABLE snapshots plus the CHIP_RDYn sleep check for the cc1101 sleep tests in cc1101-sleep.yaml.
#pragma once
#include "esphome/components/cc1101/cc1101.h"
#include "driver/gpio.h"
#include <cstring>

namespace cct {
using esphome::cc1101::CC1101Component;
static constexpr gpio_num_t MISO = GPIO_NUM_13;  // the SPI MISO pin in cc1101-sleep.yaml
static constexpr uint8_t NREG = 0x2F;            // IOCFG2..TEST0

struct Snap {
  uint8_t reg[NREG];
  uint8_t pa[8];
  uint8_t marc;
};

// Chip ready flag: with CSn low, SO stays high until the crystal runs (SLEEP), low at once otherwise
inline bool cs_low_so_high(CC1101Component *c) {
  c->enable();
  bool high = gpio_get_level(MISO);
  delayMicroseconds(300);
  c->disable();
  return high;
}
inline uint8_t rd(CC1101Component *c, uint8_t a) {
  c->enable();
  c->write_byte(a | 0x80);
  uint8_t v = c->read_byte();
  c->disable();
  return v;
}
inline uint8_t status(CC1101Component *c, uint8_t a) { return rd(c, a | 0x40); }
inline void snap(CC1101Component *c, Snap &s) {
  for (uint8_t i = 0; i < NREG; i++)
    s.reg[i] = rd(c, i);
  c->enable();
  c->write_byte(0x3E | 0xC0);
  for (auto &b : s.pa)
    b = c->read_byte();
  c->disable();
  s.marc = status(c, 0x35) & 0x1F;
}
inline void write_pa(CC1101Component *c, const uint8_t *pa) {
  c->enable();
  c->write_byte(0x3E | 0x40);
  for (int i = 0; i < 8; i++)
    c->write_byte(pa[i]);
  c->disable();
}
inline int diff(const Snap &a, const Snap &b, const char *what) {
  int n = 0;
  for (uint8_t i = 0; i < NREG; i++)
    // FSCAL3..FSCAL1 hold calibration results, FSTEST and AGCTEST are chip owned test registers
    if (i != 0x23 && i != 0x24 && i != 0x25 && i != 0x29 && i != 0x2B && a.reg[i] != b.reg[i]) {
      ESP_LOGW("cct", "%s reg 0x%02X: %02X vs %02X", what, i, a.reg[i], b.reg[i]);
      n++;
    }
  for (int i = 0; i < 8; i++)
    if (a.pa[i] != b.pa[i]) {
      ESP_LOGW("cct", "%s pa[%d]: %02X vs %02X", what, i, a.pa[i], b.pa[i]);
      n++;
    }
  return n;
}
inline void hexpa(const char *tag, const uint8_t *pa) {
  ESP_LOGI("cct", "%s PA %02X %02X %02X %02X %02X %02X %02X %02X", tag, pa[0], pa[1], pa[2], pa[3], pa[4], pa[5], pa[6],
           pa[7]);
}
}  // namespace cct
