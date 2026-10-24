#pragma once

#include "esp_color_view.h"
#include "esp_hsv_color.h"

namespace esphome::light {

class AddressableLight;
class ESPRangeIterator;

/**
 * A half-open range of LEDs, inclusive of the begin index and exclusive of the end index, using zero-based numbering.
 */
class ESPRangeView final : public ESPColorSettable {
 public:
  ESPRangeView(AddressableLight *parent, int32_t begin, int32_t end)
      : parent_(parent), begin_(begin), end_(end < begin ? begin : end) {}
  ESPRangeView(const ESPRangeView &) = default;

  int32_t size() const { return this->end_ - this->begin_; }
  ESPColorView operator[](int32_t index) const;
  ESPRangeIterator begin();
  ESPRangeIterator end();

  void set(Color color) override;
  void set_red(uint8_t red) override;
  void set_green(uint8_t green) override;
  void set_blue(uint8_t blue) override;
  void set_white(uint8_t white) override;
  void set_effect_data(uint8_t effect_data) override;

  void fade_to_white(uint8_t amnt) override;
  void fade_to_black(uint8_t amnt) override;
  void lighten(uint8_t delta) override;
  void darken(uint8_t delta) override;

  inline ESPRangeView &operator=(Color color) {
    this->set(color);
    return *this;
  }
  inline ESPRangeView &operator=(ESPHSVColor hsv_color) {
    this->set_hsv(hsv_color);
    return *this;
  }
  inline ESPRangeView &operator=(const ESPColorView &color_view) {
    this->set(color_view.get());
    return *this;
  }
  ESPRangeView &operator=(const ESPRangeView &rhs);

 protected:
  friend ESPRangeIterator;

  inline int32_t interpret_index_(int32_t index) const { return index < 0 ? this->size() + index : index; }

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

inline ESPRangeIterator ESPRangeView::begin() { return {*this, this->begin_}; }
inline ESPRangeIterator ESPRangeView::end() { return {*this, this->end_}; }

}  // namespace esphome::light
