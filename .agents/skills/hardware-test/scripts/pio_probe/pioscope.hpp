#pragma once
// Self-sampling logic probe for rp2040_pio_led_strip tests: a PIO2 state machine samples one GPIO
// (the strip's own data pin, read from the pad while the strip drives it) at about 25 MHz, then
// decodes the high pulses into bits and bytes.
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/sync.h>
#include <pico/time.h>
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_state.h"

namespace pioscope {
static const char *const TAG = "scope";
static constexpr size_t WORDS = 256;  // 8192 samples, about 330 us
inline uint32_t buf[WORDS];
inline int sm = -1;
inline uint offset = 0;
inline float ns_per_sample = 0;
inline uint16_t prog_instr[2];
inline pio_program_t prog{prog_instr, 2, -1};

inline bool init() {
  if (sm >= 0)
    return true;
  prog_instr[0] = pio_encode_wait_pin(true, 0);
  prog_instr[1] = pio_encode_in(pio_pins, 1);
  if (!pio_can_add_program(pio2, &prog)) {
    ESP_LOGE(TAG, "no room in PIO2");
    return false;
  }
  offset = pio_add_program(pio2, &prog);
  sm = pio_claim_unused_sm(pio2, false);
  if (sm < 0) {
    ESP_LOGE(TAG, "no SM in PIO2");
    return false;
  }
  ns_per_sample = 6.0f * 1e9f / clock_get_hz(clk_sys);
  ESP_LOGI(TAG, "probe on PIO2 sm %d, clk_sys %u Hz, %.1f ns per sample", sm, (unsigned) clock_get_hz(clk_sys),
           ns_per_sample);
  return true;
}

inline void arm(uint pin) {
  pio_sm_set_enabled(pio2, sm, false);
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_in_pins(&c, pin);
  sm_config_set_in_shift(&c, false, true, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
  sm_config_set_clkdiv(&c, 6.0f);
  sm_config_set_wrap(&c, offset + 1, offset + 1);
  pio_sm_init(pio2, sm, offset, &c);
  pio_sm_clear_fifos(pio2, sm);
  pio_sm_set_enabled(pio2, sm, true);
}

// Drains the FIFO until WORDS words or 3 ms without a word (no rising edge yet, or done).
inline size_t collect() {
  size_t n = 0;
  uint32_t irq = save_and_disable_interrupts();
  uint32_t last = time_us_32();
  while (n < WORDS) {
    if (!pio_sm_is_rx_fifo_empty(pio2, sm)) {
      buf[n++] = pio_sm_get(pio2, sm);
      last = time_us_32();
    } else if (time_us_32() - last > 3000) {
      break;
    }
  }
  restore_interrupts(irq);
  pio_sm_set_enabled(pio2, sm, false);
  return n;
}

inline void decode(const char *label, size_t words, size_t expect_bits) {
  if (words == 0) {
    ESP_LOGI(TAG, "RESULT %s: no output (pin never went high)", label);
    return;
  }
  // high/low run lengths in samples
  static uint16_t hi[1024], lo[1024];
  size_t pulses = 0, total = words * 32;
  int run = 0;
  bool level = true;
  bool started = false;
  for (size_t i = 0; i < total && pulses < 1024; i++) {
    bool b = buf[i / 32] & (1u << (31 - i % 32));
    if (!started) {
      if (b)
        started = true;
      else
        continue;
    }
    if (b == level) {
      run++;
      continue;
    }
    if (level)
      hi[pulses] = run;
    else {
      lo[pulses] = run;
      pulses++;
    }
    level = b;
    run = 1;
  }
  if (!level && pulses < 1024) {
    lo[pulses] = run;
    pulses++;
  }  // last pulse ends in the trailing low
  uint16_t hmin = 0xFFFF, hmax = 0;
  for (size_t i = 0; i < pulses; i++) {
    hmin = std::min(hmin, hi[i]);
    hmax = std::max(hmax, hi[i]);
  }
  float thr = (hmin + hmax) / 2.0f;
  // per-class average high and low (low of the last bit is the reset gap, skip it)
  double h[2] = {0, 0}, l[2] = {0, 0};
  unsigned nh[2] = {0, 0}, nl[2] = {0, 0};
  uint8_t bytes[64] = {0};
  size_t nbytes = 0;
  for (size_t i = 0; i < pulses; i++) {
    bool one = hi[i] > thr;
    h[one] += hi[i];
    nh[one]++;
    if (i + 1 < pulses) {
      l[one] += lo[i];
      nl[one]++;
    }
    if (i / 8 < sizeof(bytes)) {
      bytes[i / 8] |= (one ? 1 : 0) << (7 - i % 8);
      nbytes = i / 8 + 1;
    }
  }
  char hex[140];
  esphome::format_hex_to(hex, sizeof(hex), bytes, nbytes);
  float ns = ns_per_sample;
  ESP_LOGI(TAG, "RESULT %s: %u pulses (expect %u), bit0 high %.0f low %.0f ns, bit1 high %.0f low %.0f ns, bytes %s",
           label, (unsigned) pulses, (unsigned) expect_bits, nh[0] ? h[0] / nh[0] * ns : 0,
           nl[0] ? l[0] / nl[0] * ns : 0, nh[1] ? h[1] / nh[1] * ns : 0, nl[1] ? l[1] / nl[1] * ns : 0, hex);
}

// Sets every LED of the strip to c, writes it now and decodes the pin.
inline void probe(const char *label, esphome::light::LightState *state, uint pin, esphome::Color c) {
  if (!init())
    return;
  auto *out = static_cast<esphome::light::AddressableLight *>(state->get_output());
  if (out->is_failed()) {
    ESP_LOGI(TAG, "RESULT %s: component failed", label);
    return;
  }
  for (int i = 0; i < out->size(); i++)
    out->get(i).set(c);
  arm(pin);
  out->write_state(state);
  size_t n = collect();
  decode(label, n, out->size() * 24);
}
}  // namespace pioscope
