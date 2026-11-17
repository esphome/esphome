#pragma once
// Fake SX1276 behind a software SPI bus (ESP32, ESP-IDF). The bus's CLK/MOSI/MISO pins and the radio's CS pin are
// swapped for fake GPIOPin objects at on_boot priority 800 (after the bus setup at 1000, before the radio at 400), so
// the real SPIDelegateBitBash clocks every bit into this register model. DIO0 is a real pad: after the radio's setup
// it is switched to input/output and driven by the model, so the driver's rising edge interrupt fires as on a chip.
// Works with any SX127x driver revision because only public setters are used (set_clk/set_mosi/set_miso/set_cs_pin).
//
// Modelled: banked FSK/LoRa registers (LongRangeMode changes only in sleep), RegOpMode with instant transitions,
// the LoRa 256 byte FIFO with FifoAddrPtr/TxBase/RxBase, the FSK 64 byte FIFO (one FIFO for TX and RX, variable
// length byte first), RegIrqFlags (write 1 to clear) with RX_DONE/TX_DONE, RegIrqFlags2 PayloadReady/PacketSent,
// NbRxBytes/RxCurrAddr/PktRssi/PktSnr, version 0x12, DIO0 mapping 00 (RxDone, FSK PayloadReady/PacketSent) and 01
// (TxDone). A LoRa send ends in standby by itself; FSK stays in TX with PacketSent. While a send is still going out
// the chip ignores any mode request except standby and sleep (the SX1276 stays in TX when asked for RX).
// A read of RegVersion resets the model: the driver reads it first after every pulse of the reset pin.
//
// Faults: fail_mode_writes (the next N RegOpMode writes are ignored, so the mode reads back the old value),
// tx_never (a send never completes), rx_on_standby (a received packet completes just as a send puts the chip in
// standby), version (a wrong chip).
#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include "esphome/components/spi/spi.h"
#include "esphome/components/sx127x/sx127x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "spibits.hpp"

namespace sxsim {
using namespace esphome;
static const char *const TAG = "sxsim";

enum : uint8_t {
  OP_SLEEP = 0,
  OP_STDBY = 1,
  OP_TX = 3,
  OP_RX = 5,
  IRQ_RX_DONE = 0x40,
  IRQ_CRC_ERR = 0x20,
  IRQ_TX_DONE = 0x08,
};

struct Model {
  uint8_t fsk[128]{};   // FSK bank and the common registers
  uint8_t lora[128]{};  // LoRa bank
  uint8_t opmode{0x09};
  uint8_t lfifo[256]{};
  uint8_t ffifo[64]{};
  uint8_t fhead{0}, fcount{0};
  uint8_t irq{0};
  bool payload_ready{false}, packet_sent{false}, tx_active{false};
  uint8_t rx_len{0}, pkt_rssi{100};
  int8_t pkt_snr{20};
  // faults
  uint32_t fail_mode_writes{0};
  bool tx_never{false};
  uint32_t tx_ms{50};
  bool rx_on_standby{false};
  uint8_t pend_rx[64]{};
  uint8_t pend_rx_len{0};
  uint8_t version{0x12};
  // records
  uint8_t last_tx[256]{};
  uint16_t last_tx_len{0};
  uint32_t tx_started{0}, tx_done{0}, tx_aborted{0}, rx_injected{0}, rx_dropped{0}, mode_writes{0},
      mode_ignored_fault{0}, mode_ignored_tx{0}, resets{0}, fifo_underflow{0}, rx_lost_in_fifo{0};
  int dio0_gpio{-1};
  bool drive{false};
  bool dio0{false};
  // trace of SPI transactions
  bool tracing{false};
  char trace[4096]{};
  size_t tlen{0};
  int timer_action{0};  // 1 start, 2 stop; done outside the lock
  esp_timer_handle_t timer{nullptr};
} M;

static portMUX_TYPE MUX = portMUX_INITIALIZER_UNLOCKED;

inline bool lora_mode() { return (M.opmode & 0x80) != 0; }
inline uint8_t mode() { return M.opmode & 0x07; }
inline bool is_common(uint8_t a) { return a == 0x01 || (a >= 0x06 && a <= 0x0C) || a >= 0x40; }
inline uint8_t *bank(uint8_t a) { return (is_common(a) || !lora_mode()) ? &M.fsk[a] : &M.lora[a]; }

inline const char *mode_name() {
  static const char *const NAMES[8] = {"SLEEP", "STDBY", "FSTX", "TX", "FSRX", "RX", "RXSINGLE", "CAD"};
  return NAMES[mode()];
}

inline void ffifo_push(uint8_t v) {
  if (M.fcount < 64) {
    M.ffifo[(M.fhead + M.fcount) % 64] = v;
    M.fcount++;
  }
}
inline uint8_t ffifo_pop() {
  if (M.fcount == 0) {
    M.fifo_underflow++;
    return 0;
  }
  uint8_t v = M.ffifo[M.fhead];
  M.fhead = (M.fhead + 1) % 64;
  M.fcount--;
  return v;
}
inline void ffifo_clear() {
  if (M.payload_ready && M.fcount > 0)
    M.rx_lost_in_fifo++;
  M.fhead = M.fcount = 0;
  M.payload_ready = false;
}

inline void update_dio0() {
  bool lvl;
  if (lora_mode()) {
    const uint8_t map = M.fsk[0x40] & 0xC0;
    lvl = map == 0x40 ? (M.irq & IRQ_TX_DONE) != 0 : map == 0x00 ? (M.irq & IRQ_RX_DONE) != 0 : false;
  } else {
    lvl = mode() == OP_TX ? M.packet_sent : M.payload_ready;
  }
  M.dio0 = lvl;
  if (M.drive)
    gpio_set_level((gpio_num_t) M.dio0_gpio, lvl);
}

inline void abort_tx_locked() {
  if (M.tx_active) {
    M.tx_active = false;
    M.tx_aborted++;
    M.timer_action = 2;
  }
}

inline void reset_locked() {
  abort_tx_locked();
  memset(M.fsk, 0, sizeof(M.fsk));
  memset(M.lora, 0, sizeof(M.lora));
  M.opmode = 0x09;
  M.irq = 0;
  M.fhead = M.fcount = 0;
  M.payload_ready = M.packet_sent = false;
  M.resets++;
}

// A packet completes in the current modulation; dropped unless the chip is receiving
inline void inject_locked(const uint8_t *data, uint8_t len, bool crc_error) {
  if (mode() != OP_RX) {
    M.rx_dropped++;
    return;
  }
  M.rx_injected++;
  if (lora_mode()) {
    const uint8_t base = M.lora[0x0F];
    for (uint8_t i = 0; i < len; i++)
      M.lfifo[(uint8_t) (base + i)] = data[i];
    M.lora[0x10] = base;
    M.rx_len = len;
    M.irq |= IRQ_RX_DONE | 0x10 | (crc_error ? IRQ_CRC_ERR : 0);
  } else if (!crc_error) {
    ffifo_clear();
    if (M.fsk[0x30] & 0x80)
      ffifo_push(len);  // variable length: length byte first
    for (uint8_t i = 0; i < len; i++)
      ffifo_push(data[i]);
    M.payload_ready = true;
  }
  update_dio0();
}

inline void start_tx_locked() {
  M.tx_started++;
  M.last_tx_len = 0;
  if (lora_mode()) {
    const uint8_t base = M.lora[0x0E];
    const uint8_t n = M.lora[0x22];
    for (uint16_t i = 0; i < n; i++)
      M.last_tx[M.last_tx_len++] = M.lfifo[(uint8_t) (base + i)];
  } else {
    uint8_t n;
    if (M.fsk[0x30] & 0x80) {
      n = ffifo_pop();
      M.last_tx[M.last_tx_len++] = n;
    } else {
      n = M.fsk[0x32];
    }
    for (uint16_t i = 0; i < n && M.last_tx_len < sizeof(M.last_tx); i++)
      M.last_tx[M.last_tx_len++] = ffifo_pop();
    if (M.fcount == 0)
      M.payload_ready = false;
  }
  M.tx_active = true;
  M.timer_action = M.tx_never ? 0 : 1;
}

inline void write_mode_locked(uint8_t v) {
  M.mode_writes++;
  if (M.fail_mode_writes > 0) {
    M.fail_mode_writes--;
    M.mode_ignored_fault++;
    return;
  }
  const uint8_t prev = mode(), nm = v & 0x07;
  if (prev == OP_TX && M.tx_active && nm != OP_STDBY && nm != OP_SLEEP && nm != OP_TX) {
    M.mode_ignored_tx++;  // still sending: only standby or sleep stop it
    return;
  }
  if (nm == OP_STDBY && prev == OP_RX && M.rx_on_standby) {
    M.rx_on_standby = false;
    inject_locked(M.pend_rx, M.pend_rx_len, false);  // completed just before the standby took effect
  }
  const uint8_t lr = prev == OP_SLEEP ? (v & 0x80) : (M.opmode & 0x80);
  M.opmode = lr | (v & 0x78) | nm;
  if (prev == OP_TX && nm != OP_TX) {
    abort_tx_locked();
    M.packet_sent = false;
  }
  if (nm == OP_SLEEP || (nm == OP_RX && prev != OP_RX && !lora_mode()))
    ffifo_clear();
  if (nm == OP_TX && prev != OP_TX)
    start_tx_locked();
  update_dio0();
}

inline uint8_t read_locked(uint8_t a) {
  if (a == 0x00) {
    if (lora_mode())
      return M.lfifo[M.lora[0x0D]++];
    uint8_t v = ffifo_pop();
    if (M.fcount == 0)
      M.payload_ready = false;
    return v;
  }
  if (a == 0x01)
    return M.opmode;
  if (a == 0x42) {
    reset_locked();
    return M.version;
  }
  if (lora_mode()) {
    switch (a) {
      case 0x12:
        return M.irq;
      case 0x13:
        return M.rx_len;
      case 0x19:
        return (uint8_t) M.pkt_snr;
      case 0x1A:
        return M.pkt_rssi;
      default:
        break;
    }
  } else {
    if (a == 0x3F)
      return (M.payload_ready ? 0x04 : 0) | (M.packet_sent ? 0x08 : 0) | (M.fcount == 0 ? 0x40 : 0);
    if (a == 0x3B)
      return M.fsk[0x3B] & ~0x20;  // image calibration finishes at once
  }
  return *bank(a);
}

inline void write_locked(uint8_t a, uint8_t v) {
  if (a == 0x00) {
    if (lora_mode())
      M.lfifo[M.lora[0x0D]++] = v;
    else
      ffifo_push(v);
    return;
  }
  if (a == 0x01) {
    write_mode_locked(v);
    return;
  }
  if (lora_mode() && a == 0x12) {
    M.irq &= ~v;
    return;
  }
  *bank(a) = v;
}

// One SPI transaction as the trace shows it: the address byte and up to 16 data bytes
struct TraceTxn {
  uint8_t addr{0}, first{0};
  uint16_t nbytes{0};
  uint8_t bytes[16]{};
  void add(uint8_t v) {
    if (nbytes == 0)
      first = v;
    if (nbytes < 16)
      bytes[nbytes] = v;
    nbytes++;
  }
};

inline void trace_add(const TraceTxn &t) {
  const bool wr = t.addr & 0x80;
  const uint8_t a = t.addr & 0x7F;
  char buf[64];
  int n;
  if (a == 0 && t.nbytes != 1) {
    n = snprintf(buf, sizeof(buf), " %cF[%u]", wr ? 'W' : 'R', t.nbytes);
    for (uint16_t i = 0; i < t.nbytes && i < 16 && n < 56; i++)
      n += snprintf(buf + n, sizeof(buf) - n, "%02X", t.bytes[i]);
  } else {
    n = snprintf(buf, sizeof(buf), " %c%02X%c%02X", wr ? 'W' : 'R', a, wr ? '=' : ':', t.first);
  }
  if (M.tlen + n < sizeof(M.trace) - 1) {
    memcpy(M.trace + M.tlen, buf, n);
    M.tlen += n;
    M.trace[M.tlen] = 0;
  }
}

inline void timer_action() {
  int act;
  portENTER_CRITICAL(&MUX);
  act = M.timer_action;
  M.timer_action = 0;
  portEXIT_CRITICAL(&MUX);
  if (act != 0)
    esp_timer_stop(M.timer);
  if (act == 1)
    ESP_ERROR_CHECK(esp_timer_start_once(M.timer, (uint64_t) M.tx_ms * 1000));
}

inline void tx_timer_cb(void *) {
  portENTER_CRITICAL(&MUX);
  if (M.tx_active && mode() == OP_TX) {
    M.tx_active = false;
    M.tx_done++;
    if (lora_mode()) {
      M.irq |= IRQ_TX_DONE;
      M.opmode = (M.opmode & ~0x07) | OP_STDBY;  // LoRa returns to standby by itself
    } else {
      M.packet_sent = true;
    }
    update_dio0();
  }
  portEXIT_CRITICAL(&MUX);
}

// The SPI side of the model: register reads and writes per byte, the trace capture, and the TX timer after each
// transaction (esp_timer calls assert inside a critical section)
struct SimChip : spibits::SpiChip {
  uint8_t addr{0};
  TraceTxn txn, done;
  bool pending{false};
  void lock() override { portENTER_CRITICAL(&MUX); }
  void unlock() override { portEXIT_CRITICAL(&MUX); }
  uint8_t reply_locked(uint16_t p) override {
    if (p == 0 || (addr & 0x80))
      return 0;
    const uint8_t v = read_locked(addr & 0x7F);
    txn.add(v);
    return v;
  }
  void byte_locked(uint16_t p, uint8_t v) override {
    if (p == 0) {
      addr = txn.addr = v;
    } else if (addr & 0x80) {
      txn.add(v);
      write_locked(addr & 0x7F, v);
    }
  }
  void begin_locked() override { txn.nbytes = 0; }
  void end_locked() override {
    pending = M.tracing && pos > 0;
    if (pending)
      done = txn;
    update_dio0();
  }
  void after_end() override {
    if (pending) {
      pending = false;
      trace_add(done);
    }
    timer_action();
  }
};
static SimChip CHIP;
static spibits::PinSet PINS;

// on_boot priority 800: after the SPI bus setup, before the radio's
inline void install(spi::SPIComponent *bus, sx127x::SX127x *radio, int dio0_gpio) {
  PINS.bind(&CHIP);
  bus->set_clk(&PINS.clk);
  bus->set_mosi(&PINS.mosi);
  bus->set_miso(&PINS.miso);
  radio->set_cs_pin(&PINS.cs);
  M.dio0_gpio = dio0_gpio;
  esp_timer_create_args_t args{};
  args.callback = tx_timer_cb;
  args.name = "sxsim_tx";
  ESP_ERROR_CHECK(esp_timer_create(&args, &M.timer));
  ESP_LOGI(TAG, "fake SX1276 installed");
}

// After the radio's setup(): DIO0 becomes an output the model drives, keeping the input and its interrupt
inline void late_pin_setup() {
  gpio_set_direction((gpio_num_t) M.dio0_gpio, GPIO_MODE_INPUT_OUTPUT);
  portENTER_CRITICAL(&MUX);
  M.drive = true;
  update_dio0();
  portEXIT_CRITICAL(&MUX);
}

inline void inject(std::initializer_list<uint8_t> data, bool crc_error = false) {
  uint8_t buf[64];
  uint8_t n = 0;
  for (auto b : data)
    buf[n++] = b;
  portENTER_CRITICAL(&MUX);
  inject_locked(buf, n, crc_error);
  portEXIT_CRITICAL(&MUX);
}

inline void arm_rx_on_standby(std::initializer_list<uint8_t> data) {
  portENTER_CRITICAL(&MUX);
  M.pend_rx_len = 0;
  for (auto b : data)
    M.pend_rx[M.pend_rx_len++] = b;
  M.rx_on_standby = true;
  portEXIT_CRITICAL(&MUX);
}

inline void trace_start() {
  M.tlen = 0;
  M.trace[0] = 0;
  M.tracing = true;
}
inline void trace_stop() { M.tracing = false; }

inline void log_trace(const char *label) {
  for (size_t off = 0; off < M.tlen; off += 180) {
    char chunk[181];
    size_t n = std::min<size_t>(180, M.tlen - off);
    memcpy(chunk, M.trace + off, n);
    chunk[n] = 0;
    ESP_LOGI("tr", "TRACE %s %u:%s", label, (unsigned) off, chunk);
  }
}

inline void hex(char *out, size_t cap, const uint8_t *d, size_t n) {
  size_t o = 0;
  for (size_t i = 0; i < n && o + 3 < cap; i++)
    o += snprintf(out + o, cap - o, "%02X", d[i]);
  out[o] = 0;
}

inline void log_state(const char *label) {
  char tx[80];
  hex(tx, sizeof(tx), M.last_tx, std::min<size_t>(M.last_tx_len, 32));
  ESP_LOGI(TAG,
           "STATE %s mode=%s lora=%d dio0=%d irq=%02X fsk_fifo=%u tx_started=%u tx_done=%u tx_aborted=%u last_tx=[%s] "
           "rx_inj=%u rx_drop=%u rx_lost=%u mode_writes=%u ign_fault=%u ign_tx=%u resets=%u underflow=%u",
           label, mode_name(), lora_mode(), M.dio0, M.irq, M.fcount, (unsigned) M.tx_started, (unsigned) M.tx_done,
           (unsigned) M.tx_aborted, tx, (unsigned) M.rx_injected, (unsigned) M.rx_dropped, (unsigned) M.rx_lost_in_fifo,
           (unsigned) M.mode_writes, (unsigned) M.mode_ignored_fault, (unsigned) M.mode_ignored_tx, (unsigned) M.resets,
           (unsigned) M.fifo_underflow);
}
}  // namespace sxsim
