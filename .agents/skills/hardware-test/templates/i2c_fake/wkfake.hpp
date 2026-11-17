#pragma once
// Fake WeiKai WK2168 on an in-firmware I2C bus (esphome#22759, 2026-11-15).
// Device address 0 A1 A0 1 0 C1 C0 F (base 0x10 when A1 A0 = 00): eight addresses, F=1 is the FIFO of channel C1C0.
// The register byte carries only A3..A0; the 6 bit register address is C1C0A3A2A1A0 (datasheet 12.4). Global registers
// are 0x00 GENA, 0x01 GRST, 0x02 GMUT, 0x10 GIER, 0x11 GIFR, 0x21 GPDIR, 0x31 GPDAT; A3..A0 >= 3 is a sub-UART register
// of channel C1C0. A register byte with bits above A3..A0 is truncated like the chip does and counted in hi_bits.
// A write to GRST soft-resets the channels in its low nibble (registers and FIFOs to defaults) and is logged loudly.
// Every transaction is classified: W (register write), P (1 byte write: register pointer), R (read only, uses the
// pointer of that address), WR (write then read in one transaction, repeated start). trace > 0 logs the next `trace`
// transactions; writes, faults and changed read values are always logged; the rest is counted in cnt[][][].
// Fault injection: fail_reads NACKs the next read phases (R or WR), fail_ptr the next P transfers, every_n NACKs every
// Nth read phase once. After a NACK, the next read phase on the same address and register counts as a retry.
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

namespace wkfake {
static const char *const TAG = "wkfake";
using esphome::i2c::ErrorCode;

enum Kind : uint8_t { K_W = 0, K_P = 1, K_R = 2, K_WR = 3 };
static const char *const KIND[] = {"W ", "P ", "R ", "WR"};

struct Fifo {
  uint8_t d[256];
  uint16_t head{0}, n{0};
  void clear() { head = n = 0; }
  bool push(uint8_t b) {
    if (n == 256)
      return false;
    d[(head + n++) & 0xFF] = b;
    return true;
  }
  uint8_t pop() {
    if (!n)
      return 0;
    uint8_t b = d[head];
    head = (head + 1) & 0xFF;
    n--;
    return b;
  }
};

struct Bus final : public esphome::i2c::I2CBus {
  uint8_t base{0x10};
  uint8_t glob[64]{};
  uint8_t pg0[4][16]{}, pg1[4][16]{}, spage[4]{};
  uint8_t ptr[8]{};
  uint8_t gpio_in{0};
  Fifo rx[4];
  // counters
  uint32_t cnt[8][64][4]{};
  uint32_t n_trans{0}, grst_writes{0}, hi_bits{0}, reserved{0}, tx_bytes[4]{}, tx_dropped{0}, rx_dropped{0};
  uint32_t injected{0}, retries{0}, no_retry{0}, gpdir_writes{0}, gpdat_writes{0}, gpdat_reads{0};
  uint8_t last_read[8][64];
  // controls
  uint32_t trace{0}, fail_reads{0}, fail_ptr{0}, every_n{0}, read_phases{0};
  bool await_retry{false};
  uint8_t await_addr{0}, await_reg{0};

  Bus() {
    memset(this->last_read, 0xEE, sizeof(this->last_read));
    for (int c = 0; c < 4; c++)
      this->reset_channel_(c);
  }
  void reset_channel_(int c) {
    memset(this->pg0[c], 0, 16);
    memset(this->pg1[c], 0, 16);
    this->spage[c] = 0;
    this->rx[c].clear();
  }
  static const char *gname(uint8_t a6) {
    switch (a6) {
      case 0x00:
        return "GENA";
      case 0x01:
        return "GRST";
      case 0x02:
        return "GMUT";
      case 0x10:
        return "GIER";
      case 0x11:
        return "GIFR";
      case 0x21:
        return "GPDIR";
      case 0x31:
        return "GPDAT";
      default:
        return "RSVD";
    }
  }
  static const char *cname(uint8_t r, bool p1) {
    static const char *const P0[] = {"",     "",      "",      "SPAGE", "SCR", "LCR",  "FCR", "SIER",
                                     "SIFR", "TFCNT", "RFCNT", "FSR",   "LSR", "FDAT", "R0E", "R0F"};
    static const char *const P1[] = {"",    "",    "",    "SPAGE", "BRH", "BRL", "BRD", "RFI",
                                     "TFI", "R19", "R1A", "R1B",   "R1C", "R1D", "R1E", "R1F"};
    return p1 ? P1[r & 15] : P0[r & 15];
  }
  void name_(uint8_t addr, uint8_t regbyte, char *out, size_t len) {
    uint8_t c = (addr >> 1) & 3, r = regbyte & 0x0F;
    if (r < 3)
      snprintf(out, len, "%s(%02X)", gname((c << 4) | r), (c << 4) | r);
    else
      snprintf(out, len, "ch%u.%s", c, cname(r, r != 3 && this->spage[c]));
  }
  uint8_t gpdat_value() const {
    uint8_t dir = this->glob[0x21];
    return (this->glob[0x31] & dir) | (this->gpio_in & ~dir);
  }
  void show_gpio(const char *why) {
    ESP_LOGI(TAG, "GPIO %s: GPDIR=%02X GPDAT latch=%02X inputs=%02X pins=%02X", why, this->glob[0x21], this->glob[0x31],
             this->gpio_in, this->gpdat_value());
  }

  void reg_write_(uint8_t addr, uint8_t regbyte, uint8_t v) {
    uint8_t c = (addr >> 1) & 3, r = regbyte & 0x0F;
    if (r < 3) {
      uint8_t a6 = (c << 4) | r;
      switch (a6) {
        case 0x01:
          this->grst_writes++;
          ESP_LOGW(TAG, "!!!!! GRST WRITE %02X via @%02X reg byte %02X: soft reset of channels mask %X !!!!!", v, addr,
                   regbyte, v & 0x0F);
          for (int i = 0; i < 4; i++)
            if (v & (1 << i))
              this->reset_channel_(i);
          return;  // self clearing
        case 0x21:
          this->gpdir_writes++;
          this->glob[a6] = v;
          this->show_gpio("dir write");
          return;
        case 0x31:
          this->gpdat_writes++;
          this->glob[a6] = v;
          this->show_gpio("data write");
          return;
        case 0x00:
        case 0x02:
        case 0x10:
        case 0x11:
          this->glob[a6] = v;
          return;
        default:
          this->reserved++;
          ESP_LOGW(TAG, "write to reserved global %02X", a6);
          return;
      }
    }
    if (r == 3) {
      this->spage[c] = v & 1;
      return;
    }
    if (this->spage[c]) {
      this->pg1[c][r] = v;
      return;
    }
    if (r == 6) {  // FCR: reset bits clear the FIFOs and do not stay set
      if (v & 1)
        this->rx[c].clear();
      v &= 0x0C;
    }
    if (r == 0x0D) {
      this->tx_bytes[c]++;
      return;
    }
    if (r >= 9 && r <= 0x0C)
      return;  // read only
    this->pg0[c][r] = v;
  }

  uint8_t reg_read_(uint8_t addr, uint8_t regbyte) {
    uint8_t c = (addr >> 1) & 3, r = regbyte & 0x0F;
    if (r < 3) {
      uint8_t a6 = (c << 4) | r;
      if (a6 == 0x01)
        return 0;
      if (a6 == 0x31) {
        this->gpdat_reads++;
        return this->gpdat_value();
      }
      return this->glob[a6];
    }
    if (r == 3)
      return this->spage[c];
    if (this->spage[c])
      return this->pg1[c][r];
    switch (r) {
      case 0x09:
        return 0;  // TFCNT: the fake sends at once
      case 0x0A:
        return (uint8_t) this->rx[c].n;  // RFCNT (256 reads 0)
      case 0x0B:
        return this->rx[c].n ? (1 << 3) : 0;  // FSR: RFDAT
      case 0x0C:
        return 0;  // LSR
      case 0x0D:
        return this->rx[c].pop();  // FDAT
      default:
        return this->pg0[c][r];
    }
  }

  bool channel_on_(int c, uint8_t bit) const { return (this->glob[0] & (1 << c)) && (this->pg0[c][4] & bit); }

  void inject_rx(int c, const char *s) {
    if (!this->channel_on_(c, 1)) {
      this->rx_dropped += strlen(s);
      ESP_LOGW(TAG, "RX inject ch%d dropped %u bytes: receiver disabled (GENA=%02X SCR=%02X)", c, (unsigned) strlen(s),
               this->glob[0], this->pg0[c][4]);
      return;
    }
    for (const char *p = s; *p; p++)
      this->rx[c].push((uint8_t) *p);
    ESP_LOGI(TAG, "RX inject ch%d '%s' fifo=%u", c, s, this->rx[c].n);
  }

  void set_input(uint8_t pin, bool v) {
    if (v)
      this->gpio_in |= 1 << pin;
    else
      this->gpio_in &= ~(1 << pin);
    this->show_gpio("input change");
  }

  ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) override {
    if ((address & 0xF8) != this->base)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 0 && out_size == 0)
      return esphome::i2c::ERROR_OK;
    this->n_trans++;
    const uint8_t a = address & 7, c = (a >> 1) & 3;
    char nm[24];
    if (a & 1) {  // FIFO
      if (size) {
        char txt[48];
        size_t i = 0;
        for (; i < size && i < sizeof(txt) - 1; i++)
          txt[i] = data[i] >= 0x20 && data[i] < 0x7F ? (char) data[i] : '.';
        txt[i] = 0;
        if (this->channel_on_(c, 2)) {
          this->tx_bytes[c] += size;
          ESP_LOGI(TAG, "#%u @%02X FIFO TX ch%u %u bytes '%s'", this->n_trans, address, c, (unsigned) size, txt);
        } else {
          this->tx_dropped += size;
          ESP_LOGW(TAG, "#%u @%02X FIFO TX ch%u %u bytes '%s' DROPPED: transmitter disabled (GENA=%02X SCR=%02X)",
                   this->n_trans, address, c, (unsigned) size, txt, this->glob[0], this->pg0[c][4]);
        }
      }
      if (out_size) {
        for (size_t i = 0; i < out_size; i++)
          out[i] = this->rx[c].pop();
        ESP_LOGI(TAG, "#%u @%02X FIFO RX ch%u %u bytes, %u left", this->n_trans, address, c, (unsigned) out_size,
                 this->rx[c].n);
      }
      return esphome::i2c::ERROR_OK;
    }
    Kind k;
    uint8_t regbyte;
    if (out_size == 0)
      k = size == 1 ? K_P : K_W, regbyte = data[0];
    else if (size == 0)
      k = K_R, regbyte = this->ptr[a];
    else
      k = K_WR, regbyte = data[0];
    if (regbyte & 0xF0 && (++this->hi_bits <= 3 || k == K_W || k == K_P)) {
      ESP_LOGW(TAG, "#%u @%02X %s reg byte %02X has bits above A3..A0; the chip sees register %X", this->n_trans,
               address, KIND[k], regbyte, regbyte & 0x0F);
    }
    this->cnt[a][regbyte & 0x3F][k]++;
    this->name_(address, regbyte, nm, sizeof(nm));
    if (k == K_P) {
      if (this->fail_ptr) {
        this->fail_ptr--;
        this->injected++;
        this->arm_retry_(address, regbyte);
        ESP_LOGW(TAG, "#%u @%02X P  r=%02X %s INJECTED NACK", this->n_trans, address, regbyte, nm);
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      this->ptr[a] = regbyte;
      if (this->trace) {
        this->trace--;
        ESP_LOGI(TAG, "#%u @%02X P  r=%02X %s", this->n_trans, address, regbyte, nm);
      }
      return esphome::i2c::ERROR_OK;
    }
    if (k == K_W) {
      this->ptr[a] = regbyte;
      for (size_t i = 1; i < size; i++)
        this->reg_write_(address, regbyte, data[i]);
      if (this->trace)
        this->trace--;
      ESP_LOGI(TAG, "#%u @%02X W  r=%02X %s = %02X", this->n_trans, address, regbyte, nm, data[1]);
      return esphome::i2c::ERROR_OK;
    }
    // read phase (R or WR)
    this->read_phases++;
    if (this->await_retry) {
      this->await_retry = false;
      if (address == this->await_addr && (regbyte == this->await_reg || k == K_R)) {
        this->retries++;
        ESP_LOGI(TAG, "#%u @%02X %s r=%02X %s RETRY after injected NACK", this->n_trans, address, KIND[k], regbyte, nm);
      } else {
        this->no_retry++;
        ESP_LOGW(TAG, "#%u @%02X %s r=%02X %s: no retry of the NACKed read", this->n_trans, address, KIND[k], regbyte,
                 nm);
      }
    }
    if (k == K_WR)
      this->ptr[a] = regbyte;
    bool fail = false;
    if (this->fail_reads) {
      this->fail_reads--;
      fail = true;
    } else if (this->every_n && this->read_phases % this->every_n == 0) {
      fail = true;
    }
    if (fail) {
      this->injected++;
      this->arm_retry_(address, regbyte);
      memset(out, 0, out_size);
      ESP_LOGW(TAG, "#%u @%02X %s r=%02X %s INJECTED NACK", this->n_trans, address, KIND[k], regbyte, nm);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->reg_read_(address, regbyte);
    uint8_t &last = this->last_read[a][regbyte & 0x3F];
    bool changed = last != out[0];
    last = out[0];
    if (this->trace) {
      this->trace--;
      ESP_LOGI(TAG, "#%u @%02X %s r=%02X %s -> %02X", this->n_trans, address, KIND[k], regbyte, nm, out[0]);
    } else if (changed) {
      ESP_LOGI(TAG, "#%u @%02X %s r=%02X %s -> %02X (changed)", this->n_trans, address, KIND[k], regbyte, nm, out[0]);
    }
    return esphome::i2c::ERROR_OK;
  }
  void arm_retry_(uint8_t addr, uint8_t reg) {
    this->await_retry = true;
    this->await_addr = addr;
    this->await_reg = reg;
  }

  void dump() {
    ESP_LOGI(TAG,
             "STATS trans=%u grst_writes=%u hi_bits=%u reserved=%u gpdir_w=%u gpdat_w=%u gpdat_r=%u tx=%u/%u "
             "tx_dropped=%u rx_dropped=%u injected=%u retries=%u no_retry=%u",
             this->n_trans, this->grst_writes, this->hi_bits, this->reserved, this->gpdir_writes, this->gpdat_writes,
             this->gpdat_reads, this->tx_bytes[0], this->tx_bytes[1], this->tx_dropped, this->rx_dropped,
             this->injected, this->retries, this->no_retry);
    this->show_gpio("now");
    for (int c = 0; c < 2; c++)
      ESP_LOGI(TAG, "ch%d SCR=%02X LCR=%02X FCR=%02X BR=%02X%02X.%X SPAGE=%u", c, this->pg0[c][4], this->pg0[c][5],
               this->pg0[c][6], this->pg1[c][4], this->pg1[c][5], this->pg1[c][6], this->spage[c]);
    char nm[24];
    for (int a = 0; a < 8; a++)
      for (int r = 0; r < 64; r++) {
        uint32_t *k = this->cnt[a][r];
        if (k[0] | k[1] | k[2] | k[3]) {
          this->name_(this->base | a, r, nm, sizeof(nm));
          ESP_LOGI(TAG, "MAP @%02X r=%02X %-12s W=%u P=%u R=%u WR=%u", this->base | a, r, nm, k[0], k[1], k[2], k[3]);
        }
      }
  }
};

static Bus bus;
}  // namespace wkfake
