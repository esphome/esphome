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
  // Guarded by #ifdef USE_BINARY_SENSOR: entity_types.h only generates that ControllerRegistry
  // member when at least one binary_sensor exists anywhere in the device's config. A device with
  // none at all would otherwise fail to compile, even though a non-null bit here can only occur
  // when a binary_sensor was actually configured -- which itself requires USE_BINARY_SENSOR to be
  // defined (see hub.cpp's invalidate_entity() helpers for the same pattern on other domains).
  void invalidate() {
    for (auto *b : this->bits) {
      if (b != nullptr) {
        b->set_has_state(false);
#ifdef USE_BINARY_SENSOR
        ControllerRegistry::notify_binary_sensor_update(b);
#endif
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
  // above, this deliberately skips notifying already-connected clients: Home Assistant's switch
  // entity (homeassistant/components/esphome/switch.py's is_on) never checks the missing_state
  // flag, unlike sensor/binary_sensor/number/select, so it always reads back as off regardless of
  // has_state() -- there is currently no Home Assistant-visible effect to notify for. This still
  // sets the flag for any other client that does honor it (e.g. a future Home Assistant fix, or a
  // third-party API consumer).
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
