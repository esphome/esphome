#pragma once

#include "esp_color_view.h"
#include "esp_hsv_color.h"

namespace esphome::light {

class ESPRangeIterator;

/**
 * A half-open range of LEDs, inclusive of the begin index and exclusive of the end index, using zero-based numbering.
 */
class ESPRangeView final : public ESPColorSettable {
 public:
  ESPRangeView(ESPColorAccessor *accessor, int32_t begin, int32_t end)
      : accessor_(accessor), begin_(begin), end_(end < begin ? begin : end) {}
  ESPRangeView(const ESPRangeView &) = default;

  int32_t size() const { return this->end_ - this->begin_; }
  ESPColorView operator[](int32_t index) const {
    return ESPColorView(this->accessor_, this->begin_ + this->interpret_index_(index));
  }
  ESPRangeIterator begin();
  ESPRangeIterator end();

  using ESPColorSettable::set;  // ESPHSVColor overload
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

  ESPRangeView &operator=(Color color) {
    this->set(color);
    return *this;
  }
  ESPRangeView &operator=(ESPHSVColor hsv_color) {
    this->set_hsv(hsv_color);
    return *this;
  }
  ESPRangeView &operator=(const ESPColorView &color_view) {
    const Color color = color_view.get();
    this->set(color);
    return *this;
  }
  ESPRangeView &operator=(const ESPRangeView &rhs);

 protected:
  friend ESPRangeIterator;

  inline int32_t interpret_index_(int32_t index) const { return index < 0 ? this->size() + index : index; }

  ESPColorAccessor *const accessor_;
  const int32_t begin_;
  const int32_t end_;
};

class ESPRangeIterator {
 public:
  ESPRangeIterator(ESPColorAccessor *accessor, int32_t index) : accessor_(accessor), index_(index) {}
  ESPRangeIterator(const ESPRangeIterator &) = default;
  ESPRangeIterator operator++() {
    this->index_++;
    return *this;
  }
  bool operator!=(const ESPRangeIterator &other) const { return this->index_ != other.index_; }
  ESPColorView operator*() const { return ESPColorView(this->accessor_, this->index_); }

 protected:
  ESPColorAccessor *const accessor_;
  int32_t index_;
};

inline ESPRangeIterator ESPRangeView::begin() { return {this->accessor_, this->begin_}; }
inline ESPRangeIterator ESPRangeView::end() { return {this->accessor_, this->end_}; }

}  // namespace esphome::light
