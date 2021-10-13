#pragma once

#include "esp_color_view.h"
#include "esp_hsv_color.h"

namespace esphome {
namespace light {

int32_t interpret_index(int32_t index, int32_t size);

class AddressableLight;
class ESPRangeIterator;

/**
 * A half-open range of LEDs, inclusive of the begin index and exclusive of the end index, using zero-based numbering.
 */
class ESPRangeView : public ESPColorSettable {
 public:
  ESPRangeView(AddressableLight *parent, int32_t begin, int32_t end)
      : parent_(parent), begin_(begin), end_(end < begin ? begin : end) {}
  ESPRangeView(const ESPRangeView &) = default;

  int32_t size() const { return this->end_ - this->begin_; }
  ESPColorView operator[](int32_t index) const;
  ESPRangeIterator begin();
  ESPRangeIterator end();

  void set(const Color &color) override;
  void set(const ESPHSVColor &color) { this->set(color.to_rgb()); }
  void set_red(uint8_t red) override;
  void set_green(uint8_t green) override;
  void set_blue(uint8_t blue) override;
  void set_white(uint8_t white) override;
  void set_effect_data(uint8_t effect_data) override;

  void fade_to_white(uint8_t amnt) override;
  void fade_to_black(uint8_t amnt) override;
  void lighten(uint8_t delta) override;
  void darken(uint8_t delta) override;

  ESPRangeView &operator=(const Color &rhs) {
    this->set(rhs);
    return *this;
  }
  ESPRangeView &operator=(const ESPColorView &rhs) {
    this->set(rhs.get());
    return *this;
  }
  ESPRangeView &operator=(const ESPHSVColor &rhs) {
    this->set_hsv(rhs);
    return *this;
  }
  ESPRangeView &operator=(const ESPRangeView &rhs);

 protected:
  friend ESPRangeIterator;

  AddressableLight *parent_;
  int32_t begin_;
  int32_t end_;
};

class ESPRangeIterator {
 public:
  ESPRangeIterator(const ESPRangeView &range, int32_t i) : range_(range), i_(i) {}
  ESPRangeIterator(const ESPRangeIterator &) = default;
  ESPRangeIterator operator++() {
    this->i_++;
    return *this;
  }
  bool operator!=(const ESPRangeIterator &other) const { return this->i_ != other.i_; }
  ESPColorView operator*() const;

 protected:
  ESPRangeView range_;
  int32_t i_;
};

}  // namespace light
}  // namespace esphome
