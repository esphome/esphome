#pragma once

#include <array>
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/controller_registry.h"

namespace esphome::opentherm42 {

// A flag8 byte (§5.1) the master reads from the boiler, exposed one bit at a time as
// binary_sensors. Bits with no configured entity (nullptr) are simply not reported.
struct FlagReadBits {
  std::array<binary_sensor::BinarySensor *, 8> bits{};

  void publish(uint8_t value) {
    for (uint8_t i = 0; i < 8; i++) {
      if (this->bits[i] != nullptr) {
        this->bits[i]->publish_state((value >> i) & 1);
      }
    }
  }

  // Called when the conversation that would have supplied this byte failed -- every configured bit
  // must show unknown rather than keep stale data. set_has_state(false) alone doesn't notify
  // already-connected API/web_server clients (only publish_state() does, via each domain's own
  // internal call to ControllerRegistry) -- an already-subscribed client would otherwise keep
  // showing the last known value forever, so that notification has to be triggered explicitly here.
  void invalidate() {
    for (auto *b : this->bits) {
      if (b != nullptr) {
        b->set_has_state(false);
        ControllerRegistry::notify_binary_sensor_update(b);
      }
    }
  }

  // Whether any bit has a configured entity -- used to decide whether this byte's conversation is
  // worth scheduling at all.
  bool any_configured() const {
    for (auto *b : this->bits) {
      if (b != nullptr) {
        return true;
      }
    }
    return false;
  }
};

// A flag8 byte (§5.1) the master writes to the boiler, sourced one bit at a time from switches.
// Bits with no configured entity (nullptr) are sent as 0.
struct FlagWriteBits {
  std::array<switch_::Switch *, 8> bits{};

  uint8_t pack() const {
    uint8_t value = 0;
    for (uint8_t i = 0; i < 8; i++) {
      if (this->bits[i] != nullptr && this->bits[i]->state) {
        value |= 1 << i;
      }
    }
    return value;
  }

  // Called when the boiler has definitively rejected (DATA-INVALID/UNKNOWN-DATAID) the
  // conversation that carries this byte -- unlike a transient datalink error, this means the
  // underlying feature genuinely isn't present on this hardware. Unlike FlagReadBits::invalidate()
  // above, this can't also call ControllerRegistry: switch::Switch isn't wired into it in every
  // ESPHome version this component needs to build against (confirmed missing in a real user's
  // build), and unlike a float-based entity there's no NaN-equivalent sentinel to fall back on for
  // a boolean. So this only updates already-configured switches for a client that reconnects or
  // freshly subscribes afterwards -- an already-connected client keeps showing the last value.
  void invalidate() {
    for (auto *b : this->bits) {
      if (b != nullptr) {
        b->set_has_state(false);
      }
    }
  }

  // Whether any bit has a configured entity -- used to decide whether this byte's conversation is
  // worth scheduling at all.
  bool any_configured() const {
    for (auto *b : this->bits) {
      if (b != nullptr) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace esphome::opentherm42
