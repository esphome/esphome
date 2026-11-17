#pragma once
// Fake BMI270 (+ BMM150 behind its aux interface) on an in-firmware I2C bus (esphome#22778).
// BMI270: register file with reset values, soft reset, config upload port (INIT_ADDR_0/1 word address, INIT_DATA
// streaming into 8 KB), INTERNAL_STATUS 0x01 only when INIT_CTRL=1 arrives with every config byte written once and
// the memory's FNV-1a equal to `expected_fnv` (default: the current table, computed on the host from
// bmi270_config.h), 0x02 otherwise.
// Aux interface: AUX_DEV_ID selects the BMM150 (addr << 1); in manual mode (AUX_IF_CONF bit 7) a write to
// AUX_WR_ADDR writes AUX_WR_DATA to the BMM150 and a write to AUX_RD_ADDR reads man_rd_burst (1/2/6/8) bytes into
// AUX_X_LSB..; either sets STATUS.aux_busy for `busy_reads` STATUS reads. In auto mode with aux enabled the AUX data
// registers mirror the BMM150 from AUX_RD_ADDR. Modes: bmm_mode 0 ok, 1 absent (aux transfers do nothing),
// 2 wrong chip id; busy_stuck keeps aux_busy set (stuck_from_aux_op: from the Nth aux transfer); nack_at fails main-bus
// transaction N (1 based). `norm_fnv` hashes the register-level effect of the stream (split or merged writes hash the
// same). Heap is sampled at INIT_CTRL=0/1 and inside every INIT_DATA burst; -DBMI_FAKE_WRAP_MALLOC with the link flag
// from alloc_count.hpp counts malloc() during the upload.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"
#ifdef USE_HOST
#include <string>
#include <vector>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#elif defined(USE_ESP32)
#include <esp_heap_caps.h>
#endif

#ifdef BMI_FAKE_WRAP_MALLOC
#include "alloc_count.hpp"
#endif

namespace bmi270fake {
static const char *const TAG = "bmi270fake";
static constexpr uint8_t ADDR = 0x68;
static constexpr size_t MEM = 8192;
static constexpr uint8_t R_CHIP_ID = 0x00, R_STATUS = 0x03, R_AUX_X = 0x04, R_DATA_8 = 0x0C, R_INTERNAL_STATUS = 0x21,
                         R_TEMP_0 = 0x22, R_AUX_DEV_ID = 0x4B, R_AUX_IF_CONF = 0x4C, R_AUX_RD_ADDR = 0x4D,
                         R_AUX_WR_ADDR = 0x4E, R_AUX_WR_DATA = 0x4F, R_INIT_CTRL = 0x59, R_INIT_ADDR_0 = 0x5B,
                         R_INIT_ADDR_1 = 0x5C, R_INIT_DATA = 0x5E, R_IF_CONF = 0x6B, R_PWR_CONF = 0x7C,
                         R_PWR_CTRL = 0x7D, R_CMD = 0x7E;

inline uint32_t heap_free() {
#ifdef USE_ESP8266
  return ESP.getFreeHeap();
#elif defined(USE_ESP32)
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#else
  return 0;
#endif
}
inline uint32_t heap_block() {
#ifdef USE_ESP8266
  return ESP.getMaxFreeBlockSize();
#elif defined(USE_ESP32)
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
#else
  return 0;
#endif
}

struct Bus final : public esphome::i2c::I2CBus {
  uint8_t regs[128]{};
  uint8_t mem[MEM]{};
  uint8_t written[MEM / 8]{};
  uint8_t bmm[128]{};  // BMM150 registers 0x00..0x7F
  uint8_t bmm_addr{0x10};
  uint8_t bmm_mode{0};
  bool busy_stuck{false};
  uint8_t stuck_from_aux_op{0};  // aux busy never clears from this aux transfer on (1 based), 0 = off
  uint32_t aux_ops{0};
  uint8_t busy_reads{2}, busy_left{0};
  uint32_t nack_at{0};
  uint32_t expected_fnv{0x95BB75B5};  // FNV-1a of the 8192 byte table in bmi270_config.h
  uint32_t mem_fnv{0}, norm_fnv{2166136261u};
  uint32_t transactions{0}, reads{0}, writes{0}, data_writes{0}, data_bytes{0}, addr_writes{0}, rewrites{0};
  uint32_t protocol_errors{0}, loads_ok{0}, loads_bad{0}, resets{0}, aux_writes{0}, aux_reads{0}, status_reads{0};
  uint32_t burst_min{UINT32_MAX}, burst_max{0};
  uint32_t heap_start{0}, block_start{0}, heap_end{0}, block_end{0}, heap_min_in_burst{UINT32_MAX};
  uint32_t bursts_heap_below_start{0};
  // accel x/y/z, gyro x/y/z (LE int16 at 0x0C..0x17), temperature raw at 0x22
  int16_t sample[6]{4096, -8192, 12288, 1638, -3277, 819};
  int16_t temp_raw{1024};
  // BMM150 raw data 0x42..0x49 and trim 0x5D..0x74 (datasheet-like values)
  uint8_t mag_raw[8]{0x10, 0x12, 0x20, 0xF0, 0x30, 0x08, 0x40, 0x1A};
  // x1=-2 y1=3 z4=-10 x2=26 y2=27 z2=760 z1=24786 xyz1=7101 z3=64 xy2=-3 xy1=29
  uint8_t trim[24]{0xFE, 0x03, 0x00, 0x00, 0x00, 0xF6, 0xFF, 0x1A, 0x1B, 0x00, 0x00, 0xF8,
                   0x02, 0xD2, 0x60, 0xBD, 0x1B, 0x40, 0x00, 0xFD, 0x1D, 0x00, 0x00, 0x00};
#ifdef USE_HOST
  std::vector<std::string> log;  // raw transaction stream, plus "A" lines for BMM150 side effects
#endif

  Bus() {
    this->reset_();
    this->bmm_reset_(true);
  }

  // Puts the BMM150 on `addr` with fault mode `mode` and powers it off
  void init_bmm(uint8_t addr, uint8_t mode = 0) {
    this->bmm_addr = addr;
    this->bmm_mode = mode;
    this->bmm_reset_(true);
  }

  void reset_() {
    memset(this->regs, 0, sizeof(this->regs));
    this->regs[R_CHIP_ID] = 0x24;
    this->regs[R_PWR_CONF] = 0x03;
    this->regs[R_AUX_DEV_ID] = 0x20;
    this->regs[R_AUX_IF_CONF] = 0x83;
    this->regs[R_AUX_RD_ADDR] = 0x42;
    this->regs[R_AUX_WR_ADDR] = 0x4C;
    this->regs[R_AUX_WR_DATA] = 0x02;
    memset(this->mem, 0, sizeof(this->mem));
    memset(this->written, 0, sizeof(this->written));
    this->busy_left = 0;
  }
  void bmm_reset_(bool power_off) {
    uint8_t pwr = this->bmm[0x4B] & 0x01;
    memset(this->bmm, 0, sizeof(this->bmm));
    this->bmm[0x40] = this->bmm_mode == 2 ? 0x33 : 0x32;
    this->bmm[0x4B] = power_off ? 0 : pwr;
    this->bmm[0x4C] = 0x06;  // sleep mode
    memcpy(&this->bmm[0x42], this->mag_raw, 8);
    memcpy(&this->bmm[0x5D], this->trim, sizeof(this->trim));
  }
  uint8_t bmm_read_(uint8_t r) {
    if (r == 0x40 && !(this->bmm[0x4B] & 0x01))
      return 0x00;  // chip id reads 0 in suspend
    if (r >= 0x42 && r <= 0x49)
      return this->mag_raw[r - 0x42];
    return this->bmm[r & 0x7F];
  }
  void mix_(uint32_t v) {
    for (int i = 0; i < 4; i++) {
      this->norm_fnv ^= (v >> (8 * i)) & 0xFF;
      this->norm_fnv *= 16777619u;
    }
  }
  static uint32_t fnv(const uint8_t *d, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
      h ^= d[i];
      h *= 16777619u;
    }
    return h;
  }
  uint32_t covered_() const {
    uint32_t n = 0;
    for (uint8_t b : this->written)
      n += __builtin_popcount(b);
    return n;
  }
  void note_(const char *fmt, unsigned a, unsigned b) {
#ifdef USE_HOST
    char s[48];
    snprintf(s, sizeof(s), fmt, a, b);
    this->log.push_back(s);
#endif
  }
  bool aux_present_() const {
    return this->bmm_mode != 1 && (this->regs[R_IF_CONF] & 0x20) && this->regs[R_AUX_DEV_ID] == (this->bmm_addr << 1);
  }
  void aux_trigger_() {
    this->aux_ops++;
    if (this->stuck_from_aux_op && this->aux_ops >= this->stuck_from_aux_op)
      this->busy_stuck = true;
    this->busy_left = this->busy_reads;
  }
  void fill_data_() {
    for (int i = 0; i < 6; i++) {
      this->regs[R_DATA_8 + 2 * i] = this->sample[i] & 0xFF;
      this->regs[R_DATA_8 + 2 * i + 1] = (uint16_t) this->sample[i] >> 8;
    }
    this->regs[R_TEMP_0] = this->temp_raw & 0xFF;
    this->regs[R_TEMP_0 + 1] = (uint16_t) this->temp_raw >> 8;
    // auto mode: the BMI270 keeps AUX_X.. filled from the BMM150
    if (!(this->regs[R_AUX_IF_CONF] & 0x80) && (this->regs[R_PWR_CTRL] & 0x01) && this->aux_present_()) {
      for (int i = 0; i < 8; i++)
        this->regs[R_AUX_X + i] = this->bmm_read_(this->regs[R_AUX_RD_ADDR] + i);
    }
  }

  void write_reg_(uint8_t reg, uint8_t v) {
    this->mix_(0x01000000u | (reg << 8) | v);
    if (reg == R_CMD && v == 0xB6) {
      this->resets++;
      this->reset_();
      return;
    }
    if (reg == R_INIT_ADDR_0 || reg == R_INIT_ADDR_1)
      this->addr_writes++;
    if (reg == R_INIT_CTRL) {
      if (v == 0x00) {
        this->heap_start = heap_free();
        this->block_start = heap_block();
        this->heap_min_in_burst = UINT32_MAX;
        this->bursts_heap_below_start = 0;
        this->regs[R_INTERNAL_STATUS] = 0x00;
#ifdef BMI_FAKE_WRAP_MALLOC
        alloc_count::count = 0;
        alloc_count::window = true;
#endif
      } else if (v == 0x01) {
#ifdef BMI_FAKE_WRAP_MALLOC
        alloc_count::window = false;
#endif
        this->heap_end = heap_free();
        this->block_end = heap_block();
        this->mem_fnv = fnv(this->mem, MEM);
        bool ok = this->mem_fnv == this->expected_fnv && this->covered_() == MEM && this->rewrites == 0;
        this->regs[R_INTERNAL_STATUS] = ok ? 0x01 : 0x02;
        (ok ? this->loads_ok : this->loads_bad)++;
      }
    }
    this->regs[reg & 0x7F] = v;
    bool manual = this->regs[R_AUX_IF_CONF] & 0x80;
    if (reg == R_AUX_WR_ADDR && manual) {
      this->aux_writes++;
      this->aux_trigger_();
      if (this->aux_present_()) {
        uint8_t d = this->regs[R_AUX_WR_DATA];
        this->note_("A W %02x %02x", v, d);
        this->mix_(0x04000000u | (v << 8) | d);
        if (v == 0x4B && (d & 0x82) == 0x82)
          this->bmm_reset_(false);
        else
          this->bmm[v & 0x7F] = d;
      }
    } else if (reg == R_AUX_RD_ADDR && manual) {
      this->aux_reads++;
      this->aux_trigger_();
      if (this->aux_present_()) {
        static const uint8_t LEN[4] = {1, 2, 6, 8};
        uint8_t n = LEN[(this->regs[R_AUX_IF_CONF] >> 2) & 3];
        this->note_("A R %02x n%u", v, n);
        this->mix_(0x05000000u | (v << 8) | n);
        for (uint8_t i = 0; i < n; i++)
          this->regs[R_AUX_X + i] = this->bmm_read_(v + i);
      }
    }
  }

  void init_data_(const uint8_t *d, size_t n) {
    this->data_writes++;
    this->data_bytes += n;
    if (n < this->burst_min)
      this->burst_min = n;
    if (n > this->burst_max)
      this->burst_max = n;
    uint32_t h = heap_free();
    if (h < this->heap_min_in_burst)
      this->heap_min_in_burst = h;
    if (h < this->heap_start)
      this->bursts_heap_below_start++;
    if ((this->regs[R_PWR_CONF] & 0x01) || this->regs[R_INIT_CTRL] != 0x00) {
      this->protocol_errors++;
      return;
    }
    size_t off = 2 * ((this->regs[R_INIT_ADDR_0] & 0x0F) | ((size_t) this->regs[R_INIT_ADDR_1] << 4));
    for (size_t i = 0; i < n; i++, off++) {
      if (off >= MEM) {
        this->protocol_errors++;
        return;
      }
      this->mix_(0x02000000u | (off << 8) | d[i]);
      if (this->written[off / 8] & (1 << (off % 8)))
        this->rewrites++;
      this->written[off / 8] |= 1 << (off % 8);
      this->mem[off] = d[i];
    }
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != ADDR)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    this->transactions++;
#ifdef USE_HOST
    {
      std::string s = size > 1 ? "W" : "R";
      char b[8];
      for (size_t i = 0; i < size; i++) {
        snprintf(b, sizeof(b), " %02x", data[i]);
        s += b;
      }
      if (out_size) {
        snprintf(b, sizeof(b), " r%u", (unsigned) out_size);
        s += b;
      }
      if (this->transactions == this->nack_at)
        s += " NACK";
      this->log.push_back(s);
    }
#endif
    if (this->transactions == this->nack_at)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 0) {
      this->protocol_errors++;
      return esphome::i2c::ERROR_INVALID_ARGUMENT;
    }
    uint8_t reg = data[0] & 0x7F;
    if (size > 1) {
      this->writes++;
      if (reg == R_INIT_DATA) {
        this->init_data_(data + 1, size - 1);
      } else {
        for (size_t i = 1; i < size; i++)
          this->write_reg_((reg + i - 1) & 0x7F, data[i]);
      }
    }
    if (out_size) {
      this->reads++;
      // STATUS polls are timing dependent (busy wait), so they stay out of the hash
      if (reg != R_STATUS)
        this->mix_(0x03000000u | (reg << 8) | (out_size & 0xFF));
      if (reg == R_STATUS) {
        this->status_reads++;
        this->regs[R_STATUS] = (this->busy_stuck || this->busy_left) ? 0x04 : 0x00;
        if (this->busy_left)
          this->busy_left--;
      }
      this->fill_data_();
      for (size_t i = 0; i < out_size; i++)
        out[i] = this->regs[(reg + i) & 0x7F];
    }
    return esphome::i2c::ERROR_OK;
  }

  // Final configuration registers, for comparing builds
  void final_regs(char *buf, size_t n) const {
    snprintf(buf, n,
             "ACC_CONF=%02X ACC_RANGE=%02X GYR_CONF=%02X GYR_RANGE=%02X PWR_CONF=%02X PWR_CTRL=%02X IF_CONF=%02X "
             "AUX_DEV_ID=%02X AUX_IF_CONF=%02X AUX_RD_ADDR=%02X BMM_PWR=%02X BMM_OPMODE=%02X",
             this->regs[0x40], this->regs[0x41], this->regs[0x42], this->regs[0x43], this->regs[R_PWR_CONF],
             this->regs[R_PWR_CTRL], this->regs[R_IF_CONF], this->regs[R_AUX_DEV_ID], this->regs[R_AUX_IF_CONF],
             this->regs[R_AUX_RD_ADDR], this->bmm[0x4B], this->bmm[0x4C]);
  }

  void report() const {
    char fr[220];
    this->final_regs(fr, sizeof(fr));
    ESP_LOGI(TAG,
             "UPLOAD status=0x%02X ok=%u bad=%u mem_fnv=%08X expected=%08X covered=%u rewrites=%u perr=%u resets=%u",
             this->regs[R_INTERNAL_STATUS], this->loads_ok, this->loads_bad, this->mem_fnv, this->expected_fnv,
             this->covered_(), this->rewrites, this->protocol_errors, this->resets);
    ESP_LOGI(TAG,
             "STREAM tx=%u writes=%u reads=%u data_writes=%u data_bytes=%u burst=%u..%u addr_reg_writes=%u aux_w=%u "
             "aux_r=%u norm=%08X",
             this->transactions, this->writes, this->reads, this->data_writes, this->data_bytes,
             this->burst_min == UINT32_MAX ? 0 : this->burst_min, this->burst_max, this->addr_writes, this->aux_writes,
             this->aux_reads, this->norm_fnv);
    ESP_LOGI(TAG, "REGS %s", fr);
    ESP_LOGI(TAG, "HEAP start=%u/%u end=%u/%u min_in_burst=%u bursts_below_start=%u", this->heap_start,
             this->block_start, this->heap_end, this->block_end, this->heap_min_in_burst,
             this->bursts_heap_below_start);
#ifdef BMI_FAKE_WRAP_MALLOC
    ESP_LOGI(TAG, "MALLOC during upload=%u", (unsigned) alloc_count::count);
#endif
  }
};
inline Bus bus;
}  // namespace bmi270fake
