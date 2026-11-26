#pragma once
// Fake I2C register file and fake UART devices (TMC22xx, SEN0609, CN105), so drivers pass setup without chips.
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace fk {

struct RegBus final : public esphome::i2c::I2CBus {
  static constexpr uint8_t N = 4;
  uint8_t addr[N]{0x68, 0x43, 0, 0};
  uint8_t regs[N][64]{};
  uint8_t ptr[N]{};
  int find(uint8_t a) {
    for (uint8_t i = 0; i < N; i++)
      if (addr[i] == a && a != 0)
        return i;
    return -1;
  }
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) override {
    int d = this->find(address);
    if (d < 0)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (wn > 0) {
      this->ptr[d] = w[0] & 63;
      for (size_t i = 1; i < wn; i++)
        this->regs[d][(this->ptr[d] + i - 1) & 63] = w[i];
    }
    for (size_t i = 0; i < rn; i++)
      r[i] = this->regs[d][(this->ptr[d] + i) & 63];
    return esphome::i2c::ERROR_OK;
  }
  int get_port() const { return 0; }
};
inline RegBus bus;

// UART device whose replies queue in an N byte ring; a subclass answers in write_array().
template<size_t N> struct RingUart : public esphome::uart::UARTComponent {
  uint8_t rx[N];
  size_t head{0}, tail{0};
  void push(uint8_t b) {
    if (this->tail - this->head < N)
      this->rx[this->tail++ % N] = b;
  }
  bool peek_byte(uint8_t *d) override {
    if (this->head == this->tail)
      return false;
    *d = this->rx[this->head % N];
    return true;
  }
  bool read_array(uint8_t *d, size_t n) override {
    if (this->tail - this->head < n)
      return false;
    for (size_t i = 0; i < n; i++)
      d[i] = this->rx[this->head++ % N];
    return true;
  }
  size_t available() override { return this->tail - this->head; }
  esphome::uart::UARTFlushResult flush() override { return esphome::uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)  // the base declares load_settings(bool) only there
  void load_settings(bool) override {}
#endif
  void check_logger_conflict() override {}
};

// TMC22xx single wire: echoes every byte, answers 4 byte read requests from a register map.
struct TmcUart final : public RingUart<64> {
  uint32_t regs[128]{};
  TmcUart() { this->regs[0x06] = 0x21000000; }
  static uint8_t crc(const uint8_t *d, size_t n) {
    uint8_t c = 0;
    for (size_t i = 0; i < n; i++) {
      uint8_t b = esphome::reverse_bits(d[i]);
      c = esphome::crc8(&b, 1, c, 0x07, true);
    }
    return c;
  }
  void write_array(const uint8_t *d, size_t n) override {
    for (size_t i = 0; i < n; i++)
      this->push(d[i]);
    if (n == 4 && d[0] == 0x05 && (d[2] & 0x80) == 0) {
      uint32_t v = this->regs[d[2] & 0x7F];
      uint8_t rep[8] = {0x05, 0xFF, d[2], uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0};
      rep[7] = crc(rep, 7);
      for (uint8_t b : rep)
        this->push(b);
    } else if (n == 8 && d[0] == 0x05 && (d[2] & 0x80) != 0) {
      uint8_t reg = d[2] & 0x7F;
      uint32_t v = esphome::encode_uint32(d[3], d[4], d[5], d[6]);
      if (reg == 0x01)
        this->regs[1] &= ~v;  // GSTAT clears on write 1
      else
        this->regs[reg] = v;
    }
  }
};
inline TmcUart tmc;

// SEN0609 text console: "Done" for stop/start/save and every set command except setLedMode; silence otherwise.
struct SenUart final : public RingUart<128> {
  char line[64];
  size_t n{0};
  void push_str(const char *s) {
    for (; *s; s++)
      this->push(*s);
  }
  static bool pre(const char *l, const char *p) { return strncmp(l, p, strlen(p)) == 0; }
  void write_array(const uint8_t *d, size_t len) override {
    for (size_t i = 0; i < len; i++) {
      char c = d[i];
      if (c == '\r')
        continue;
      if (c != '\n') {
        if (this->n < sizeof(this->line) - 1)
          this->line[this->n++] = c;
        continue;
      }
      this->line[this->n] = 0;
      this->n = 0;
      const char *l = this->line;
      this->push_str(l);
      this->push_str("\r\n");
      if (pre(l, "setLedMode") || pre(l, "resetSystem") || pre(l, "resetCfg") || pre(l, "get"))
        continue;
      if (pre(l, "sensor") || pre(l, "save") || pre(l, "set"))
        this->push_str("Done\r\n");
    }
  }
};
inline SenUart sen;

// CN105 heat pump: ignores the first connect (read timeout), then answers connect, status and write settings.
struct CnUart final : public RingUart<256> {
  uint8_t buf[40];
  size_t n{0};
  uint8_t connects{0};
  void reply(uint8_t type, const uint8_t *p, uint8_t len) {
    uint8_t pkt[24] = {0xFC, type, 0x01, 0x30, len};
    memcpy(pkt + 5, p, len);
    uint8_t sum = 0;
    for (size_t i = 0; i < 5u + len; i++)
      sum += pkt[i];
    pkt[5 + len] = 0xFC - sum;
    for (size_t i = 0; i < 6u + len; i++)
      this->push(pkt[i]);
  }
  void handle() {
    uint8_t type = this->buf[1];
    uint8_t p[16]{};
    if (type == 0x5A) {
      if (this->connects++ == 0)
        return;
      this->reply(0x7A, p, 1);
    } else if (type == 0x42) {
      p[0] = this->buf[5];
      this->reply(0x62, p, 16);
    } else if (type == 0x41) {
      this->reply(0x61, p, 16);
    }
  }
  void write_array(const uint8_t *d, size_t len) override {
    for (size_t i = 0; i < len; i++) {
      if (this->n == 0 && d[i] != 0xFC)
        continue;
      if (this->n < sizeof(this->buf))
        this->buf[this->n++] = d[i];
      if (this->n >= 5 && this->n == 6u + this->buf[4]) {
        this->handle();
        this->n = 0;
      }
    }
  }
};
inline CnUart cn;

// ENS210 CRC-7 over a 17 bit payload (poly 0x89, init 0x7F), and a helper that writes one 3 byte value
inline uint32_t ens_crc7(uint32_t value) {
  uint32_t poly = 0x89u << (17 - 7 - 1), bit = 1u << 16;
  value <<= 7;
  bit <<= 7;
  poly <<= 7;
  value |= 0x7F;
  while (bit & (0x1FFFFu << 7)) {
    if (bit & value)
      value ^= poly;
    bit >>= 1;
    poly >>= 1;
  }
  return value;
}
inline void ens_set(uint8_t off, uint16_t data, bool valid, bool good_crc) {
  uint32_t payload = data | (valid ? 0x10000u : 0);
  uint32_t crc = ens_crc7(payload) & 0x7F;
  if (!good_crc)
    crc ^= 0x01;
  uint32_t v = payload | (crc << 17);
  bus.regs[1][0x30 + off] = v;
  bus.regs[1][0x31 + off] = v >> 8;
  bus.regs[1][0x32 + off] = v >> 16;
}

}  // namespace fk
