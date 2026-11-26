#pragma once
// SPI capture on the same board, no chips or wiring (#22376). Call add() per device, start() once, tick() every 20 ms.
// Classic ESP32: SPI3 as a non-DMA slave fed from the master's pads through the GPIO matrix, answering on MISO.
// Other boards: GPIO edge interrupts on CLK and CS take snapshots of the input register (low data rates only).
#include <cstring>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/components/spi_device/spi_device.h"
#ifdef USE_ESP32
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "soc/spi_periph.h"
#include "soc/gpio_reg.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#endif
#if defined(USE_ESP32_VARIANT_ESP32)
#define SPITEST_SLAVE 1
#include "driver/spi_slave.h"
#endif
#if defined(USE_ESP8266) || defined(USE_RP2)
#include <Arduino.h>
#endif
#ifdef USE_RP2
#include "hardware/structs/sio.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/timer.h"
#endif

#ifdef USE_RP2
#define SPITEST_ISR
#else
#define SPITEST_ISR IRAM_ATTR
#endif

namespace spitest {
using esphome::spi_device::SPIDeviceComponent;
static const char *const TAG = "spitest";

struct Dev {
  const char *name;
  SPIDeviceComponent *d;
  uint8_t mode;
  bool lsb;
  int cs, clk, mosi, miso;
  bool hw;
  uint8_t tag;
};
static Dev devs[8];
static int ndev = 0;
static esphome::PollingComponent *mx_ = nullptr;
static int mx_cs_ = -1, mx_clk_ = -1, mx_mosi_ = -1;
static int hw_mosi_ = -1, hw_miso_ = -1;  // for the read path tricks

inline void add(const char *name, SPIDeviceComponent *d, int mode, bool lsb, int cs, int clk, int mosi, int miso,
                bool hw) {
  devs[ndev] = Dev{name, d, (uint8_t) mode, lsb, cs, clk, mosi, miso, hw, (uint8_t) (0x10 * (ndev + 1))};
  if (hw) {
    hw_mosi_ = mosi;
    hw_miso_ = miso;
  }
  ndev++;
}
inline void add_mx(esphome::PollingComponent *mx, int cs, int clk, int mosi) {
  mx_ = mx;
  mx_cs_ = cs;
  mx_clk_ = clk;
  mx_mosi_ = mosi;
}

inline void hex(const uint8_t *b, size_t n, char *out) {
  for (size_t i = 0; i < n; i++)
    sprintf(out + 2 * i, "%02x", b[i]);
  out[2 * n] = 0;
  if (n == 0)
    strcpy(out, "-");
}

static char boot_heap_[96];
inline void log_heap(const char *what) {
  char *b = strcmp(what, "boot") == 0 ? boot_heap_ : nullptr;
  char tmp[96];
  if (b == nullptr)
    b = tmp;
#ifdef USE_ESP32
  snprintf(b, 96, "HEAP %s free=%u total=%u used=%u minfree=%u", what,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (unsigned) heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
           (unsigned) (heap_caps_get_total_size(MALLOC_CAP_DEFAULT) - heap_caps_get_free_size(MALLOC_CAP_DEFAULT)),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
#elif defined(USE_ESP8266)
  uint32_t total = 0x3FFFC000u - (uint32_t) &_heap_start[0];
  uint32_t fr = ESP.getFreeHeap();
  snprintf(b, 96, "HEAP %s free=%u total=%u used=%u", what, (unsigned) fr, (unsigned) total, (unsigned) (total - fr));
#elif defined(USE_RP2)
  snprintf(b, 96, "HEAP %s free=%u total=%u used=%u", what, (unsigned) rp2040.getFreeHeap(),
           (unsigned) rp2040.getTotalHeap(), (unsigned) rp2040.getUsedHeap());
#endif
  ESP_LOGI(TAG, "%s", b);
}

// One operation on a device. Returns elapsed us; rx/rxn get what the master read.
static int clk_before_ = -1, clk_after_ = -1;
static char regs_[64] = "";
inline int read_level(int pin);
inline uint32_t do_op(Dev &v, int step, uint8_t *rx, size_t &rxn) {
  auto *d = v.d;
  uint8_t buf[8] = {0x01, 0x80, 0xA5, 0x3C, 0xF0, 0x0F, 0x55, v.tag};
  rxn = 0;
  uint32_t t0 = (esphome::micros) ();
  d->enable();
  clk_before_ = read_level(v.clk);
  switch (step) {
    case 0:
    case 5:
    case 6:
    case 8:
      d->transfer_array(buf, 8);
      memcpy(rx, buf, 8);
      rxn = 8;
      break;
#ifdef USE_ESP32
    case 9: {  // same loopback read into a word aligned DMA capable buffer
      static uint8_t *dma = (uint8_t *) heap_caps_malloc(8, MALLOC_CAP_DMA);
      memcpy(dma, buf, 8);
      d->transfer_array(dma, 8);
      memcpy(rx, dma, 8);
      rxn = 8;
      break;
    }
#endif
    case 1:
      d->write_byte16(0x1234 ^ v.tag);
      break;
    case 2:
      d->write(0x5A5, 12);
      break;
    case 3:
      memset(rx, 0x99, 4);
      d->read_array(rx, 4);
      rxn = 4;
      break;
    case 4: {
      uint8_t w[5] = {0xDE, 0xAD, 0xBE, 0xEF, v.tag};
      d->write_array(w, 5);
      break;
    }
  }
  clk_after_ = read_level(v.clk);
  d->disable();
  uint32_t t1 = (esphome::micros) ();
#ifdef USE_ESP8266
  // The HSPI pads are not readable, so record what beginTransaction() programmed; endTransaction() leaves it.
  if (v.hw)
    snprintf(regs_, sizeof(regs_), " clk=%08x c=%08x u=%08x p=%08x", (unsigned) SPI1CLK, (unsigned) SPI1C,
             (unsigned) SPI1U, (unsigned) SPI1P);
#endif
  return t1 - t0;
}

inline void lifecycle(Dev &v, int step) {
  if (step == 5) {
    v.d->spi_teardown();
    v.d->spi_setup();
  } else if (step == 6) {
    v.d->spi_setup();  // already registered: must keep the existing delegate
  } else {
    return;
  }
  (esphome::delay)(20);  // let the log lines go out before the capture
}

#ifdef SPITEST_SLAVE
// ---------------- classic ESP32: SPI3 slave ----------------
static constexpr int SCS = 14;  // nominal slave CS, held high; driven low only for the max7219 window
static WORD_ALIGNED_ATTR uint8_t s_rx[64];
static WORD_ALIGNED_ATTR uint8_t s_tx[64];
static bool s_init = false;
static volatile uint16_t cs_edges[40];
inline int read_level(int pin) { return gpio_get_level((gpio_num_t) pin); }

static void IRAM_ATTR cs_isr(void *arg) { cs_edges[(int) (intptr_t) arg]++; }

inline void arm_cs(int pin) {
  gpio_input_enable((gpio_num_t) pin);
  gpio_set_intr_type((gpio_num_t) pin, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add((gpio_num_t) pin, cs_isr, (void *) (intptr_t) pin);
  gpio_intr_enable((gpio_num_t) pin);
}

inline void prep() {
  gpio_install_isr_service(0);
  for (int i = 0; i < ndev; i++) {
    gpio_input_enable((gpio_num_t) devs[i].clk);
    gpio_input_enable((gpio_num_t) devs[i].mosi);
    arm_cs(devs[i].cs);
  }
  if (mx_cs_ >= 0)
    arm_cs(mx_cs_);
}

inline void slave_setup(int mode) {
  if (s_init)
    spi_slave_free(SPI3_HOST);
  spi_bus_config_t b{};
  b.mosi_io_num = -1;
  b.miso_io_num = -1;
  b.sclk_io_num = -1;
  b.quadwp_io_num = -1;
  b.quadhd_io_num = -1;
  b.max_transfer_sz = 64;
  spi_slave_interface_config_t s{};
  s.mode = mode;
  s.spics_io_num = SCS;
  s.queue_size = 1;
  ESP_ERROR_CHECK(spi_slave_initialize(SPI3_HOST, &b, &s, SPI_DMA_DISABLED));
  s_init = true;
  gpio_set_direction((gpio_num_t) SCS, GPIO_MODE_INPUT_OUTPUT);  // plain GPIO output, input still enabled
  gpio_set_level((gpio_num_t) SCS, 1);
}

inline void route(int clk, int mosi, int miso) {
  gpio_input_enable((gpio_num_t) clk);
  gpio_input_enable((gpio_num_t) mosi);
  esp_rom_gpio_connect_in_signal(clk, spi_periph_signal[SPI3_HOST].spiclk_in, false);
  esp_rom_gpio_connect_in_signal(mosi, spi_periph_signal[SPI3_HOST].spid_in, false);
  if (miso >= 0) {
    gpio_set_direction((gpio_num_t) miso, GPIO_MODE_INPUT_OUTPUT);
    esp_rom_gpio_connect_out_signal(miso, spi_periph_signal[SPI3_HOST].spiq_out, false, false);
  }
}

inline bool queue(int len, uint8_t tag) {
  static const uint8_t ans[8] = {0xC3, 0x5A, 0x81, 0x7E, 0x0F, 0xF0, 0x33, 0xCC};
  memset(s_rx, 0, sizeof(s_rx));
  for (int i = 0; i < 64; i++)
    s_tx[i] = ans[i % 8] ^ tag;
  static spi_slave_transaction_t t;
  t = {};
  t.length = len * 8;
  t.tx_buffer = s_tx;
  t.rx_buffer = s_rx;
  if (spi_slave_queue_trans(SPI3_HOST, &t, pdMS_TO_TICKS(100)) != ESP_OK)
    return false;
  esp_rom_delay_us(300);  // let the slave ISR load it (it reconnects CS from SCS)
  return true;
}

inline int result() {
  spi_slave_transaction_t *r;
  if (spi_slave_get_trans_result(SPI3_HOST, &r, pdMS_TO_TICKS(200)) == ESP_OK)
    return r->trans_len;
  // flush a transaction that never saw CS: pulse the slave CS
  esp_rom_gpio_connect_in_signal(SCS, spi_periph_signal[SPI3_HOST].spics_in, false);
  gpio_set_level((gpio_num_t) SCS, 0);
  esp_rom_delay_us(20);
  gpio_set_level((gpio_num_t) SCS, 1);
  spi_slave_get_trans_result(SPI3_HOST, &r, pdMS_TO_TICKS(200));
  return -1;
}

inline void cs_summary(int own, int &o, int &oth) {
  o = cs_edges[own];
  oth = 0;
  for (int p = 0; p < 40; p++)
    if (p != own)
      oth += cs_edges[p];
}

inline void run_step(int pass, int di, int step) {
  char h1[140], h2[140];
  if (di < 0) {  // max7219 window on the hardware bus
    slave_setup(0);
    route(mx_clk_, mx_mosi_, -1);
    bool q = queue(64, 0);
    memset((void *) cs_edges, 0, sizeof(cs_edges));
    gpio_set_level((gpio_num_t) SCS, 0);
    esp_rom_delay_us(10);
    uint32_t t0 = (esphome::micros) ();
    mx_->update();
    uint32_t us = (esphome::micros) () - t0;
    esp_rom_delay_us(10);
    gpio_set_level((gpio_num_t) SCS, 1);
    int bits = q ? result() : -2;
    int o, oth;
    cs_summary(mx_cs_, o, oth);
    hex(s_rx, bits > 0 ? bits / 8 : 0, h1);
    ESP_LOGI(TAG, "S %d mx 7 bits=%d srx=%s cs=%d/%d us=%u", pass, bits, h1, o, oth, (unsigned) us);
    return;
  }
  Dev &v = devs[di];
  lifecycle(v, step);
  if (step == 5 || step == 6)
    prep();  // a new delegate re-runs the CS pin setup
  slave_setup(v.mode);
  route(v.clk, v.mosi, v.miso);
  bool q = queue(64, v.tag);
  esp_rom_gpio_connect_in_signal(v.cs, spi_periph_signal[SPI3_HOST].spics_in, false);
  if (step == 8 || step == 9)  // master reads its own MOSI pad
    esp_rom_gpio_connect_in_signal(v.mosi, spi_periph_signal[SPI2_HOST].spiq_in, false);
  memset((void *) cs_edges, 0, sizeof(cs_edges));
  uint8_t rx[8];
  size_t rxn;
  uint32_t us = do_op(v, step, rx, rxn);
  int bits = q ? result() : -2;
  gpio_set_direction((gpio_num_t) v.miso, GPIO_MODE_INPUT);
  if (step == 8 || step == 9)
    esp_rom_gpio_connect_in_signal(v.miso, spi_periph_signal[SPI2_HOST].spiq_in, false);
  int o, oth;
  cs_summary(v.cs, o, oth);
  hex(s_rx, bits > 0 ? (bits + 7) / 8 : 0, h1);
  hex(rx, rxn, h2);
  ESP_LOGI(TAG, "S %d %s %d bits=%d srx=%s cs=%d/%d c0=%d c1=%d", pass, v.name, step, bits, h1, o, oth, clk_before_,
           clk_after_);
  ESP_LOGI(TAG, "T %d %s %d us=%u rx=%s", pass, v.name, step, (unsigned) us, h2);
}

#else
// ---------------- edge interrupt snapshots ----------------
struct Ev {
  uint32_t t;
  uint32_t in;
};
static constexpr int NEV = 1100;
static Ev evs[NEV];
static volatile int nev = 0;
static volatile uint32_t ndrop = 0;

static inline __attribute__((always_inline)) uint32_t raw_in() {
#if defined(USE_ESP8266)
  return (GPI & 0xFFFF) | ((GP16O & 1) << 16);  // GPIO16 is only ever a CS output here
#elif defined(USE_ESP32)
  return REG_READ(GPIO_IN_REG);
#elif defined(USE_RP2)
  return sio_hw->gpio_in;
#endif
}
static inline __attribute__((always_inline)) uint32_t raw_t() {
#if defined(USE_ESP8266)
  return esp_get_cycle_count();
#elif defined(USE_ESP32)
  return esp_cpu_get_cycle_count();
#elif defined(USE_RP2)
  return time_us_32();
#endif
}
static inline uint32_t ticks_per_us() {
#if defined(USE_RP2)
  return 1;
#else
  return esphome::arch_get_cpu_freq_hz() / 1000000;
#endif
}
static void SPITEST_ISR isr_rec() {
  int i = nev;
  if (i < NEV) {
    evs[i].t = raw_t();
    evs[i].in = raw_in();
    nev = i + 1;
  } else {
    ndrop = ndrop + 1;
  }
}
#ifdef USE_ESP32
static void SPITEST_ISR isr_rec_arg(void *) { isr_rec(); }
#endif
inline int read_level(int pin) { return (raw_in() >> pin) & 1; }

inline void arm(int pin) {
#ifdef USE_ESP32
  gpio_input_enable((gpio_num_t) pin);
  gpio_set_intr_type((gpio_num_t) pin, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add((gpio_num_t) pin, isr_rec_arg, nullptr);
  gpio_intr_enable((gpio_num_t) pin);
#else
#ifdef USE_ESP8266
  if (pin == 16)
    return;  // no interrupt on GPIO16; its level is still in every snapshot
#endif
  attachInterrupt(digitalPinToInterrupt(pin), isr_rec, CHANGE);
#endif
}

static bool loop_done_ = false;
inline void prep() {
#ifdef USE_ESP32
  static bool svc = false;
  if (!svc) {
    svc = true;
    esp_err_t e = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM);
    ESP_LOGI(TAG, "isr service: %s", esp_err_to_name(e));
  }
#endif
  for (int i = 0; i < ndev; i++) {
    arm(devs[i].clk);
    arm(devs[i].cs);
#ifdef USE_ESP32
    gpio_input_enable((gpio_num_t) devs[i].mosi);
#endif
  }
  if (mx_cs_ >= 0)
    arm(mx_cs_);
  if (loop_done_)
    return;
  loop_done_ = true;
  // software bus: MOSI and MISO share one pad, so the bit-banged read sees what was written
  for (int i = 0; i < ndev; i++) {
    if (devs[i].hw || devs[i].mosi != devs[i].miso)
      continue;
#ifdef USE_ESP32
    gpio_set_direction((gpio_num_t) devs[i].mosi, GPIO_MODE_INPUT_OUTPUT);
#else
    pinMode(devs[i].mosi, OUTPUT);
#endif
  }
#if defined(USE_ESP32)
  // C3 hardware bus: feed the SPI2 MISO input from the MOSI pad
  if (hw_mosi_ >= 0) {
    gpio_input_enable((gpio_num_t) hw_mosi_);
    esp_rom_gpio_connect_in_signal(hw_mosi_, spi_periph_signal[SPI2_HOST].spiq_in, false);
  }
#elif defined(USE_ESP8266)
  if (hw_miso_ >= 0)
    GPF(hw_miso_) |= (1 << GPFPU);  // pull-up, function unchanged: reads give 0xff
#endif
}

#ifdef USE_RP2
// INOVER on the MISO pad: 2 forces the SPI input low, 3 high, 0 normal
inline void rp2_miso(int pin, uint32_t v) {
  hw_write_masked(&io_bank0_hw->io[pin].ctrl, v << IO_BANK0_GPIO0_CTRL_INOVER_LSB, IO_BANK0_GPIO0_CTRL_INOVER_BITS);
}
#endif

static char es_[420];
inline void decode(int pass, const char *name, int step, int cs, int clk, int mosi) {
  int n = nev;
  uint32_t csm = 1u << cs, clkm = 1u << clk, mom = 1u << mosi, othm = 0;
  for (int i = 0; i < ndev; i++)
    if (devs[i].cs != cs)
      othm |= 1u << devs[i].cs;
  if (mx_cs_ >= 0 && mx_cs_ != cs)
    othm |= 1u << mx_cs_;
  bool in = false, ov = false;
  int frames = 0, el = 0, idle = 0, nr = 0;
  uint32_t t_cs = 0, t_e1 = 0;
  uint32_t fr = 0, lr = 0;
  uint32_t prev = evs[0].in;
  for (int i = 1; i < n; i++) {
    uint32_t cur = evs[i].in;
    bool low = !(cur & csm);
    bool chg = (cur ^ prev) & clkm;
    if (!in && low) {
      in = true;
      el = 0;
      ov = false;
      nr = 0;
      idle = chg ? !!(prev & clkm) : !!(cur & clkm);
      t_cs = evs[i].t;
      t_e1 = 0;
    }
    if (in) {
      if (chg && low) {
        if (t_e1 == 0)
          t_e1 = evs[i].t;
        bool rise = cur & clkm;
        bool mo = cur & mom;
        if (el < (int) sizeof(es_) - 2)
          es_[el++] = rise ? (mo ? 'R' : 'r') : (mo ? 'F' : 'f');
        if (rise) {
          if (nr == 0)
            fr = evs[i].t;
          lr = evs[i].t;
          nr++;
        }
      }
      if ((~cur) & othm)
        ov = true;
      if (!low) {
        es_[el] = 0;
        uint32_t per10 = nr > 1 ? (uint32_t) ((uint64_t) (lr - fr) * 10 / (nr - 1) / ticks_per_us()) : 0;
        uint32_t t1 = t_e1 ? (uint32_t) ((uint64_t) (t_e1 - t_cs) * 10 / ticks_per_us()) : 0;
        ESP_LOGI(TAG, "F %d %s %d %d idle=%d ov=%d per10=%u t1x10=%u e=%s", pass, name, step, frames, idle, ov,
                 (unsigned) per10, (unsigned) t1, el ? es_ : "-");
        frames++;
        in = false;
      }
    }
    prev = cur;
  }
  if (in) {
    es_[el] = 0;
    ESP_LOGW(TAG, "F %d %s %d %d UNTERMINATED e=%s", pass, name, step, frames, es_);
  }
  ESP_LOGI(TAG, "N %d %s %d frames=%d ev=%d drop=%u", pass, name, step, frames, n, (unsigned) ndrop);
}

inline void run_step(int pass, int di, int step) {
  char h2[40];
  if (di < 0) {
    nev = 0;
    ndrop = 0;
    isr_rec();
    uint32_t t0 = (esphome::micros) ();
    mx_->update();
    uint32_t us = (esphome::micros) () - t0;
    isr_rec();
    decode(pass, "mx", 7, mx_cs_, mx_clk_, mx_mosi_);
    ESP_LOGI(TAG, "T %d mx 7 us=%u rx=-", pass, (unsigned) us);
    return;
  }
  Dev &v = devs[di];
  lifecycle(v, step);
  if (step == 5 || step == 6)
    prep();
#ifdef USE_RP2
  if (v.hw)
    rp2_miso(v.miso, step == 3 ? 3 : 2);
#endif
  nev = 0;
  ndrop = 0;
  isr_rec();
  uint8_t rx[8];
  size_t rxn;
  uint32_t us = do_op(v, step, rx, rxn);
  isr_rec();
#ifdef USE_RP2
  if (v.hw)
    rp2_miso(v.miso, 0);
#endif
  decode(pass, v.name, step, v.cs, v.clk, v.mosi);
  hex(rx, rxn, h2);
  ESP_LOGI(TAG, "T %d %s %d us=%u rx=%s c0=%d c1=%d%s", pass, v.name, step, (unsigned) us, h2, clk_before_, clk_after_,
           regs_);
  regs_[0] = 0;
}
#endif

// ---------------- schedule ----------------
struct Item {
  int8_t dev;
  uint8_t step;
};
static Item sched_[200];
static int nsched_ = 0, pos_ = 0;
static constexpr int PASSES = 3;
static uint32_t done_at_ = 0;
static bool started_ = false;

inline void build() {
  for (int p = 0; p < PASSES; p++) {
    for (int i = 0; i < ndev; i++)
      for (int s = 0; s <= 4; s++)
        sched_[nsched_++] = Item{(int8_t) i, (uint8_t) s};
    for (int i = 0; i < ndev; i++)  // back to back, all devices, then the first again
      sched_[nsched_++] = Item{(int8_t) i, 0};
    sched_[nsched_++] = Item{0, 0};
    for (int i = 0; i < ndev; i++)  // teardown + setup on every device
      sched_[nsched_++] = Item{(int8_t) i, 5};
    sched_[nsched_++] = Item{1, 6};  // second spi_setup() on a registered device
    sched_[nsched_++] = Item{(int8_t) (ndev - 1), 6};
#ifdef SPITEST_SLAVE
    for (int i = 0; i < ndev; i++)  // hardware bus read through a MOSI to MISO loopback
      if (devs[i].hw) {
        sched_[nsched_++] = Item{(int8_t) i, 8};
        sched_[nsched_++] = Item{(int8_t) i, 9};
      }
#endif
    if (mx_ != nullptr) {
      sched_[nsched_++] = Item{0, 0};  // leave the hardware bus clock at mode 0 idle before the max7219 window
      sched_[nsched_++] = Item{-1, 7};
    }
  }
}

inline void start() {
  prep();
  build();
  started_ = true;
  ESP_LOGI(TAG, "START items=%d devs=%d", nsched_, ndev);
  ESP_LOGI(TAG, "%s", boot_heap_);
}

inline void tick() {
  if (!started_)
    return;
  if (pos_ < nsched_) {
    int per_pass = nsched_ / PASSES;
    Item it = sched_[pos_];
    run_step(pos_ / per_pass, it.dev, it.step);
    pos_++;
    if (pos_ == nsched_) {
      log_heap("after");
      ESP_LOGI(TAG, "DONE items=%d", nsched_);
      done_at_ = (esphome::millis) ();
    }
    return;
  }
  if (done_at_ != 0 && (esphome::millis) () - done_at_ > 2000) {
    done_at_ = 0;
    esphome::App.safe_reboot();
  }
}
}  // namespace spitest
