#pragma once
// Drives the hub inputs on the D1 mini's own pads: open drain outputs with the input still read by the hubs.
#include <Arduino.h>
#include "esphome/core/log.h"

namespace sim {
// Matrix keypad column held low reads as a pressed key in the single row.
inline void press(uint8_t pin) {
  pinMode(pin, OUTPUT_OPEN_DRAIN);
  digitalWrite(pin, LOW);
}
inline void release(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }

static constexpr uint8_t D0 = 13, D1 = 0;
inline void open_drain_high(uint8_t pin) {
  GPOS = (1 << pin);  // output latch high first, so enabling the driver makes no falling edge
  pinMode(pin, OUTPUT_OPEN_DRAIN);
  GPF(pin) |= (1 << GPFPU);  // pinMode clears the pull-up
  digitalWrite(pin, HIGH);
}
// Wiegand frame, MSB first: a 0 bit pulses D0 low, a 1 bit pulses D1 low.
inline void wiegand_arm() {
  open_drain_high(D0);
  open_drain_high(D1);
}
inline void wiegand(uint64_t value, uint8_t bits) {
  for (int i = bits - 1; i >= 0; i--) {
    uint8_t pin = (value >> i) & 1 ? D1 : D0;
    digitalWrite(pin, LOW);
    delayMicroseconds(80);
    digitalWrite(pin, HIGH);
    delayMicroseconds(1500);
  }
  ESP_LOGI("sim", "sent %u-bit wiegand %llx", bits, (unsigned long long) value);
}
}  // namespace sim
