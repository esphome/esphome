#pragma once

#include <cmath>
#include <cstdint>

namespace esphome::ledc {

/// Ticks for a constant high output, one past the highest PWM level.
inline uint32_t full_scale_ticks(uint8_t bit_depth, uint8_t fractional_bits) {
  return uint32_t(1) << (bit_depth + fractional_bits);
}

/// Convert a 0-1 duty to timer ticks, with fractional_bits extra bits for dithering.
inline uint32_t duty_to_ticks(float state, uint8_t bit_depth, uint8_t fractional_bits) {
  const uint32_t max_duty = full_scale_ticks(bit_depth, fractional_bits);
  const float scaled_duty = state * max_duty;
  auto duty = static_cast<uint32_t>(roundf(scaled_duty));
  if (fractional_bits != 0) {
    // Fractional values above the last whole tick can wrap the PWM output instead of approaching 100%.
    const uint32_t highest_pwm = max_duty - (1U << fractional_bits);
    if (duty > highest_pwm) {
      const uint32_t midpoint = highest_pwm + (1U << (fractional_bits - 1));
      duty = scaled_duty < midpoint ? highest_pwm : max_duty;
    }
  }
  return duty;
}

}  // namespace esphome::ledc
