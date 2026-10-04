#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::ld2450 {

static constexpr uint8_t MIN_POLYGON_POINTS = 3;
// The most points whose text always fits Home Assistant's 255-character limit: 23 x "-4860,7560" + 22 x ';' = 252
static constexpr uint8_t MAX_POLYGON_POINTS = 23;
// Coordinate limits of the radar detection area, in mm
static constexpr int16_t POLYGON_MIN_X = -4860;
static constexpr int16_t POLYGON_MAX_X = 4860;
static constexpr int16_t POLYGON_MIN_Y = 0;
static constexpr int16_t POLYGON_MAX_Y = 7560;
// Longest point is "-4860,7560;" (11 characters), plus the null terminator
static constexpr size_t POLYGON_STR_SIZE = MAX_POLYGON_POINTS * 11 + 1;

struct Point {
  int16_t x;
  int16_t y;
};

/// A polygon in radar coordinates. Trivially copyable so it can be saved to preferences as-is.
struct Polygon {
  std::array<Point, MAX_POLYGON_POINTS> points{};
  uint8_t count{0};

  /// True when the polygon has no points, which disables the zone.
  bool empty() const { return this->count == 0; }

  /// True when the polygon is empty, or has a valid number of points that are all inside the radar area.
  bool is_valid() const;

  /// Return whether the point is inside the polygon (even-odd rule). Always false for an empty polygon.
  bool contains(int16_t x, int16_t y) const;

  /** Parse "x,y;x,y;..." (mm) into this polygon.
   *
   * Spaces around numbers and a trailing ';' are allowed. An empty or all-space string gives an empty polygon.
   * @return false if the text is invalid; the polygon is then left unchanged.
   */
  bool parse(const char *str, size_t len);

  /// Write the polygon as "x,y;x,y;..." into buf, which must hold at least POLYGON_STR_SIZE bytes.
  void format(char *buf) const;
};

}  // namespace esphome::ld2450
