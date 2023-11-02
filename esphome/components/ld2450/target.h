#pragma once

#include "esphome.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <stdint.h>
#include <string>

namespace esphome {
namespace ld2450 {

class PresenceZone;

/**
 * Two helper functions to help convert the 16 bit buffered numbers into usable
 * values. Making these inline is extremely valuable, and results in both
 * functionstaking approximately two instructions (basically xor'ing itself).
 * Can't get much faster than that.
 */
inline int16_t convert_signed(const uint8_t *two_bytes) {
  int16_t val = (two_bytes[1] << 8) | two_bytes[0];
  // The first bit in the second byte is used to indicate a negative.
  return ((two_bytes[1] & 0x80) >> 7) ? val - 32768 : 0 - val;
}

inline uint16_t convert_unsigned(const uint8_t *two_bytes) { return (two_bytes[1] << 8) | two_bytes[0]; }

class Target {
 public:
  /**
   * Publish values to sensors.
   */
  void publish() const;

  /**
   * Parses the incoming 8-byte record and replace this classes contents
   */
  void update_from_buffer(const uint8_t *buffer);

  bool valid() const { return valid_; }

  friend class PresenceZone;

 protected:
#if defined(USE_SENSOR) || defined(USE_BINARY_SENSOR) || defined(USE_TEXT_SENSOR)
#define USE_TARGET_COORDS
  int16_t x_ = 0;
  int16_t y_ = 0;

  float get_angle() const;

#ifdef USE_SENSOR
  SUB_SENSOR(angle)
  SUB_SENSOR(x)
  SUB_SENSOR(y)
#endif
#endif

#if defined(USE_SENSOR) || defined(USE_TEXT_SENSOR)
#define USE_TARGET_SPEED
  int16_t speed_ = 0;

  const std::string &get_position() const;

#ifdef USE_SENSOR
  SUB_SENSOR(speed)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(position)
#endif
#endif

#if defined(USE_SENSOR)
#define USE_TARGET_RESOLUTION
  uint16_t resolution_ = 0;

  const std::string &get_direction() const;

#ifdef USE_SENSOR
  SUB_SENSOR(resolution)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(direction)
#endif
#endif

  bool valid_ = false;

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(any_presence)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(all_target_counts)
#endif
};

}  // namespace ld2450
}  // namespace esphome
