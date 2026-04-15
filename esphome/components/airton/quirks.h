#pragma once

#ifndef AIRTON_QUIRKS_H
#define AIRTON_QUIRKS_H

#include <cstdint>
#include <string>
#include "esphome/core/log.h"

namespace esphome {
namespace airton {

// Vertical direction for air outlet flap
class VerticalDirection {
 public:
  enum Direction {
    VERTICAL_DIRECTION_OFF = 0x00,
    VERTICAL_DIRECTION_SWING = 0x01,
    VERTICAL_DIRECTION_UP = 0x02,
    VERTICAL_DIRECTION_MIDDLE_UP = 0x03,
    VERTICAL_DIRECTION_MIDDLE = 0x04,
    VERTICAL_DIRECTION_MIDDLE_DOWN = 0x05,
    VERTICAL_DIRECTION_DOWN = 0x06,
  };

  VerticalDirection(Direction direction = Direction::VERTICAL_DIRECTION_OFF) : direction_(direction) {}

  const char *to_string() const {
    switch (direction_) {
      case Direction::VERTICAL_DIRECTION_SWING:
        return "swing";
      case Direction::VERTICAL_DIRECTION_UP:
        return "up";
      case Direction::VERTICAL_DIRECTION_MIDDLE_UP:
        return "middle-up";
      case Direction::VERTICAL_DIRECTION_MIDDLE:
        return "middle";
      case Direction::VERTICAL_DIRECTION_MIDDLE_DOWN:
        return "middle-down";
      case Direction::VERTICAL_DIRECTION_DOWN:
        return "down";
      case Direction::VERTICAL_DIRECTION_OFF:
      default:
        return "off";
    }
  }

  uint8_t to_uint8() const { return static_cast<uint8_t>(direction_); }

  void set_direction(Direction direction) { direction_ = direction; }
  void set_direction(const std::string &direction) {
    if (direction == "swing") {
      direction_ = Direction::VERTICAL_DIRECTION_SWING;
    } else if (direction == "up") {
      direction_ = Direction::VERTICAL_DIRECTION_UP;
    } else if (direction == "middle-up") {
      direction_ = Direction::VERTICAL_DIRECTION_MIDDLE_UP;
    } else if (direction == "middle") {
      direction_ = Direction::VERTICAL_DIRECTION_MIDDLE;
    } else if (direction == "middle-down") {
      direction_ = Direction::VERTICAL_DIRECTION_MIDDLE_DOWN;
    } else if (direction == "down") {
      direction_ = Direction::VERTICAL_DIRECTION_DOWN;
    } else {
      direction_ = Direction::VERTICAL_DIRECTION_OFF;
    }
  }

  Direction get_direction() const { return direction_; }

 private:
  Direction direction_;
};

}  // namespace airton
}  // namespace esphome

#endif  // AIRTON_QUIRKS_H
