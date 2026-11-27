#pragma once
// ac_dimmer bench harness: ZC generator output GPIO12 (esp8266_pwm 100 Hz, falling edge = crossing) jumpered to the
// dimmer's ZC input GPIO14; dimmer gate GPIO13 jumpered to GPIO4 (measured here). Noise: short pulses written on
// GPIO12. Used for #22912; acd.yaml is the config.
#include <Arduino.h>
#include <cmath>
#include "esphome/core/log.h"
#include "esphome/components/output/float_output.h"

namespace acdm {
static constexpr uint8_t ZC_PIN = 12;
static constexpr uint8_t GATE_IN = 4;
static constexpr uint32_t CYCLE = 10000;

// Error of one gate edge against its expected time; off counts edges more than 100 us off.
struct ErrStat {
  uint32_t n, off;
  int32_t min, max;
  int64_t sum;
  uint64_t sq;
  void clear() {
    *this = {};
    this->min = INT32_MAX;
    this->max = INT32_MIN;
  }
  void IRAM_ATTR add(int32_t err) {
    this->n++;
    this->sum += err;
    this->sq += (int64_t) err * err;
    if (err < this->min)
      this->min = err;
    if (err > this->max)
      this->max = err;
    if (err > 100 || err < -100)
      this->off++;
  }
  float mean() const { return this->n ? (double) this->sum / this->n : 0; }
  float sd() const {
    if (this->n == 0)
      return 0;
    double m = (double) this->sum / this->n;
    double v = (double) this->sq / this->n - m * m;
    return v > 0 ? sqrt(v) : 0;
  }
};

struct Stats {
  uint32_t zc, glitch, rises, falls, missed, dbl, high_at_zc;
  ErrStat rise, fall;
  uint32_t w_min, w_max;
};

static volatile bool injecting = false;
static volatile uint32_t last_zc = 0, last_rise = 0, rise_zc = 0;
static volatile uint32_t rises_this_half = 0;
static volatile int32_t exp_rise = -1, exp_fall = -1;  // relative to the zero crossing; -1 = not checked
static Stats st;

static void reset_stats() {
  noInterrupts();
  st = {};
  st.rise.clear();
  st.fall.clear();
  st.w_min = UINT32_MAX;
  interrupts();
}

static void IRAM_ATTR zc_isr() {
  uint32_t now = micros();
  if (injecting) {
    st.glitch++;
    return;
  }
  if (last_zc != 0) {
    if (rises_this_half == 0)
      st.missed++;
    else if (rises_this_half > 1)
      st.dbl++;
  }
  if ((GPI >> GATE_IN) & 1)
    st.high_at_zc++;
  rises_this_half = 0;
  last_zc = now;
  st.zc++;
}

static void IRAM_ATTR gate_isr() {
  uint32_t now = micros();
  bool high = (GPI >> GATE_IN) & 1;
  if (high) {
    st.rises++;
    rises_this_half++;
    last_rise = now;
    rise_zc = last_zc;
    if (exp_rise >= 0)
      st.rise.add((int32_t) (now - last_zc) - exp_rise);
  } else {
    st.falls++;
    uint32_t w = now - last_rise;
    if (w < st.w_min)
      st.w_min = w;
    if (w > st.w_max)
      st.w_max = w;
    if (exp_fall >= 0)
      st.fall.add((int32_t) (now - rise_zc) - exp_fall);
  }
}

// method: 0 leading pulse, 1 leading, 2 trailing (min_power 0.1, 50 Hz)
static void set_expected(float level, int method) {
  if (level >= 1.0f || level <= 0.0f) {
    exp_rise = exp_fall = -1;
    return;
  }
  float s = std::acos(1 - 2 * level) / 3.14159265f;
  uint32_t value = (uint32_t) roundf(s * 65535);
  uint32_t min_us = CYCLE * 100 / 1000;
  if (method == 2) {
    exp_rise = 1;
    exp_fall = std::max((uint32_t) 10, value * (CYCLE - min_us) / 65535 + min_us);
    return;
  }
  uint32_t en = std::max((uint32_t) 1, ((65535 - value) * (CYCLE - min_us)) / 65535);
  exp_rise = en;
  exp_fall = method == 0 ? std::max(en + 50, CYCLE / 10) : CYCLE;
}

static void start() {
  reset_stats();
  pinMode(GATE_IN, INPUT);
  attachInterrupt(ZC_PIN, zc_isr, FALLING);
  attachInterrupt(GATE_IN, gate_isr, CHANGE);
}

static const char *phase_name = "boot";
static float cur_level = 0;

static void format_err(char *buf, size_t len, char p, const ErrStat &e) {
  snprintf(buf, len, "%cn=%u %coff=%u %cmin=%d %cmax=%d %cmean=%.1f %csd=%.1f", p, e.n, p, e.off, p, e.n ? e.min : 0, p,
           e.n ? e.max : 0, p, e.mean(), p, e.sd());
}

static void report(const char *tag) {
  noInterrupts();
  Stats s = st;
  reset_stats();  // turns interrupts back on after the reset
  ESP_LOGI("acdm",
           "WIN %s phase=%s level=%.0f zc=%u glitch=%u rises=%u falls=%u missed=%u dbl=%u hi_at_zc=%u exp_rise=%d "
           "exp_fall=%d",
           tag, phase_name, cur_level * 100, s.zc, s.glitch, s.rises, s.falls, s.missed, s.dbl, s.high_at_zc,
           (int) exp_rise, (int) exp_fall);
  // Every key is unique so the parsers read the line as one key=value dict (r = rise, f = fall, w = gate width).
  char rise[96], fall[96];
  format_err(rise, sizeof(rise), 'r', s.rise);
  format_err(fall, sizeof(fall), 'f', s.fall);
  ESP_LOGI("acdm", "ERR %s phase=%s level=%.0f %s %s wmin=%u wmax=%u heap=%u", tag, phase_name, cur_level * 100, rise,
           fall, s.falls ? s.w_min : 0, s.falls ? s.w_max : 0, (unsigned) ESP.getFreeHeap());
}

// Noise: one pulse on the ZC line per call, at a rotating offset and width after the last real crossing.
static constexpr uint16_t OFFSETS[] = {2000, 4000, 6500, 8000};
static constexpr uint16_t WIDTHS[] = {5, 50, 300, 1000};
static uint32_t glitch_idx = 0;
static void inject() {
  uint16_t off = OFFSETS[glitch_idx % 4];
  uint16_t w = WIDTHS[(glitch_idx / 4) % 4];
  glitch_idx++;
  // falling edge of the pulse lands at `off` after the crossing
  uint32_t start_at = off - w;
  uint32_t zc = last_zc;
  uint32_t since = micros() - zc;
  if (since > start_at) {
    // wait for the next crossing (at most one half cycle)
    uint32_t t0 = micros();
    while (last_zc == zc && micros() - t0 < 12000) {
    }
    zc = last_zc;
    since = micros() - zc;
    if (since > start_at)
      return;
  }
  delayMicroseconds(start_at - since);
  injecting = true;
  GPOS = 1 << ZC_PIN;
  delayMicroseconds(w);
  GPOC = 1 << ZC_PIN;
  delayMicroseconds(20);
  injecting = false;
}

static constexpr float LEVELS[] = {0.10f, 0.25f, 0.50f, 0.75f, 1.0f};
static uint32_t secs = 0;
static bool noise = false;
static constexpr const char *PHASES[] = {"clean", "noise"};
static constexpr uint32_t BOOT_S = 15, STEP_S = 22, STEPS = 10, HOLD_T = STEPS * STEP_S, HOLD_REPORT_S = 20;

static void enter(esphome::output::FloatOutput *out, const char *ph, float lvl, int method) {
  noise = false;
  phase_name = ph;
  cur_level = lvl;
  out->set_level(lvl);
  set_expected(lvl, method);
}

// Called every second. BOOT_S boot, then a clean and a noisy sweep (STEP_S per level, first 2 s not counted),
// then hold 50 % clean with a report every HOLD_REPORT_S. run.sh starts its hammer on the HOLD start line.
static void tick(esphome::output::FloatOutput *out, int method) {
  secs++;
  if (secs < BOOT_S)
    return;
  uint32_t t = secs - BOOT_S;
  uint32_t step = t / STEP_S, in = t % STEP_S;
  if (step < STEPS) {
    int ph = step / 5;
    if (in == 0) {
      enter(out, PHASES[ph], LEVELS[step % 5], method);
    } else if (in == 2) {
      reset_stats();
      noise = ph == 1;
    } else if (in == STEP_S - 1) {
      report("sweep");
    }
    return;
  }
  if (t == HOLD_T) {
    enter(out, "hold", 0.5f, method);
    reset_stats();
    ESP_LOGI("acdm", "HOLD start");
    return;
  }
  if ((t - HOLD_T) % HOLD_REPORT_S == 0)
    report("hold");
}
}  // namespace acdm
