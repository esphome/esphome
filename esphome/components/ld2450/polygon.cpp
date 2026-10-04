#include "polygon.h"

#include <cstdio>

namespace esphome::ld2450 {

namespace {

class Scanner {
 public:
  Scanner(const char *str, size_t len) : pos_(str), end_(str + len) {}

  bool at_end() const { return this->pos_ == this->end_; }

  void skip_spaces() {
    while (this->pos_ != this->end_ && (*this->pos_ == ' ' || *this->pos_ == '\t'))
      this->pos_++;
  }

  bool consume(char c) {
    this->skip_spaces();
    if (this->pos_ != this->end_ && *this->pos_ == c) {
      this->pos_++;
      return true;
    }
    return false;
  }

  /// Read an integer in [min, max]; returns false if there is none or it is out of range.
  bool read_int(int16_t min, int16_t max, int16_t &out) {
    this->skip_spaces();
    bool negative = this->consume('-');
    int32_t value = 0;
    uint8_t digits = 0;
    while (this->pos_ != this->end_ && *this->pos_ >= '0' && *this->pos_ <= '9') {
      // Six digits is already beyond any valid coordinate; stop before value can overflow
      if (++digits > 6)
        return false;
      value = value * 10 + (*this->pos_ - '0');
      this->pos_++;
    }
    if (digits == 0)
      return false;
    if (negative)
      value = -value;
    if (value < min || value > max)
      return false;
    out = static_cast<int16_t>(value);
    return true;
  }

 protected:
  const char *pos_;
  const char *end_;
};

}  // namespace

bool Polygon::is_valid() const {
  if (this->count == 0)
    return true;
  if (this->count < MIN_POLYGON_POINTS || this->count > MAX_POLYGON_POINTS)
    return false;
  for (uint8_t i = 0; i < this->count; i++) {
    const Point &p = this->points[i];
    if (p.x < POLYGON_MIN_X || p.x > POLYGON_MAX_X || p.y < POLYGON_MIN_Y || p.y > POLYGON_MAX_Y)
      return false;
  }
  return true;
}

bool Polygon::contains(int16_t x, int16_t y) const {
  bool inside = false;
  for (uint8_t i = 0, j = this->count - 1; i < this->count; j = i++) {
    const Point &a = this->points[i];
    const Point &b = this->points[j];
    if ((a.y > y) == (b.y > y))
      continue;
    // Does the edge a-b cross the ray going from (x, y) towards +x? Compare
    // x - a.x < (y - a.y) * (b.x - a.x) / (b.y - a.y) without division; values fit in 32 bits.
    const int32_t dy = static_cast<int32_t>(b.y) - a.y;
    const int32_t lhs = (static_cast<int32_t>(x) - a.x) * dy;
    const int32_t rhs = (static_cast<int32_t>(y) - a.y) * (static_cast<int32_t>(b.x) - a.x);
    if (dy > 0 ? lhs < rhs : lhs > rhs)
      inside = !inside;
  }
  return inside;
}

bool Polygon::parse(const char *str, size_t len) {
  Polygon result;
  Scanner scanner(str, len);
  scanner.skip_spaces();
  while (!scanner.at_end()) {
    if (result.count == MAX_POLYGON_POINTS)
      return false;
    Point &p = result.points[result.count];
    if (!scanner.read_int(POLYGON_MIN_X, POLYGON_MAX_X, p.x) || !scanner.consume(',') ||
        !scanner.read_int(POLYGON_MIN_Y, POLYGON_MAX_Y, p.y))
      return false;
    result.count++;
    bool separator = scanner.consume(';');
    scanner.skip_spaces();
    if (!separator && !scanner.at_end())
      return false;
  }
  if (!result.is_valid())
    return false;
  *this = result;
  return true;
}

void Polygon::format(char *buf) const {
  char *pos = buf;
  buf[0] = '\0';
  for (uint8_t i = 0; i < this->count; i++) {
    // Coordinates are checked on parse and load, so each point fits in its 11-character share of the buffer
    pos += snprintf(pos, POLYGON_STR_SIZE - (pos - buf), "%s%d,%d", i == 0 ? "" : ";", this->points[i].x,
                    this->points[i].y);
  }
}

}  // namespace esphome::ld2450
