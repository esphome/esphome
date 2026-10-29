#pragma once
// Self-capture of bit-banged LED driver lines on a classic ESP32.
// SPI2 (slave mode 1, DMA) samples data on rising clock edges, SPI3 (clock inverted) on falling edges, both fed from
// the driver's own pads through the GPIO matrix; CS comes from a spare pad the test drives. PCNT counts edges on clock,
// data and latch.
#include <cstring>
#include "driver/gpio.h"
#include "driver/spi_slave.h"
#include "driver/pulse_cnt.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "soc/spi_periph.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esphome/core/log.h"

static volatile uint64_t sniff_keep_ie = 0;
extern "C" esp_err_t __real_gpio_set_direction(gpio_num_t pin, gpio_mode_t mode);
extern "C" esp_err_t __wrap_gpio_set_direction(gpio_num_t pin, gpio_mode_t mode) {
  esp_err_t e = __real_gpio_set_direction(pin, mode);
  if (sniff_keep_ie & (1ULL << pin))
    gpio_input_enable(pin);
  return e;
}

namespace sniff {
static const char *const TAG = "sniff";
static constexpr size_t BUF = 4096;
static constexpr int CS = 2;
static uint8_t *rx_[2];
static spi_slave_transaction_t t_[2];
static int clk_ = -1, dat_ = -1, lat_ = -1;
static const char *name_ = "";
static bool ready_ = false;
struct Unit {
  pcnt_unit_handle_t u{nullptr};
  pcnt_channel_handle_t c{nullptr};
};
static Unit units_[5];  // clock rise, clock fall, data rise, data fall, latch rise
static int clk_idle_ = 0;
static int64_t t_open_ = 0;

inline void del_unit(Unit &x) {
  if (x.u == nullptr)
    return;
  pcnt_unit_stop(x.u);
  pcnt_unit_disable(x.u);
  pcnt_del_channel(x.c);
  pcnt_del_unit(x.u);
  x = Unit{};
}
inline void mk_unit(Unit &x, int pin, bool rising) {
  pcnt_unit_config_t uc{};
  uc.low_limit = -32768;
  uc.high_limit = 32767;
  ESP_ERROR_CHECK(pcnt_new_unit(&uc, &x.u));
  pcnt_chan_config_t cc{};
  cc.edge_gpio_num = pin;
  cc.level_gpio_num = -1;
  ESP_ERROR_CHECK(pcnt_new_channel(x.u, &cc, &x.c));
  pcnt_channel_set_edge_action(x.c, rising ? PCNT_CHANNEL_EDGE_ACTION_INCREASE : PCNT_CHANNEL_EDGE_ACTION_HOLD,
                               rising ? PCNT_CHANNEL_EDGE_ACTION_HOLD : PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_unit_enable(x.u);
  pcnt_unit_clear_count(x.u);
  pcnt_unit_start(x.u);
}
inline int count(Unit &x) {
  int v = 0;
  if (x.u != nullptr)
    pcnt_unit_get_count(x.u, &v);
  return v;
}

inline void init_slaves() {
  for (int i = 0; i < 2; i++) {
    spi_host_device_t host = i == 0 ? SPI2_HOST : SPI3_HOST;
    spi_bus_config_t b{};
    b.mosi_io_num = -1;
    b.miso_io_num = -1;
    b.sclk_io_num = -1;
    b.quadwp_io_num = -1;
    b.quadhd_io_num = -1;
    b.max_transfer_sz = BUF;
    spi_slave_interface_config_t s{};
    s.mode = 1;  // measured: samples on the rising edge of its clock input
    s.spics_io_num = CS;
    s.queue_size = 1;
    ESP_ERROR_CHECK(spi_slave_initialize(host, &b, &s, SPI_DMA_CH_AUTO));
    rx_[i] = (uint8_t *) heap_caps_malloc(BUF, MALLOC_CAP_DMA);
  }
  gpio_set_direction((gpio_num_t) CS, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level((gpio_num_t) CS, 1);
  ready_ = true;
}

// Point the capture at one driver's pins.
inline void use(const char *name, int clk, int dat, int lat = -1) {
  if (!ready_)
    init_slaves();
  for (auto &u : units_)
    del_unit(u);
  name_ = name;
  clk_ = clk;
  dat_ = dat;
  lat_ = lat;
  sniff_keep_ie = (1ULL << clk) | (1ULL << dat) | (lat >= 0 ? (1ULL << lat) : 0);
  gpio_input_enable((gpio_num_t) clk);
  gpio_input_enable((gpio_num_t) dat);
  if (lat >= 0)
    gpio_input_enable((gpio_num_t) lat);
  esp_rom_gpio_connect_in_signal(clk, spi_periph_signal[SPI2_HOST].spiclk_in, false);
  esp_rom_gpio_connect_in_signal(dat, spi_periph_signal[SPI2_HOST].spid_in, false);
  esp_rom_gpio_connect_in_signal(clk, spi_periph_signal[SPI3_HOST].spiclk_in, true);
  esp_rom_gpio_connect_in_signal(dat, spi_periph_signal[SPI3_HOST].spid_in, false);
  mk_unit(units_[0], clk, true);
  mk_unit(units_[1], clk, false);
  mk_unit(units_[2], dat, true);
  mk_unit(units_[3], dat, false);
  if (lat >= 0)
    mk_unit(units_[4], lat, true);
}

inline void open() {
  for (auto &u : units_)
    if (u.u != nullptr)
      pcnt_unit_clear_count(u.u);
  for (int i = 0; i < 2; i++) {
    memset(rx_[i], 0, BUF);
    t_[i] = {};
    t_[i].length = BUF * 8;
    t_[i].rx_buffer = rx_[i];
    spi_slave_queue_trans(i == 0 ? SPI2_HOST : SPI3_HOST, &t_[i], portMAX_DELAY);
  }
  clk_idle_ = gpio_get_level((gpio_num_t) clk_);
  esp_rom_delay_us(20);
  gpio_set_level((gpio_num_t) CS, 0);
  esp_rom_delay_us(20);
  t_open_ = esp_timer_get_time();
}

inline void hex_bits(const uint8_t *buf, int n, char *out, size_t cap) {
  size_t o = 0;
  int bytes = (n + 7) / 8;
  for (int i = 0; i < bytes && o + 3 < cap; i++) {
    uint8_t b = buf[i];
    if (i == bytes - 1 && (n % 8))
      b &= (uint8_t) (0xFF << (8 - n % 8));
    o += snprintf(out + o, cap - o, "%02x", b);
  }
  out[o] = 0;
}

// Close the window and log what the driver sent: bit counts, edge counts and the sampled bits.
inline void close(const char *step) {
  int rise = count(units_[0]), fall = count(units_[1]);
  int drise = count(units_[2]), dfall = count(units_[3]), lrise = count(units_[4]);
  // Pad with whole clock pulses so DMA, which drops a partial last word, keeps every real bit.
  int pad = 64 - (rise % 32);
  int lvl = gpio_get_level((gpio_num_t) clk_);
  for (int i = 0; i < pad; i++) {
    gpio_set_level((gpio_num_t) clk_, !lvl);
    esp_rom_delay_us(1);
    gpio_set_level((gpio_num_t) clk_, lvl);
    esp_rom_delay_us(1);
  }
  esp_rom_delay_us(20);
  gpio_set_level((gpio_num_t) CS, 1);
  spi_slave_transaction_t *r;
  int got[2] = {-1, -1};
  for (int i = 0; i < 2; i++) {
    if (spi_slave_get_trans_result(i == 0 ? SPI2_HOST : SPI3_HOST, &r, pdMS_TO_TICKS(500)) == ESP_OK)
      got[i] = r->trans_len;
  }
  static char h0[1200], h1[1200];
  hex_bits(rx_[0], rise + 2, h0, sizeof(h0));
  hex_bits(rx_[1], fall + 2, h1, sizeof(h1));
  ESP_LOGI(TAG, "CAP %s %s rise=%d fall=%d idle=%d drise=%d dfall=%d lat=%d spi=%d/%d", name_, step, rise, fall,
           clk_idle_, drise, dfall, lrise, got[0], got[1]);
  ESP_LOGI(TAG, "R %s %s %s", name_, step, h0);
  ESP_LOGI(TAG, "F %s %s %s", name_, step, h1);
}

// Known pattern through the same path, to prove the capture before trusting it.
inline void selftest(int clk, int dat, int idle) {
  gpio_set_direction((gpio_num_t) clk, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_direction((gpio_num_t) dat, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level((gpio_num_t) clk, idle);
  use("selftest", clk, dat);
  open();
  static const uint8_t pat[] = {0xA5, 0x3C, 0x00, 0xFF, 0x81, 0x7E, 0x12};
  int lvl = gpio_get_level((gpio_num_t) clk);
  for (uint8_t b : pat)
    for (int m = 0x80; m; m >>= 1) {
      gpio_set_level((gpio_num_t) dat, (b & m) != 0);
      esp_rom_delay_us(1);
      gpio_set_level((gpio_num_t) clk, !lvl);
      esp_rom_delay_us(1);
      gpio_set_level((gpio_num_t) clk, lvl);
      esp_rom_delay_us(1);
    }
  // one odd trailing bit
  gpio_set_level((gpio_num_t) dat, 1);
  gpio_set_level((gpio_num_t) clk, !lvl);
  gpio_set_level((gpio_num_t) clk, lvl);
  close("expect_a53c00ff817e1280_57bits");
}
}  // namespace sniff
