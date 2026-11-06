#pragma once
// Fake CCS811 on an in-firmware I2C bus: answers setup and update reads, checks every ENV_DATA write.
#include <cmath>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

namespace ccsfake {
static const char *const TAG = "ccsfake";

struct Bus final : public esphome::i2c::I2CBus {
  float hum{NAN}, temp{NAN};  // inputs of the update being checked
  uint32_t checked{0}, bad{0}, writes{0};
  bool quiet{false};

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x5A)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 0)
      return esphome::i2c::ERROR_OK;
    uint8_t reg = data[0];
    if (reg == 0x05 && size == 5) {
      this->check_(data + 1);
    } else if (reg == 0x05) {
      this->writes++;
      this->checked++;
      this->bad++;
      ESP_LOGI(TAG, "ENV write of %u bytes, want 4 BAD", (unsigned) (size - 1));
    }
    static const uint8_t ALG[4] = {0x01, 0xC2, 0x00, 0x0C};  // 450 ppm, 12 ppb
    for (size_t i = 0; i < out_size; i++) {
      switch (reg) {
        case 0x20:
          out[i] = 0x81;
          break;  // HW_ID
        case 0x00:
          out[i] = 0x98;
          break;  // FW_MODE | APP_VALID | DATA_READY
        case 0x02:
          out[i] = i < 4 ? ALG[i] : 0;
          break;
        case 0x11:
          out[i] = i == 0 ? 0xAB : 0xCD;
          break;  // baseline
        case 0x21:
          out[i] = 0x12;
          break;
        case 0x23:
          out[i] = i == 0 ? 0x10 : 0x00;
          break;
        case 0x24:
          out[i] = i == 0 ? 0x20 : 0x00;
          break;  // app 2.0.0
        default:
          out[i] = 0;
      }
    }
    return esphome::i2c::ERROR_OK;
  }

  static long want_steps(float v) { return lround((double) v * 512.0); }

  void check_(const uint8_t *d) {
    this->writes++;
    float h = this->hum, t = this->temp;
    if (std::isnan(h) || h < 0 || h > 100)
      h = 50;
    if (std::isnan(t) || t < -25 || t > 50)
      t = 25;
    t += 25;
    long wh = want_steps(h), wt = want_steps(t);
    unsigned gh = (d[0] << 8) | d[1], gt = (d[2] << 8) | d[3];
    bool ok = gh == (unsigned) wh && gt == (unsigned) wt;
    this->checked++;
    if (!ok)
      this->bad++;
    if (!this->quiet || !ok)
      ESP_LOGI(TAG, "ENV in h=%.4f t=%.4f sent %04X %04X want %04lX %04lX %s", this->hum, this->temp, gh, gt, wh, wt,
               ok ? "OK" : "BAD");
  }
};
inline Bus bus;

// Set the compensation inputs and run one update; an update without an ENV_DATA write counts as bad.
template<typename C, typename S> void run(C *ccs, S *hs, S *ts, float h, float t) {
  hs->state = h;
  ts->state = t;
  bus.hum = h;
  bus.temp = t;
  uint32_t w0 = bus.writes;
  ccs->update();
  if (bus.writes == w0) {
    bus.checked++;
    bus.bad++;
    ESP_LOGI(TAG, "ENV in h=%.4f t=%.4f no ENV_DATA write BAD", h, t);
  }
}
}  // namespace ccsfake
