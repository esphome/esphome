#pragma once
// Captures every period of the LEDC pin on the same chip: RMT RX at 80 MHz (12.5 ns ticks) through the GPIO
// matrix, analysed in the IRAM-safe RMT ISR, plus PCNT counting rising edges as an independent period count.
// Assumes LEDC timer 0 channel 0 drives the pad; report() logs one WIN line per call.
#include <cstring>
#include <cinttypes>
#include "driver/rmt_rx.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "soc/ledc_struct.h"
#include "soc/pcr_struct.h"
#include "driver/ledc.h"
#include "esphome/core/log.h"

namespace pwmcap {
static const char *const TAG = "pwmcap";
static constexpr int TOL = 3;  // ticks; one LEDC step at 11 bits is about 1.95 ticks
static constexpr int HBINS = 4200;
static constexpr int PBASE = 3800, PBINS = 400;
static constexpr size_t RXBUF = 1024;
static constexpr size_t CHG = 2048;

struct Win {
  uint32_t hist_h[HBINS];
  uint32_t hist_p[PBINS];
  uint32_t p_under, p_over, h_over;
  uint32_t n, zero_dur, chunks;
  uint32_t pmin, pmax, hmin, hmax;
  // level changes: |h - level| > TOL
  uint32_t chg_n;
  uint32_t chg_idx[CHG];
  uint16_t chg_h[CHG];
};

static DRAM_ATTR Win wins[2];
static DRAM_ATTR volatile int cur = 0;
static DRAM_ATTR rmt_symbol_word_t rxbuf[RXBUF];
static DRAM_ATTR volatile uint32_t total_idx = 0;
static DRAM_ATTR volatile int level = -1;
static DRAM_ATTR volatile bool skip_first = true;
static DRAM_ATTR volatile uint32_t idle_ends = 0;
static rmt_channel_handle_t rx = nullptr;
static pcnt_unit_handle_t pcnt = nullptr;
static rmt_receive_config_t rcfg{};
static int64_t win_start = 0;
static int last_pcnt = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool need_rearm = false;

static void reset_win(Win &w) {
  memset(&w, 0, sizeof(w));
  w.pmin = UINT32_MAX;
  w.hmin = UINT32_MAX;
}

static bool IRAM_ATTR on_rx(rmt_channel_handle_t, const rmt_rx_done_event_data_t *ev, void *) {
  Win &w = wins[cur];
  w.chunks++;
  size_t n = ev->num_symbols;
  const rmt_symbol_word_t *s = ev->received_symbols;
  for (size_t i = 0; i < n; i++) {
    uint32_t d0 = s[i].duration0, d1 = s[i].duration1;
    if (skip_first) {
      skip_first = false;
      continue;
    }
    if (d0 == 0 || d1 == 0) {
      w.zero_dur++;
      continue;
    }
    uint32_t h = s[i].level0 ? d0 : d1;
    uint32_t p = d0 + d1;
    uint32_t idx = total_idx++;
    w.n++;
    if (h < HBINS)
      w.hist_h[h]++;
    else
      w.h_over++;
    if (p < PBASE)
      w.p_under++;
    else if (p >= PBASE + PBINS)
      w.p_over++;
    else
      w.hist_p[p - PBASE]++;
    if (p < w.pmin)
      w.pmin = p;
    if (p > w.pmax)
      w.pmax = p;
    if (h < w.hmin)
      w.hmin = h;
    if (h > w.hmax)
      w.hmax = h;
    int lv = level;
    int dh = (int) h - lv;
    if (lv < 0 || dh > TOL || dh < -TOL) {
      level = (int) h;
      if (w.chg_n < CHG) {
        w.chg_idx[w.chg_n] = idx;
        w.chg_h[w.chg_n] = (uint16_t) h;
      }
      w.chg_n++;
    }
  }
  if (ev->flags.is_last) {
    idle_ends++;
    need_rearm = true;
  }
  return false;
}

// Mode of a histogram whose bin i counts value base + i, the count more than TOL from the mode and the largest
// such distance.
struct HistStats {
  uint32_t mode, out, dev;
};

static HistStats hist_stats(const uint32_t *hist, int bins, int base) {
  HistStats st{0, 0, 0};
  uint32_t best = 0;
  for (int i = 0; i < bins; i++) {
    if (hist[i] > best) {
      best = hist[i];
      st.mode = i + base;
    }
  }
  for (int i = 0; i < bins; i++) {
    int d = i + base - (int) st.mode;
    uint32_t ad = d < 0 ? -d : d;
    if (ad > (uint32_t) TOL && hist[i]) {
      st.out += hist[i];
      if (ad > st.dev)
        st.dev = ad;
    }
  }
  return st;
}

// Logs LABEL, PREFIX and up to 12 bins more than TOL from mode.
static void log_outliers(const char *label, const char *prefix, const uint32_t *hist, int bins, int base,
                         uint32_t mode) {
  char buf[200];
  int pos = snprintf(buf, sizeof(buf), "%s", prefix);
  for (int i = 0, k = 0; i < bins && k < 12 && pos < (int) sizeof(buf); i++) {
    int d = i + base - (int) mode;
    if ((d > TOL || d < -TOL) && hist[i]) {
      pos += snprintf(buf + pos, sizeof(buf) - pos, " %d:%" PRIu32, i + base, hist[i]);
      k++;
    }
  }
  ESP_LOGI(TAG, "%s%s", label, buf);
}

static bool check(esp_err_t e, const char *what) {
  if (e != ESP_OK)
    ESP_LOGE(TAG, "%s: %s", what, esp_err_to_name(e));
  return e == ESP_OK;
}

static void arm() {
  skip_first = true;
  esp_err_t e = rmt_receive(rx, rxbuf, sizeof(rxbuf), &rcfg);
  if (e != ESP_OK)
    ESP_LOGE(TAG, "rmt_receive: %s", esp_err_to_name(e));
}

static void dump_ledc() {
  auto &t = LEDC.timer_group[0].timer[0].conf;
  auto &c = LEDC.channel_group[0].channel[0];
  ESP_LOGI(TAG,
           "LEDC sclk_sel=%u (1=PLL_F80M 2=RC_FAST 3=XTAL) timer0 duty_res=%u clk_div=0x%05x (%.4f) freq=%" PRIu32
           " Hz duty_r=%u (x1/16) hpoint=%u",
           (unsigned) PCR.ledc_sclk_conf.ledc_sclk_sel, (unsigned) t.duty_res, (unsigned) t.clk_div, t.clk_div / 256.0f,
           ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0), (unsigned) c.duty_rd.duty_r, (unsigned) c.hpoint.hpoint);
}

inline void start(int pin) {
  reset_win(wins[0]);
  reset_win(wins[1]);
  rmt_rx_channel_config_t cc{};
  cc.gpio_num = (gpio_num_t) pin;
  cc.clk_src = RMT_CLK_SRC_DEFAULT;
  cc.resolution_hz = 80000000;
  cc.mem_block_symbols = 96;
  cc.intr_priority = 3;
  esp_err_t e = rmt_new_rx_channel(&cc, &rx);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "rmt_new_rx_channel: %s", esp_err_to_name(e));
    return;
  }
  rmt_rx_event_callbacks_t cbs{};
  cbs.on_recv_done = on_rx;
  if (!check(rmt_rx_register_event_callbacks(rx, &cbs, nullptr), "rmt callbacks") ||
      !check(rmt_enable(rx), "rmt_enable")) {
    rmt_del_channel(rx);
    rx = nullptr;
    return;
  }
  rcfg.signal_range_min_ns = 100;
  rcfg.signal_range_max_ns = 300000;
  rcfg.flags.en_partial_rx = 1;

  pcnt_unit_config_t uc{};
  uc.low_limit = -1;
  uc.high_limit = 30000;
  uc.flags.accum_count = 1;
  // a PCNT failure leaves pcnt=0 in every WIN line, which summ.py flags against n
  pcnt_chan_config_t pc{};
  pc.edge_gpio_num = pin;
  pc.level_gpio_num = -1;
  pcnt_channel_handle_t ch;
  if (check(pcnt_new_unit(&uc, &pcnt), "pcnt_new_unit") && check(pcnt_new_channel(pcnt, &pc, &ch), "pcnt channel")) {
    check(pcnt_channel_set_edge_action(ch, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD),
          "pcnt edge action");
    check(pcnt_unit_add_watch_point(pcnt, 30000), "pcnt watch point");
    check(pcnt_unit_enable(pcnt), "pcnt_unit_enable");
    check(pcnt_unit_clear_count(pcnt), "pcnt_unit_clear_count");
    check(pcnt_unit_start(pcnt), "pcnt_unit_start");
  }
  // The drivers only add input; make sure LEDC still drives the pad.
  gpio_ll_output_enable(&GPIO, pin);
  gpio_pullup_dis((gpio_num_t) pin);
  win_start = esp_timer_get_time();
  arm();
  dump_ledc();
}

// Called from the main loop every 10 s.
inline void report() {
  if (rx == nullptr) {
    ESP_LOGE(TAG, "WIN capture=off");
    return;
  }
  if (need_rearm) {
    need_rearm = false;
    arm();
  }
  portENTER_CRITICAL(&mux);
  int done = cur;
  cur = done ^ 1;
  portEXIT_CRITICAL(&mux);
  int64_t now = esp_timer_get_time();
  int pc = 0;
  pcnt_unit_get_count(pcnt, &pc);
  int pc_delta = pc - last_pcnt;
  last_pcnt = pc;
  Win &w = wins[done];
  HistStats hs = hist_stats(w.hist_h, HBINS, 0);
  HistStats ps = hist_stats(w.hist_p, PBINS, PBASE);
  uint32_t h_out = w.h_over + hs.out, p_out = w.p_under + w.p_over + ps.out;
  // spikes: a level change that returns to the previous level within 64 periods; reversals: direction flips
  uint32_t spikes = 0, reversals = 0;
  uint32_t nchg = w.chg_n < CHG ? w.chg_n : CHG;
  int last_dir = 0;
  for (uint32_t i = 1; i < nchg; i++) {
    int d = (int) w.chg_h[i] - (int) w.chg_h[i - 1];
    int dir = d > 0 ? 1 : -1;
    if (i >= 2) {
      int back = (int) w.chg_h[i] - (int) w.chg_h[i - 2];
      if (back <= TOL && back >= -TOL && w.chg_idx[i] - w.chg_idx[i - 1] <= 64)
        spikes++;
    }
    if (last_dir && dir != last_dir && w.chg_idx[i] - w.chg_idx[i - 1] <= 2000)
      reversals++;
    last_dir = dir;
  }
  ESP_LOGI(TAG,
           "WIN dt=%" PRId64 "ms n=%" PRIu32 " pcnt=%d chunks=%" PRIu32 " zero=%" PRIu32 " idle=%" PRIu32
           " h[min=%" PRIu32 " mode=%" PRIu32 " max=%" PRIu32 " out=%" PRIu32 " dev=%" PRIu32 "] p[min=%" PRIu32
           " mode=%" PRIu32 " max=%" PRIu32 " out=%" PRIu32 " dev=%" PRIu32 "] chg=%" PRIu32 " spikes=%" PRIu32
           " rev=%" PRIu32 " duty_r=%u",
           (now - win_start) / 1000, w.n, pc_delta, w.chunks, w.zero_dur, idle_ends, w.hmin == UINT32_MAX ? 0 : w.hmin,
           hs.mode, w.hmax, h_out, hs.dev, w.pmin == UINT32_MAX ? 0 : w.pmin, ps.mode, w.pmax, p_out, ps.dev, w.chg_n,
           spikes, reversals, (unsigned) LEDC.channel_group[0].channel[0].duty_rd.duty_r);
  // print outlier bins (up to 12) for the static cases
  if (h_out && w.chg_n < 40)
    log_outliers("HOUT", "", w.hist_h, HBINS, 0, hs.mode);
  if (p_out) {
    char prefix[48];
    snprintf(prefix, sizeof(prefix), " under=%" PRIu32 " over=%" PRIu32, w.p_under, w.p_over);
    log_outliers("POUT", prefix, w.hist_p, PBINS, PBASE, ps.mode);
  }
  if (spikes || reversals) {
    char buf[240];
    int pos = 0;
    for (uint32_t i = 0; i < nchg && pos < 200; i++)
      pos += snprintf(buf + pos, sizeof(buf) - pos, " %u", w.chg_h[i]);
    ESP_LOGI(TAG, "CHG%s", buf);
  }
  win_start = now;
  reset_win(w);
}
}  // namespace pwmcap
