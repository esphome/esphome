#pragma once
// Captures what the LED drivers put on their pads: RMT RX channels on the two esp32_rmt_led_strip pins (10 MHz,
// one frame each, decoded as WS2812 bits) and an SPI2 DMA slave fed from the spi_led_strip clock and data pads.
#include <cstring>
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/spi_slave.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "soc/spi_periph.h"
#include "esp_heap_caps.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include <algorithm>

namespace wire {
static const char *const TAG = "wire";
static constexpr int CS = 2;
static constexpr size_t SPI_BUF = 256;

struct Rx {
  rmt_channel_handle_t h{nullptr};
  rmt_symbol_word_t sym[192];
  volatile size_t n{0};
  volatile bool done{false};
  const char *name{""};
  unsigned thr{6};  // a high longer than this many 0.1 us ticks is a 1
};
static Rx rx_[2];
static uint8_t *spi_buf_{nullptr};
static spi_slave_transaction_t t_{};

static bool on_done(rmt_channel_handle_t, const rmt_rx_done_event_data_t *e, void *ctx) {
  auto *r = static_cast<Rx *>(ctx);
  r->n = e->num_symbols;
  r->done = true;
  return false;
}

inline void init(int pin_a, const char *name_a, unsigned thr_a, int pin_b, const char *name_b, unsigned thr_b,
                 int spi_clk, int spi_dat) {
  int pins[2] = {pin_a, pin_b};
  const char *names[2] = {name_a, name_b};
  rx_[0].thr = thr_a;
  rx_[1].thr = thr_b;
  for (int i = 0; i < 2; i++) {
    if (pins[i] < 0)
      continue;
    rmt_rx_channel_config_t c{};
    c.gpio_num = (gpio_num_t) pins[i];
    c.clk_src = RMT_CLK_SRC_DEFAULT;
    c.resolution_hz = 10000000;
    c.mem_block_symbols = 192;
    esp_err_t err = rmt_new_rx_channel(&c, &rx_[i].h);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "rx %d: %s", i, esp_err_to_name(err));
      continue;
    }
    rx_[i].name = names[i];
    rmt_rx_event_callbacks_t cb{};
    cb.on_recv_done = on_done;
    rmt_rx_register_event_callbacks(rx_[i].h, &cb, &rx_[i]);
    rmt_enable(rx_[i].h);
  }
  spi_bus_config_t b{};
  b.mosi_io_num = -1;
  b.miso_io_num = -1;
  b.sclk_io_num = -1;
  b.quadwp_io_num = -1;
  b.quadhd_io_num = -1;
  b.max_transfer_sz = SPI_BUF;
  spi_slave_interface_config_t s{};
  s.mode = 1;  // samples on the rising edge of its clock input (measured, see led_driver_sniff)
  s.spics_io_num = CS;
  s.queue_size = 1;
  ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &b, &s, SPI_DMA_CH_AUTO));
  spi_buf_ = (uint8_t *) heap_caps_malloc(SPI_BUF, MALLOC_CAP_DMA);
  gpio_set_direction((gpio_num_t) CS, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level((gpio_num_t) CS, 1);
  gpio_input_enable((gpio_num_t) spi_clk);
  gpio_input_enable((gpio_num_t) spi_dat);
  esp_rom_gpio_connect_in_signal(spi_clk, spi_periph_signal[SPI2_HOST].spiclk_in, false);
  esp_rom_gpio_connect_in_signal(spi_dat, spi_periph_signal[SPI2_HOST].spid_in, false);
  ESP_LOGI(TAG, "wire capture ready");
}

static bool armed_{false};  // a queued SPI transaction owns t_ and spi_buf_ until it is collected
inline void arm() {
  if (armed_)
    return;
  armed_ = true;
  for (auto &r : rx_) {
    if (r.h == nullptr)
      continue;
    r.done = false;
    r.n = 0;
    rmt_receive_config_t rc{};
    rc.signal_range_min_ns = 100;
    rc.signal_range_max_ns = 20000;
    rmt_receive(r.h, r.sym, sizeof(r.sym), &rc);
  }
  memset(spi_buf_, 0, SPI_BUF);
  t_ = {};
  t_.length = SPI_BUF * 8;
  t_.rx_buffer = spi_buf_;
  spi_slave_queue_trans(SPI2_HOST, &t_, portMAX_DELAY);
  esp_rom_delay_us(20);
  gpio_set_level((gpio_num_t) CS, 0);
}

// Bit = high time over the chipset threshold (ws2812 0.4/0.8 us, sk6812 0.3/0.6 us); on_bit(high, one) sees each bit.
template<typename F> inline size_t decode(const Rx &r, uint8_t (&bytes)[64], F &&on_bit) {
  size_t bits = 0;
  for (size_t i = 0; i < r.n && bits < 64 * 8; i++) {
    if (r.sym[i].level0 != 1)
      continue;
    unsigned high = r.sym[i].duration0;
    bool one = high > r.thr;
    on_bit(high, one);
    if (one)
      bytes[bits / 8] |= 0x80 >> (bits % 8);
    bits++;
  }
  return bits;
}
inline void rx_cancel(Rx &r) {
  rmt_disable(r.h);  // cancels the pending receive
  rmt_enable(r.h);
}

inline void report(const char *step) {
  char hex[2 * 64 + 1];
  for (auto &r : rx_) {
    if (r.h == nullptr)
      continue;
    if (!r.done) {
      ESP_LOGI(TAG, "RMT %s %s none", step, r.name);
      continue;
    }
    // Keep the high ranges seen for 0 and 1 bits, so timing changes show up as well.
    uint8_t bytes[64] = {0};
    unsigned hmin[2] = {999, 999}, hmax[2] = {0, 0};
    size_t bits = decode(r, bytes, [&](unsigned high, bool one) {
      hmin[one] = std::min(hmin[one], high);
      hmax[one] = std::max(hmax[one], high);
    });
    esphome::format_hex_to(hex, bytes, bits / 8);
    ESP_LOGI(TAG, "RMT %s %s bits=%u high0=%u-%u high1=%u-%u (0.1us) %s", step, r.name, (unsigned) bits, hmin[0],
             hmax[0], hmin[1], hmax[1], hex);
  }
  for (auto &r : rx_) {
    if (r.h != nullptr && !r.done)
      rx_cancel(r);
  }
  esp_rom_delay_us(20);
  gpio_set_level((gpio_num_t) CS, 1);
  spi_slave_transaction_t *res;
  int len = -1;
  if (spi_slave_get_trans_result(SPI2_HOST, &res, pdMS_TO_TICKS(500)) == ESP_OK)
    len = res->trans_len;
  armed_ = false;
  esphome::format_hex_to(hex, spi_buf_, len > 0 ? std::min(len / 8, 64) : 0);
  ESP_LOGI(TAG, "SPI %s bits=%d %s", step, len, hex);
}
inline void readback(const char *step, const char *name, esphome::light::LightState *ls) {
  auto *a = static_cast<esphome::light::AddressableLight *>(ls->get_output());
  char buf[400];
  size_t o = 0;
  for (int32_t i = 0; i < a->size() && o + 10 < sizeof(buf); i++) {
    esphome::Color c = a->get(i).get();
    o += snprintf(buf + o, sizeof(buf) - o, "%02x%02x%02x%02x ", c.r, c.g, c.b, c.w);
  }
  buf[o] = 0;
  ESP_LOGI(TAG, "GET %s %s %s", step, name, buf);
}
// Stress: decode each strip's frame, compare with the first one seen, count frames with off-nominal highs.
struct Stats {
  uint8_t ref[64];
  size_t ref_bits{0};
  uint32_t frames{0}, missing{0}, bad_bytes{0}, bad_timing{0};
  unsigned nom[2]{0, 0};  // first high seen for a 0 and a 1 bit
};
static Stats st_[2];
inline void stress_step() {
  for (int k = 0; k < 2; k++) {
    auto &r = rx_[k];
    auto &s = st_[k];
    if (r.h == nullptr)
      continue;
    if (!r.done) {
      s.missing++;
      rx_cancel(r);
      continue;
    }
    uint8_t bytes[64] = {0};
    bool off = false;
    size_t bits = decode(r, bytes, [&](unsigned high, bool one) {
      if (s.frames == 0 && s.nom[one] == 0)
        s.nom[one] = high;
      if (high != s.nom[one])
        off = true;
    });
    if (s.frames == 0) {
      memcpy(s.ref, bytes, sizeof(bytes));
      s.ref_bits = bits;
    } else if (bits != s.ref_bits || memcmp(bytes, s.ref, (bits + 7) / 8) != 0) {
      s.bad_bytes++;
    }
    if (off)
      s.bad_timing++;
    s.frames++;
  }
  // keep the SPI slave from piling up: finish this window's transaction
  gpio_set_level((gpio_num_t) CS, 1);
  spi_slave_transaction_t *res;
  spi_slave_get_trans_result(SPI2_HOST, &res, pdMS_TO_TICKS(100));
  armed_ = false;
}
inline void stress_report() {
  for (int k = 0; k < 2; k++) {
    if (rx_[k].h == nullptr)
      continue;
    auto &s = st_[k];
    ESP_LOGI(TAG, "STRESS %s frames=%u missing=%u bad_bytes=%u bad_timing=%u nominal=%u/%u", rx_[k].name,
             (unsigned) s.frames, (unsigned) s.missing, (unsigned) s.bad_bytes, (unsigned) s.bad_timing, s.nom[0],
             s.nom[1]);
  }
}
}  // namespace wire
