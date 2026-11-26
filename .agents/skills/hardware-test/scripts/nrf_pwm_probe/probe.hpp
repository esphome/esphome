#pragma once
// Self-sampling PWM probe: reconnects the input buffer of a pin a PWM peripheral drives, then polls the GPIO IN
// register with interrupts locked and times every edge with the DWT cycle counter.
#include <zephyr/kernel.h>
#include <hal/nrf_gpio.h>
#include "esphome/core/log.h"

namespace pwmprobe {
static constexpr const char *const TAG = "probe";

inline void dwt_on() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

inline void calibrate() {
  dwt_on();
  uint32_t a = DWT->CYCCNT;
  k_busy_wait(10000);
  uint32_t b = DWT->CYCCNT;
  ESP_LOGI(TAG, "CAL SystemCoreClock=%u dwt_per_10ms=%u", (unsigned) SystemCoreClock, (unsigned) (b - a));
}

inline void measure(const char *name, uint32_t abs_pin, float set, uint32_t window_ms) {
  uint32_t p = abs_pin;
  NRF_GPIO_Type *reg = nrf_gpio_pin_port_decode(&p);
  reg->PIN_CNF[p] &= ~GPIO_PIN_CNF_INPUT_Msk;  // INPUT = Connect
  const uint32_t mask = 1u << p;
  const uint32_t mhz = SystemCoreClock / 1000000;
  const uint32_t window = window_ms * 1000 * mhz;
  dwt_on();
  uint32_t samples = 0, high_samples = 0, rises = 0, highs = 0, high_total = 0;
  uint32_t first_rise = 0, last_rise = 0, rise_at = 0;
  bool seen_rise = false;
  unsigned key = irq_lock();
  const uint32_t start = DWT->CYCCNT;
  bool last = (reg->IN & mask) != 0;
  while (true) {
    const uint32_t now = DWT->CYCCNT;
    const bool lvl = (reg->IN & mask) != 0;
    samples++;
    if (lvl)
      high_samples++;
    if (lvl != last) {
      if (lvl) {
        if (seen_rise) {
          last_rise = now;
          rises++;
        } else {
          first_rise = now;
          seen_rise = true;
        }
        rise_at = now;
      } else if (seen_rise) {
        high_total += now - rise_at;
        highs++;
      }
      last = lvl;
    }
    if (now - start > window)
      break;
  }
  irq_unlock(key);
  const float sample_ns = 1000.0f * window_ms * 1000.0f / samples;
  if (rises == 0) {
    ESP_LOGI(TAG, "MEAS %s set=%.3f freq=0 high=%.4f const=%d samples=%u sample_ns=%.0f", name, set,
             (float) high_samples / samples, (int) last, (unsigned) samples, sample_ns);
    return;
  }
  const float period = (float) (last_rise - first_rise) / rises;
  const float freq = (float) SystemCoreClock / period;
  const float high = highs ? (float) high_total / highs / period : 0.0f;
  ESP_LOGI(TAG, "MEAS %s set=%.3f freq=%.2f high=%.4f periods=%u samples=%u sample_ns=%.0f", name, set, freq, high,
           (unsigned) rises, (unsigned) samples, sample_ns);
}
}  // namespace pwmprobe
