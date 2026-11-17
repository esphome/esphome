#pragma once
// Several fake radios in one ESP32 firmware, joined by a simulated air (esphome#21110). Each radio has its own software
// SPI bus whose CLK/MOSI/MISO and the radio's CS (and reset, busy) are fake GPIOPins swapped in at on_boot priority
// 1100, before the bus's setup, so no SPI pad is ever driven. Only the interrupt pad (SX127x DIO0, SX126x DIO1) is
// real: it is switched to input/output after the radio's setup and driven by the model, so the driver's rising edge
// interrupt fires.
//
// Air: a packet one radio sends lands, after its airtime (worked out from the sender's registers or commands), in every
// other radio that was receiving in the same modulation for the whole airtime, with the receiver's rssi/snr knobs.
// While it is on air each receiver shows a signal: SX127x LoRa RegModemStat (signal detected, synchronized, header
// valid), SX127x FSK RegIrqFlags1 SyncAddressMatch (kept until the packet is read), SX126x IRQ preamble/sync/header
// valid (when in the IRQ mask). Two overlapping senders collide: receivers get a CRC error. Knobs: drop_next (packets
// nobody hears), corrupt_next (CRC error at the receiver), per radio jam (a signal with no packet, for ms or forever),
// tx_never. Counters: tx_over_signal counts sends started while that radio showed a signal (what listen before talk
// prevents).
#include <driver/gpio.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <cmath>
#include "esphome/components/spi/spi.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "spibits.hpp"

namespace sxair {
using namespace esphome;
static const char *const TAG = "sxair";
static portMUX_TYPE MUX = portMUX_INITIALIZER_UNLOCKED;

inline uint32_t now_us() { return (uint32_t) esp_timer_get_time(); }

struct Radio;
struct Air {
  Radio *r[4]{};
  int n{0};
  uint32_t drop_next{0}, corrupt_next{0};
  uint32_t dropped{0}, corrupted{0}, collisions{0};
  esp_timer_handle_t timer{nullptr};
  void begin_tx_locked(Radio *x, uint16_t len);
  void end_tx_locked(Radio *x);
  void abort_tx_locked(Radio *x);
  void left_rx_locked(Radio *y);
} AIR;

inline uint32_t lora_airtime_us(uint8_t sf, uint32_t bw_hz, uint8_t cr, uint16_t preamble, bool crc, bool implicit,
                                bool ldro, uint16_t len) {
  const double ts = (double) (1u << sf) / bw_hz;
  const int de = ldro ? 1 : 0;
  double num = 8.0 * len - 4.0 * sf + 28 + (crc ? 16 : 0) - (implicit ? 20 : 0);
  double sym = 8 + std::max(std::ceil(num / (4.0 * (sf - 2 * de))) * (cr + 4), 0.0);
  return (uint32_t) ((preamble + 4.25 + sym) * ts * 1e6);
}

struct Radio {
  const char *name{"?"};
  int dio_gpio{-1};
  bool drive{false}, dio{false};
  // sending
  bool on_air{false};
  uint32_t air_end_us{0};
  bool tx_never{false};
  // receiving
  int incoming{-1};  // index of the sender being heard
  bool collided{false}, left_mid{false};
  bool jam{false};
  uint32_t jam_until_us{0};  // 0: until cleared
  float rssi{-80.0f}, snr{7.25f};
  int index{0};
  // counters
  uint32_t tx{0}, rx_ok{0}, rx_crc{0}, rx_collision{0}, rx_left{0}, rx_too_long{0}, tx_over_signal{0}, tx_aborted{0};
  uint16_t last_tx_len{0}, last_rx_len{0};
  // Both radios share one main loop, so a packet that lands while the previous one is still unread (the loop was busy
  // sending on the other radio) is held here and handed over once the driver has read the previous one. A real peer
  // reads its own radio in time; this keeps the shared loop from losing packets that two devices would not.
  static constexpr int QN = 8;
  uint8_t q[QN][256];
  uint16_t qlen[QN]{};
  bool qcrc[QN]{};
  int qhead{0}, qcount{0};
  uint32_t queued{0}, queue_full{0};
  uint32_t last_spi_us{0};  // a held packet is handed over only after the driver has finished its reads
  uint32_t last_tx_air_us{0}, last_tx_end_ms{0};
  // the last send started while this radio showed a signal: when, and how long the signal had been showing
  uint32_t over_ms{0}, over_signal_age_us{0}, signal_since_us{0};
  int over_from{-1};
  bool over_jam{false};

  virtual bool listening() = 0;
  virtual bool lora() = 0;
  virtual uint32_t airtime_us(uint16_t len) = 0;
  virtual bool crc_on() = 0;
  virtual void show_signal_locked(bool on) = 0;
  virtual bool signal_visible() = 0;
  virtual void tx_done_locked() = 0;
  virtual void update_dio_locked() = 0;
  virtual bool unread_locked() = 0;
  virtual void put_locked(const uint8_t *data, uint16_t len, bool crc_error) = 0;
  void deliver_locked(const uint8_t *data, uint16_t len, bool crc_error) {
    if (this->unread_locked() || this->qcount > 0) {
      if (this->qcount == QN) {
        this->queue_full++;
        return;
      }
      int i = (this->qhead + this->qcount) % QN;
      memcpy(this->q[i], data, len);
      this->qlen[i] = len;
      this->qcrc[i] = crc_error;
      this->qcount++;
      this->queued++;
      this->show_signal_locked(this->jam);  // the packet is over; it waits in the hold queue
      return;
    }
    this->put_locked(data, len, crc_error);
  }
  // After every SPI transaction: hand over a held packet once the previous one is read
  void drain_locked() {
    if (this->qcount == 0 || this->unread_locked() || !this->listening())
      return;
    int i = this->qhead;
    this->qhead = (this->qhead + 1) % QN;
    this->qcount--;
    this->put_locked(this->q[i], this->qlen[i], this->qcrc[i]);
  }
  void refresh_signal_locked() {
    const bool on = this->jam || this->incoming >= 0;
    if (on && !this->signal_visible())
      this->signal_since_us = now_us();
    this->show_signal_locked(on);
  }
  void set_dio_locked(bool level) {
    this->dio = level;
    if (this->drive)
      gpio_set_level((gpio_num_t) this->dio_gpio, level);
  }
};

inline void Air::begin_tx_locked(Radio *x, uint16_t len) {
  x->tx++;
  x->last_tx_len = len;
  if (x->signal_visible()) {
    x->tx_over_signal++;
    x->over_ms = millis();
    x->over_signal_age_us = now_us() - x->signal_since_us;
    x->over_from = x->incoming;
    x->over_jam = x->jam;
  }
  if (x->incoming >= 0)
    x->left_mid = true;  // it stops hearing the packet it was receiving
  x->on_air = true;
  x->last_tx_air_us = x->airtime_us(len);
  x->air_end_us = now_us() + (x->tx_never ? 0x7FFFFFFF : x->last_tx_air_us);
  bool heard = true;
  if (this->drop_next > 0) {
    this->drop_next--;
    this->dropped++;
    heard = false;
  }
  for (int i = 0; i < this->n; i++) {
    Radio *y = this->r[i];
    if (y == x)
      continue;
    if (y->incoming >= 0) {
      y->collided = true;  // two senders at once
      this->collisions++;
      continue;
    }
    if (!heard || y->on_air || !y->listening() || y->lora() != x->lora())
      continue;
    y->incoming = x->index;
    y->collided = y->left_mid = false;
    y->refresh_signal_locked();
  }
}

inline void Air::end_tx_locked(Radio *x) {
  x->on_air = false;
  x->last_tx_end_ms = millis();
  x->tx_done_locked();
}

inline void Air::abort_tx_locked(Radio *x) {
  if (!x->on_air)
    return;
  x->on_air = false;
  x->tx_aborted++;
  for (int i = 0; i < this->n; i++) {
    Radio *y = this->r[i];
    if (y->incoming == x->index) {
      y->incoming = -1;
      y->refresh_signal_locked();
    }
  }
}

inline void Air::left_rx_locked(Radio *y) {
  if (y->incoming >= 0)
    y->left_mid = true;
}

// The sender's bytes, kept per sender for delivery at the end of the airtime
static uint8_t TX_PAYLOAD[4][256];

inline void air_tick(void *) {
  portENTER_CRITICAL(&MUX);
  const uint32_t t = now_us();
  for (int i = 0; i < AIR.n; i++) {
    Radio *x = AIR.r[i];
    if (x->jam && x->jam_until_us != 0 && (int32_t) (t - x->jam_until_us) >= 0) {
      x->jam = false;
      x->refresh_signal_locked();
    }
    if ((int32_t) (t - x->last_spi_us) > 3000)
      x->drain_locked();
    if (!x->on_air || (int32_t) (t - x->air_end_us) < 0)
      continue;
    AIR.end_tx_locked(x);
    for (int j = 0; j < AIR.n; j++) {
      Radio *y = AIR.r[j];
      if (y->incoming != x->index)
        continue;
      y->incoming = -1;
      if (y->collided) {
        y->rx_collision++;
        y->collided = false;
        y->refresh_signal_locked();
        if (y->listening())
          y->deliver_locked(TX_PAYLOAD[i], x->last_tx_len, true);
      } else if (y->left_mid || !y->listening()) {
        y->rx_left++;
        y->left_mid = false;
        y->refresh_signal_locked();
      } else {
        bool bad = false;
        if (AIR.corrupt_next > 0) {
          AIR.corrupt_next--;
          AIR.corrupted++;
          bad = true;
        }
        y->deliver_locked(TX_PAYLOAD[i], x->last_tx_len, bad);
      }
    }
  }
  portEXIT_CRITICAL(&MUX);
}

inline void start_tx_locked(Radio *x, const uint8_t *data, uint16_t len) {
  memcpy(TX_PAYLOAD[x->index], data, len);
  AIR.begin_tx_locked(x, len);
}

using spibits::NopPin;
using spibits::PinSet;

// A model behind fake SPI pins, its hooks run under the air's lock
struct AirChip : spibits::SpiChip {
  void lock() override { portENTER_CRITICAL(&MUX); }
  void unlock() override { portEXIT_CRITICAL(&MUX); }
};

// ---------------------------------------------------------------- SX127x
struct Sx127x : Radio, AirChip {
  uint8_t fsk[128]{}, lr[128]{};
  uint8_t opmode{0x09};
  uint8_t lfifo[256]{};
  uint8_t ffifo[64]{};
  uint8_t fhead{0}, fcount{0};
  uint8_t irq{0};
  bool payload_ready{false}, packet_sent{false}, tx_active{false}, sync_hold{false}, signal{false};
  uint8_t rx_len{0}, raw_rssi{0};
  int8_t raw_snr{0};
  uint8_t addr{0};

  bool is_lora_mode() const { return (opmode & 0x80) != 0; }
  uint8_t mode() const { return opmode & 7; }
  static bool common(uint8_t a) { return a == 0x01 || (a >= 0x06 && a <= 0x0C) || a >= 0x40; }
  uint8_t *bank(uint8_t a) { return (common(a) || !is_lora_mode()) ? &fsk[a] : &lr[a]; }

  bool listening() override { return mode() == 5; }
  bool lora() override { return is_lora_mode(); }
  bool crc_on() override { return is_lora_mode() ? (lr[0x1E] & 0x04) != 0 : (fsk[0x30] & 0x10) != 0; }
  uint32_t freq_hz() { return (uint32_t) ((((uint64_t) fsk[6] << 16) | (fsk[7] << 8) | fsk[8]) * 32000000ULL >> 19); }
  uint32_t airtime_us(uint16_t len) override {
    if (is_lora_mode()) {
      static const uint32_t BW[10] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};
      uint8_t bwc = lr[0x1D] >> 4;
      return lora_airtime_us(lr[0x1E] >> 4, BW[bwc < 10 ? bwc : 7], (lr[0x1D] >> 1) & 7, (lr[0x20] << 8) | lr[0x21],
                             crc_on(), lr[0x1D] & 1, (lr[0x26] >> 3) & 1, len);
    }
    uint32_t br = 32000000u / std::max<uint32_t>(1, (fsk[2] << 8) | fsk[3]);
    uint32_t bytes = ((fsk[0x25] << 8) | fsk[0x26]) + ((fsk[0x27] & 0x10) ? (fsk[0x27] & 7) + 1 : 0) +
                     ((fsk[0x30] & 0x80) ? 1 : 0) + len + (crc_on() ? 2 : 0);
    return (uint32_t) ((uint64_t) bytes * 8 * 1000000 / br);
  }
  void show_signal_locked(bool on) override {
    signal = on;
    if (!on && !payload_ready)
      sync_hold = false;
  }
  bool signal_visible() override { return signal || (!is_lora_mode() && sync_hold); }

  void update_dio_locked() override {
    bool lvl;
    if (is_lora_mode()) {
      const uint8_t map = fsk[0x40] & 0xC0;
      lvl = map == 0x40 ? (irq & 0x08) != 0 : map == 0x00 ? (irq & 0x40) != 0 : false;
    } else {
      lvl = mode() == 3 ? packet_sent : payload_ready;
    }
    set_dio_locked(lvl);
  }

  void ffifo_push(uint8_t v) {
    if (fcount < 64) {
      ffifo[(fhead + fcount) % 64] = v;
      fcount++;
    }
  }
  uint8_t ffifo_pop() {
    if (fcount == 0)
      return 0;
    uint8_t v = ffifo[fhead];
    fhead = (fhead + 1) % 64;
    fcount--;
    return v;
  }
  void ffifo_clear() {
    fhead = fcount = 0;
    payload_ready = false;
    sync_hold = false;
  }

  bool unread_locked() override { return is_lora_mode() ? (irq & 0x40) != 0 : payload_ready; }
  void put_locked(const uint8_t *data, uint16_t len, bool crc_error) override {
    signal = jam;
    if (is_lora_mode()) {
      const uint8_t base = lr[0x0F];
      for (uint16_t i = 0; i < len; i++)
        lfifo[(uint8_t) (base + i)] = data[i];
      lr[0x10] = base;
      rx_len = len;
      const int off = freq_hz() > 700000000 ? 157 : 164;
      raw_rssi = (uint8_t) std::lround(rssi + off);
      raw_snr = (int8_t) std::lround(snr * 4);
      irq |= 0x40 | 0x10 | (crc_error ? 0x20 : 0);
      if (crc_error)
        rx_crc++;
      else
        rx_ok++;
      last_rx_len = len;
    } else {
      const bool variable = fsk[0x30] & 0x80;
      if (crc_error && crc_on()) {
        rx_crc++;  // the chip drops it and restarts RX
        sync_hold = false;
      } else if (variable && len > fsk[0x32]) {
        rx_too_long++;
        sync_hold = false;
      } else {
        ffifo_clear();
        if (variable)
          ffifo_push(len);
        for (uint16_t i = 0; i < len; i++)
          ffifo_push(crc_error ? data[i] ^ 0x5A : data[i]);
        payload_ready = true;
        sync_hold = true;
        rx_ok++;
        last_rx_len = len;
      }
    }
    update_dio_locked();
  }

  void tx_done_locked() override {
    if (!tx_active || mode() != 3)
      return;
    tx_active = false;
    if (is_lora_mode()) {
      irq |= 0x08;
      opmode = (opmode & ~7) | 1;
    } else {
      packet_sent = true;
    }
    update_dio_locked();
  }

  void reset_locked() {
    AIR.abort_tx_locked(this);
    memset(fsk, 0, sizeof(fsk));
    memset(lr, 0, sizeof(lr));
    opmode = 0x09;
    irq = 0;
    fhead = fcount = 0;
    payload_ready = packet_sent = tx_active = sync_hold = false;
  }

  void start_tx_locked_() {
    uint8_t buf[256];
    uint16_t n = 0;
    if (is_lora_mode()) {
      const uint8_t base = lr[0x0E];
      const uint8_t len = lr[0x22];
      for (uint16_t i = 0; i < len; i++)
        buf[n++] = lfifo[(uint8_t) (base + i)];
    } else {
      uint8_t len = (fsk[0x30] & 0x80) ? ffifo_pop() : fsk[0x32];
      for (uint16_t i = 0; i < len; i++)
        buf[n++] = ffifo_pop();
      if (fcount == 0)
        payload_ready = false;
    }
    tx_active = true;
    start_tx_locked(this, buf, n);
  }

  void write_mode_locked(uint8_t v) {
    const uint8_t prev = mode(), nm = v & 7;
    if (prev == 3 && tx_active && nm != 1 && nm != 0 && nm != 3)
      return;  // still sending: only standby or sleep stop it
    const uint8_t lrbit = prev == 0 ? (v & 0x80) : (opmode & 0x80);
    opmode = lrbit | (v & 0x78) | nm;
    if (prev == 5 && nm != 5)
      AIR.left_rx_locked(this);
    if (prev == 3 && nm != 3) {
      if (tx_active) {
        tx_active = false;
        AIR.abort_tx_locked(this);
      }
      packet_sent = false;
    }
    if (nm == 0 || (nm == 5 && prev != 5 && !is_lora_mode()))
      ffifo_clear();
    if (nm == 3 && prev != 3)
      start_tx_locked_();
    update_dio_locked();
  }

  uint8_t read_locked(uint8_t a) {
    if (a == 0x00) {
      if (is_lora_mode())
        return lfifo[lr[0x0D]++];
      uint8_t v = ffifo_pop();
      if (fcount == 0) {
        payload_ready = false;
        sync_hold = false;
      }
      return v;
    }
    if (a == 0x01)
      return opmode;
    if (a == 0x42) {
      reset_locked();
      return 0x12;
    }
    if (is_lora_mode()) {
      switch (a) {
        case 0x12:
          return irq;
        case 0x13:
          return rx_len;
        case 0x18:
          return signal ? 0x0B : 0x00;  // signal detected, synchronized, header valid
        case 0x19:
          return (uint8_t) raw_snr;
        case 0x1A:
          return raw_rssi;
        default:
          break;
      }
    } else {
      if (a == 0x3E)
        return 0x80 | ((signal || sync_hold) ? 0x03 : 0x00);
      if (a == 0x3F)
        return (payload_ready ? 0x04 : 0) | (packet_sent ? 0x08 : 0) | (fcount == 0 ? 0x40 : 0);
      if (a == 0x3B)
        return fsk[0x3B] & ~0x20;
    }
    return *bank(a);
  }

  void write_locked(uint8_t a, uint8_t v) {
    if (a == 0x00) {
      if (is_lora_mode())
        lfifo[lr[0x0D]++] = v;
      else
        ffifo_push(v);
      return;
    }
    if (a == 0x01) {
      write_mode_locked(v);
      return;
    }
    if (is_lora_mode() && a == 0x12) {
      irq &= ~v;
      return;
    }
    *bank(a) = v;
  }

  uint8_t reply_locked(uint16_t p) override {
    if (p == 0 || (addr & 0x80))
      return 0;
    return read_locked(addr & 0x7F);
  }
  void byte_locked(uint16_t p, uint8_t v) override {
    if (p == 0)
      addr = v;
    else if (addr & 0x80)
      write_locked(addr & 0x7F, v);
  }
  void end_locked() override {
    last_spi_us = now_us();
    update_dio_locked();
  }
};

// ---------------------------------------------------------------- SX126x
struct Sx126x : Radio, AirChip {
  uint8_t buf[256]{};
  uint8_t cmd[264]{};
  uint16_t ncmd{0};
  uint8_t pkt_type{0}, mode{1};  // 0 sleep, 1 standby, 3 tx, 4 rx
  uint16_t irq{0}, irq_mask{0}, dio1_mask{0};
  uint8_t sf{7}, bw{4}, cr{1}, ldro{0};
  uint16_t preamble{8};
  bool implicit{false}, crc{false}, fsk_var{true};
  uint8_t plen{0}, sync_bits{0};
  uint32_t bitrate{4800};
  uint8_t rx_len{0};
  bool signal{false};
  uint8_t pkt_rssi{0};
  int8_t pkt_snr{0};

  bool listening() override { return mode == 4; }
  bool lora() override { return pkt_type == 1; }
  bool crc_on() override { return crc; }
  uint32_t airtime_us(uint16_t len) override {
    if (pkt_type == 1) {
      uint32_t hz;
      switch (bw) {
        case 0x00:
          hz = 7810;
          break;
        case 0x08:
          hz = 10420;
          break;
        case 0x01:
          hz = 15630;
          break;
        case 0x09:
          hz = 20830;
          break;
        case 0x02:
          hz = 31250;
          break;
        case 0x0A:
          hz = 41670;
          break;
        case 0x03:
          hz = 62500;
          break;
        case 0x05:
          hz = 250000;
          break;
        case 0x06:
          hz = 500000;
          break;
        default:
          hz = 125000;
          break;
      }
      return lora_airtime_us(sf, hz, cr, preamble, crc, implicit, ldro, len);
    }
    uint32_t bytes = preamble / 8 + sync_bits / 8 + (fsk_var ? 1 : 0) + len + (crc ? 2 : 0);
    return (uint32_t) ((uint64_t) bytes * 8 * 1000000 / std::max<uint32_t>(1, bitrate));
  }
  void raise_locked(uint16_t bits) { irq |= bits & irq_mask; }
  void show_signal_locked(bool on) override {
    signal = on;
    if (on)
      raise_locked(0x0004 | 0x0008 | 0x0010);  // preamble, sync word, header valid
    else
      irq &= ~(0x0004 | 0x0008 | 0x0010);
    update_dio_locked();
  }
  bool signal_visible() override { return signal; }
  void update_dio_locked() override { set_dio_locked((irq & dio1_mask) != 0); }
  bool unread_locked() override { return (irq & 0x0002) != 0; }
  void put_locked(const uint8_t *data, uint16_t len, bool crc_error) override {
    signal = jam;
    memcpy(buf, data, len);
    rx_len = len;
    pkt_rssi = (uint8_t) std::lround(-rssi * 2);
    pkt_snr = (int8_t) std::lround(snr * 4);
    raise_locked(0x0002 | (crc_error ? 0x0040 : 0));
    if (crc_error)
      rx_crc++;
    else
      rx_ok++;
    last_rx_len = len;
    update_dio_locked();
  }
  void tx_done_locked() override {
    if (mode != 3)
      return;
    mode = 1;
    raise_locked(0x0001);
    update_dio_locked();
  }
  void reset_locked() {
    AIR.abort_tx_locked(this);
    mode = 1;
    irq = irq_mask = dio1_mask = 0;
    pkt_type = 0;
    update_dio_locked();
  }
  void set_mode_locked(uint8_t m) {
    if (mode == 4 && m != 4)
      AIR.left_rx_locked(this);
    if (mode == 3 && m != 3)
      AIR.abort_tx_locked(this);
    mode = m;
  }
  uint8_t reply_locked(uint16_t p) override {
    if (p == 0)
      return 0x22;
    const uint8_t op = cmd[0];
    switch (op) {
      case 0x1D: {  // ReadRegister: op, addr hi, addr lo, status, data
        if (p < 4)
          return 0x22;
        const uint16_t reg = ((cmd[1] << 8) | cmd[2]) + (p - 4);
        static const char VERSION[] = "SX1261 V2D 2D02";
        if (reg >= 0x0320 && reg < 0x0320 + 16)
          return (uint8_t) VERSION[reg - 0x0320];
        return 0;
      }
      case 0x1E:  // ReadBuffer: op, offset, status, data
        return p < 3 ? 0x22 : buf[(uint8_t) (cmd[1] + p - 3)];
      case 0x12:
        return p == 2 ? irq >> 8 : p == 3 ? irq & 0xFF : 0x22;
      case 0x13:
        return p == 2 ? rx_len : p == 3 ? 0 : 0x22;
      case 0x14:
        if (pkt_type == 1)
          return p == 2 ? pkt_rssi : p == 3 ? (uint8_t) pkt_snr : p == 4 ? pkt_rssi : 0x22;
        return p == 2 ? 0 : (p == 3 || p == 4) ? pkt_rssi : 0x22;
      default:
        return 0x22;
    }
  }
  void byte_locked(uint16_t p, uint8_t v) override {
    if (p < sizeof(cmd))
      cmd[p] = v;
    ncmd = p + 1;
  }
  void begin_locked() override { ncmd = 0; }
  void end_locked() override {
    const uint16_t n = ncmd;
    switch (cmd[0]) {
      case 0x80:
        set_mode_locked(1);
        break;
      case 0x84:
        set_mode_locked(0);
        break;
      case 0x82:
        set_mode_locked(4);
        break;
      case 0x83:
        if (mode != 3) {
          if (mode == 4)
            AIR.left_rx_locked(this);
          mode = 3;
          start_tx_locked(this, buf, plen);
        }
        break;
      case 0x8A:
        pkt_type = cmd[1];
        break;
      case 0x8B:
        if (pkt_type == 1) {
          sf = cmd[1];
          bw = cmd[2];
          cr = cmd[3];
          ldro = cmd[4];
        } else {
          uint32_t raw = (cmd[1] << 16) | (cmd[2] << 8) | cmd[3];
          bitrate = raw ? (uint32_t) (32ULL * 32000000ULL / raw) : 4800;
        }
        break;
      case 0x8C:
        preamble = (cmd[1] << 8) | cmd[2];
        if (pkt_type == 1) {
          implicit = cmd[3];
          plen = cmd[4];
          crc = cmd[5];
        } else {
          sync_bits = cmd[4];
          fsk_var = cmd[6];
          plen = cmd[7];
          crc = cmd[8] != 0x01;
        }
        break;
      case 0x08:
        irq_mask = (cmd[1] << 8) | cmd[2];
        dio1_mask = (cmd[3] << 8) | cmd[4];
        break;
      case 0x02:
        irq &= ~((cmd[1] << 8) | cmd[2]);
        break;
      case 0x0E:
        for (uint16_t i = 2; i < n; i++)
          buf[(uint8_t) (cmd[1] + i - 2)] = cmd[i];
        break;
      default:
        break;
    }
    last_spi_us = now_us();
    update_dio_locked();
  }
};

class Sx126xRst : public NopPin {
 public:
  Sx126x *m{nullptr};
  void digital_write(bool v) override {
    if (!v) {
      portENTER_CRITICAL(&MUX);
      m->reset_locked();
      portEXIT_CRITICAL(&MUX);
    }
  }
};

// ---------------------------------------------------------------- install helpers
inline void start_air() {
  if (AIR.timer != nullptr)
    return;
  esp_timer_create_args_t args{};
  args.callback = air_tick;
  args.name = "sxair";
  esp_timer_create(&args, &AIR.timer);
  esp_timer_start_periodic(AIR.timer, 1000);
}

inline void add_radio(Radio *r, const char *name, int dio_gpio) {
  r->name = name;
  r->dio_gpio = dio_gpio;
  r->index = AIR.n;
  AIR.r[AIR.n++] = r;
}

// on_boot priority 1100: before the SPI buses' setup
template<typename R>
inline void install_sx127x(spi::SPIComponent *bus, R *radio, Sx127x *m, PinSet *p, const char *name, int dio_gpio) {
  p->bind(m);
  bus->set_clk(&p->clk);
  bus->set_mosi(&p->mosi);
  bus->set_miso(&p->miso);
  radio->set_cs_pin(&p->cs);
  radio->set_rst_pin(&p->rst);
  add_radio(m, name, dio_gpio);
  start_air();
}

template<typename R>
inline void install_sx126x(spi::SPIComponent *bus, R *radio, Sx126x *m, PinSet *p, Sx126xRst *rst, const char *name,
                           int dio_gpio) {
  p->bind(m);
  rst->m = m;
  bus->set_clk(&p->clk);
  bus->set_mosi(&p->mosi);
  bus->set_miso(&p->miso);
  radio->set_cs_pin(&p->cs);
  radio->set_rst_pin(rst);
  radio->set_busy_pin(&p->busy);
  add_radio(m, name, dio_gpio);
  start_air();
}

// After the radios' setup: the interrupt pads become outputs the models drive
inline void late_pins() {
  for (int i = 0; i < AIR.n; i++) {
    gpio_set_direction((gpio_num_t) AIR.r[i]->dio_gpio, GPIO_MODE_INPUT_OUTPUT);
    portENTER_CRITICAL(&MUX);
    AIR.r[i]->drive = true;
    AIR.r[i]->update_dio_locked();
    portEXIT_CRITICAL(&MUX);
  }
}

inline void jam(Radio *r, uint32_t ms) {
  portENTER_CRITICAL(&MUX);
  r->jam = ms != 0;
  r->jam_until_us = (ms == 0 || ms == UINT32_MAX) ? 0 : now_us() + ms * 1000;
  r->refresh_signal_locked();
  portEXIT_CRITICAL(&MUX);
}
inline void unjam(Radio *r) {
  portENTER_CRITICAL(&MUX);
  r->jam = false;
  r->refresh_signal_locked();
  portEXIT_CRITICAL(&MUX);
}

inline void log_radio(const char *label, Radio *r) {
  ESP_LOGI(TAG,
           "AIR %s %s tx=%u last_tx_len=%u last_air=%uus rx_ok=%u rx_crc=%u rx_coll=%u rx_left=%u rx_too_long=%u "
           "tx_over_signal=%u tx_aborted=%u on_air=%d listening=%d signal=%d dio=%d held=%u held_full=%u | dropped=%u "
           "corrupted=%u collisions=%u | last over-signal send at %ums, signal %uus old, from %d, jam %d",
           label, r->name, (unsigned) r->tx, r->last_tx_len, (unsigned) r->last_tx_air_us, (unsigned) r->rx_ok,
           (unsigned) r->rx_crc, (unsigned) r->rx_collision, (unsigned) r->rx_left, (unsigned) r->rx_too_long,
           (unsigned) r->tx_over_signal, (unsigned) r->tx_aborted, r->on_air, r->listening(), r->signal_visible(),
           r->dio, (unsigned) r->queued, (unsigned) r->queue_full, (unsigned) AIR.dropped, (unsigned) AIR.corrupted,
           (unsigned) AIR.collisions, (unsigned) r->over_ms, (unsigned) r->over_signal_age_us, r->over_from,
           r->over_jam);
}
}  // namespace sxair
