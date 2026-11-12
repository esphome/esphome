#pragma once
// Fake MS8607s (PT at 0x76.., humidity at 0x40..) on an in-firmware I2C bus, with per-device faults.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace msfake {
static const char *const TAG = "msfake";
using esphome::i2c::ErrorCode;

struct Pt {
  uint8_t addr;
  uint32_t d1, d2;
  int reset_nacks;  // NACK this many resets first
  bool bad_prom_crc;
  int zero_d1_every;  // every Nth pressure ADC read returns 0
  uint16_t prom[7]{0, 46372, 43981, 29059, 27842, 31553, 28165};
  uint8_t pending{0};  // 0x4A or 0x5A
  uint8_t last{0xFF};  // last command, for a read in its own transaction
  uint32_t resets{0}, conv_t{0}, conv_p{0}, adc{0}, prom_reads{0}, d1_reads{0}, bad_reads{0}, bad_cmds{0};
  uint32_t adc_no_conv{0};               // ADC reads with no conversion pending, which answer 0 like the real chip
  uint32_t no_reset{0};                  // PROM reads and conversions before the first reset, which loads the PROM
  uint32_t reset_nacked{0}, zero_d1{0};  // injected faults actually delivered
};
struct Hd {
  uint8_t addr;
  uint16_t raw;
  int bad_crc_every;  // every Nth read has a wrong CRC
  uint32_t resets{0}, measures{0}, reads{0}, bad_reads{0}, bad_cmds{0};
  uint32_t no_reset{0};  // measures before the first reset
  bool measuring{false};
  uint32_t read_no_measure{0};  // reads with no measure pending, which the real chip NACKs in no-hold mode
  uint32_t bad_crc{0};          // injected CRC faults actually delivered
};

// PROM words 0 to 6 sit at the even addresses 0xA0 to 0xAC
inline bool is_prom(uint8_t c) { return c >= 0xA0 && c <= 0xAC && (c & 1) == 0; }

inline uint8_t crc4(const uint16_t *w) {
  uint16_t r = 0;
  auto add = [&r](uint8_t b) {
    r ^= b;
    for (int i = 0; i < 8; i++)
      r = (r & 0x8000) ? (r << 1) ^ 0x3000 : (r << 1);
  };
  for (int i = 0; i < 7; i++) {
    add(w[i] >> 8);
    add(w[i] & 0xFF);
  }
  add(0);
  add(0);
  return (r >> 12) & 0xF;
}

struct Bus final : public esphome::i2c::I2CBus {
  Pt pts[8] = {
      {0x76, 6465444, 8077636, 0, false, 0},   // A all sensors
      {0x77, 6465444, 8077636, 0, false, 0},   // B no humidity
      {0x78, 6465444, 8077636, 0, false, 0},   // C humidity only
      {0x79, 6465444, 8077636, 0, false, 0},   // D temperature + humidity, humidity CRC faults
      {0x7A, 6465444, 8077636, 0, true, 0},    // E bad PROM CRC
      {0x7B, 6465444, 8077636, 2, false, 3},   // F reset NACKs twice, every 3rd D1 is zero
      {0x7C, 6465444, 16000000, 0, false, 0},  // G temperature out of range
      {0x7D, 6465444, 8077636, 0, false, 0},   // I humidity at full scale
  };
  Hd hds[8] = {{0x40, 31872, 0}, {0x00, 0, 0},     {0x41, 31872, 0}, {0x42, 31872, 2},
               {0x43, 31872, 0}, {0x44, 31872, 0}, {0x45, 31872, 0}, {0x46, 0xFFFE, 0}};

  Bus() {
    for (auto &p : this->pts) {
      // CRC is over word 0 with its top nibble zeroed
      uint16_t w[7];
      for (int i = 0; i < 7; i++)
        w[i] = p.prom[i];
      w[0] &= 0x0FFF;
      p.prom[0] = (uint16_t(crc4(w) ^ (p.bad_prom_crc ? 0x5 : 0)) << 12) | 0x0123;
    }
  }

  ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) override {
    for (auto &p : pts)
      if (p.addr == address)
        return pt_(p, data, size, out, out_size);
    for (auto &h : hds)
      if (h.addr == address && h.addr != 0)
        return hd_(h, data, size, out, out_size);
    return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
  }

  ErrorCode pt_(Pt &p, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) {
    if (size > 1) {  // every command is one byte
      p.bad_cmds++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size == 1) {
      uint8_t c = data[0];
      p.last = c;
      if (out_size == 0 && (is_prom(c) || c == 0x00))
        return esphome::i2c::ERROR_OK;  // register pointer only, data follows in a read
      if (c == 0x1E) {
        if (p.reset_nacks > 0) {
          p.reset_nacks--;
          p.reset_nacked++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
        p.resets++;
        p.pending = 0;  // a reset aborts a running conversion
      } else if (c == 0x4A || c == 0x5A) {
        if (p.resets == 0)
          p.no_reset++;
        p.pending = c;
        (c == 0x5A ? p.conv_t : p.conv_p)++;
      } else if (out_size == 0) {
        // a command the fake does not know
        p.bad_cmds++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
    }
    // a bare read answers the last PROM or ADC pointer; it never repeats a reset or conversion command
    uint8_t c = size == 1 ? data[0] : p.last;
    if (is_prom(c) && out_size == 2) {
      esphome::put_be16(out, p.prom[(c - 0xA0) / 2]);
      p.prom_reads++;
      if (p.resets == 0)
        p.no_reset++;
      return esphome::i2c::ERROR_OK;
    }
    if (c == 0x00 && out_size == 3) {
      uint32_t v = 0;
      if (p.pending == 0x5A)
        v = p.d2;
      if (p.pending == 0x4A) {
        p.d1_reads++;
        if (p.zero_d1_every && p.d1_reads % p.zero_d1_every == 0) {
          p.zero_d1++;
        } else {
          v = p.d1;
        }
      }
      if (p.pending == 0)
        p.adc_no_conv++;
      p.pending = 0;
      p.adc++;
      esphome::put_be24(out, v);
      return esphome::i2c::ERROR_OK;
    }
    if (out_size == 0)
      return esphome::i2c::ERROR_OK;
    // a read shape the fake does not model: fail it visibly instead of returning zeros
    memset(out, 0, out_size);
    p.bad_reads++;
    return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
  }

  ErrorCode hd_(Hd &h, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) {
    if (size > 1) {  // every command is one byte
      h.bad_cmds++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size >= 1 && data[0] == 0xFE) {
      h.resets++;
      h.measuring = false;
    } else if (size >= 1 && data[0] == 0xF5) {
      h.measures++;
      h.measuring = true;
      if (h.resets == 0)
        h.no_reset++;
    } else if (size >= 1 && out_size == 0) {
      // a command the fake does not know
      h.bad_cmds++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (out_size == 0)
      return esphome::i2c::ERROR_OK;
    if (size != 0 || out_size != 3) {
      // a read shape the fake does not model: fail it visibly instead of returning garbage
      memset(out, 0, out_size);
      h.bad_reads++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (!h.measuring) {
      // no measure pending: the real chip NACKs, so a driver that reads twice or without measuring fails here
      memset(out, 0, out_size);
      h.read_no_measure++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    h.measuring = false;
    h.reads++;
    uint16_t v = (h.raw & ~0x3) | 0x2;
    esphome::put_be16(out, v);
    out[2] = esphome::crc8(out, 2, 0x00, 0x31, true);
    if (h.bad_crc_every && h.reads % h.bad_crc_every == 0) {
      out[2] ^= 0xFF;
      h.bad_crc++;
    }
    return esphome::i2c::ERROR_OK;
  }

  void report() {
    for (auto &p : pts)
      ESP_LOGI(TAG,
               "PT 0x%02X resets=%u prom=%u convT=%u convP=%u adc=%u adc_no_conv=%u no_reset=%u bad_reads=%u "
               "bad_cmds=%u inj_reset_nack=%u reset_nack_left=%d inj_zero_d1=%u",
               p.addr, (unsigned) p.resets, (unsigned) p.prom_reads, (unsigned) p.conv_t, (unsigned) p.conv_p,
               (unsigned) p.adc, (unsigned) p.adc_no_conv, (unsigned) p.no_reset, (unsigned) p.bad_reads,
               (unsigned) p.bad_cmds, (unsigned) p.reset_nacked, p.reset_nacks, (unsigned) p.zero_d1);
    for (auto &h : hds)
      if (h.addr)
        ESP_LOGI(TAG,
                 "H 0x%02X resets=%u measures=%u reads=%u read_no_measure=%u no_reset=%u bad_reads=%u bad_cmds=%u "
                 "inj_bad_crc=%u",
                 h.addr, (unsigned) h.resets, (unsigned) h.measures, (unsigned) h.reads, (unsigned) h.read_no_measure,
                 (unsigned) h.no_reset, (unsigned) h.bad_reads, (unsigned) h.bad_cmds, (unsigned) h.bad_crc);
  }
};
inline Bus bus;
}  // namespace msfake
