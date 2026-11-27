#pragma once
// Fake CCS811 on an in-firmware I2C bus: answers setup and update reads, checks every ENV_DATA write.
// Any other write or read the driver does not make, or one of the wrong length, gets a NACK and is counted.
#include <cmath>
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

namespace ccsfake {
static const char *const TAG = "ccsfake";

struct Bus final : public esphome::i2c::I2CBus {
  float hum{NAN}, temp{NAN};  // inputs of the update being checked
  uint32_t checked{0}, bad{0}, writes{0}, bad_reads{0}, bad_writes{0};
  bool quiet{false};

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x5A)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 0 && out_size == 0)
      return esphome::i2c::ERROR_OK;
    if (size == 0) {
      // a read without a register the fake does not model: fail it visibly
      memset(out, 0, out_size);
      this->bad_reads++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    uint8_t reg = data[0];
    size_t n = size - 1;
    if (out_size == 0) {
      if (reg == 0x05 && n == 4) {
        this->check_(data + 1);
        return esphome::i2c::ERROR_OK;
      }
      if (reg == 0x05) {
        this->writes++;
        this->checked++;
        this->bad++;
        ESP_LOGI(TAG, "ENV write of %u bytes, want 4 BAD", (unsigned) n);
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      static const uint8_t SW_RESET[4] = {0x11, 0xE5, 0x72, 0x8A};
      // SW_RESET, APP_START, MEAS_MODE and BASELINE are the only other writes the driver makes
      bool ok = (reg == 0xFF && n == 4 && memcmp(data + 1, SW_RESET, 4) == 0) || (reg == 0xF4 && n == 0) ||
                (reg == 0x01 && n == 1) || (reg == 0x11 && n == 2);
      if (!ok) {
        this->bad_writes++;
        ESP_LOGI(TAG, "write to 0x%02X of %u bytes BAD", reg, (unsigned) n);
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      return esphome::i2c::ERROR_OK;
    }
    size_t want = 0;  // reply length of each register the driver reads
    switch (reg) {
      case 0x00:
      case 0x20:
      case 0x21:
        want = 1;
        break;
      case 0x11:
      case 0x23:
      case 0x24:
        want = 2;
        break;
      case 0x02:
        want = 4;
        break;
      default:
        break;
    }
    if (n != 0 || want == 0 || out_size != want) {
      // a register the fake does not model, a read of the wrong length, or data written before the read
      memset(out, 0, out_size);
      this->bad_reads++;
      ESP_LOGI(TAG, "read of 0x%02X, %u bytes after %u written BAD", reg, (unsigned) out_size, (unsigned) n);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
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
        default:  // 0x24
          out[i] = i == 0 ? 0x20 : 0x00;
          break;  // app 2.0.0
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
