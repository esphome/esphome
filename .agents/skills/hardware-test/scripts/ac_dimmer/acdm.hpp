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

struct Stats {
  uint32_t zc, glitch, rises, falls, missed, dbl, high_at_zc;
  uint32_t rise_n, rise_off;  // rises compared with the expected delay, and those more than 100 us off
  int32_t rise_min, rise_max;
  int64_t rise_sum;
  uint64_t rise_sq;
  uint32_t fall_n, fall_off;
  int32_t fall_min, fall_max;
  int64_t fall_sum;
  uint64_t fall_sq;
  uint32_t w_min, w_max;
};

static volatile bool injecting = false;
static volatile uint32_t last_zc = 0, last_rise = 0, rise_zc = 0;
static volatile uint32_t rises_this_half = 0;
static volatile int32_t exp_rise = -1, exp_fall = -1;  // relative to the zero crossing; -1 = not checked
static Stats st;

static void reset_stats() {
  noInterrupts();
  memset(&st, 0, sizeof(st));
  st.rise_min = st.fall_min = INT32_MAX;
  st.rise_max = st.fall_max = INT32_MIN;
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
    if (exp_rise >= 0) {
      int32_t err = (int32_t) (now - last_zc) - exp_rise;
      st.rise_n++;
      st.rise_sum += err;
      st.rise_sq += (int64_t) err * err;
      if (err < st.rise_min)
        st.rise_min = err;
      if (err > st.rise_max)
        st.rise_max = err;
      if (err > 100 || err < -100)
        st.rise_off++;
    }
  } else {
    st.falls++;
    uint32_t w = now - last_rise;
    if (w < st.w_min)
      st.w_min = w;
    if (w > st.w_max)
      st.w_max = w;
    if (exp_fall >= 0) {
      int32_t err = (int32_t) (now - rise_zc) - exp_fall;
      st.fall_n++;
      st.fall_sum += err;
      st.fall_sq += (int64_t) err * err;
      if (err < st.fall_min)
        st.fall_min = err;
      if (err > st.fall_max)
        st.fall_max = err;
      if (err > 100 || err < -100)
        st.fall_off++;
    }
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

static void report(const char *tag) {
  noInterrupts();
  Stats s = st;
  interrupts();
  reset_stats();
  auto sd = [](int64_t sum, uint64_t sq, uint32_t n) -> float {
    if (n == 0)
      return 0;
    double m = (double) sum / n;
    double v = (double) sq / n - m * m;
    return v > 0 ? sqrt(v) : 0;
  };
  ESP_LOGI("acdm",
           "WIN %s phase=%s level=%.0f zc=%u glitch=%u rises=%u falls=%u missed=%u dbl=%u hi_at_zc=%u exp_rise=%d "
           "exp_fall=%d",
           tag, phase_name, cur_level * 100, s.zc, s.glitch, s.rises, s.falls, s.missed, s.dbl, s.high_at_zc,
           (int) exp_rise, (int) exp_fall);
  ESP_LOGI("acdm",
           "ERR %s phase=%s level=%.0f rise n=%u off=%u min=%d max=%d mean=%.1f sd=%.1f | fall n=%u off=%u min=%d "
           "max=%d mean=%.1f sd=%.1f | width min=%u max=%u heap=%u",
           tag, phase_name, cur_level * 100, s.rise_n, s.rise_off, s.rise_n ? s.rise_min : 0, s.rise_n ? s.rise_max : 0,
           s.rise_n ? (double) s.rise_sum / s.rise_n : 0.0, sd(s.rise_sum, s.rise_sq, s.rise_n), s.fall_n, s.fall_off,
           s.fall_n ? s.fall_min : 0, s.fall_n ? s.fall_max : 0, s.fall_n ? (double) s.fall_sum / s.fall_n : 0.0,
           sd(s.fall_sum, s.fall_sq, s.fall_n), s.falls ? s.w_min : 0, s.falls ? s.w_max : 0,
           (unsigned) ESP.getFreeHeap());
}

// Noise: one pulse on the ZC line per call, at a rotating offset and width after the last real crossing.
static const uint16_t OFFSETS[] = {2000, 4000, 6500, 8000};
static const uint16_t WIDTHS[] = {5, 50, 300, 1000};
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

static const float LEVELS[] = {0.10f, 0.25f, 0.50f, 0.75f, 1.0f};
static uint32_t secs = 0;
static bool noise = false;
static const char *PHASES[] = {"clean", "noise"};
// Called every second. 15 s boot, then a clean and a noisy sweep (22 s per level, first 2 s not counted),
// then hold 50 % clean with a report every 20 s.
static void tick(esphome::output::FloatOutput *out, int method) {
  secs++;
  if (secs < 15)
    return;
  uint32_t t = secs - 15;
  uint32_t step = t / 22, in = t % 22;
  if (step < 10) {
    int ph = step / 5;
    float lvl = LEVELS[step % 5];
    if (in == 0) {
      noise = false;
      phase_name = PHASES[ph];
      cur_level = lvl;
      out->set_level(lvl);
      set_expected(lvl, method);
    } else if (in == 2) {
      reset_stats();
      noise = ph == 1;
    } else if (in == 21) {
      report("sweep");
    }
    return;
  }
  if (step == 10 && in == 0) {
    noise = false;
    phase_name = "hold";
    cur_level = 0.5f;
    out->set_level(0.5f);
    set_expected(0.5f, method);
    reset_stats();
    return;
  }
  if ((t - 220) % 20 == 0)
    report("hold");
}
}  // namespace acdm
