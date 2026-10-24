#include "esp_range_view.h"
#include "addressable_light.h"

namespace esphome::light {

ESPColorView ESPRangeView::operator[](int32_t index) const {
  return (*this->parent_)[this->interpret_index_(index) + this->begin_];
}

void ESPRangeView::set(Color color) {
  for (int32_t i = this->begin_; i < this->end_; i++) {
    (*this->parent_)[i] = color;
  }
}

void ESPRangeView::set_red(uint8_t red) {
  for (auto c : *this)
    c.set_red(red);
}
void ESPRangeView::set_green(uint8_t green) {
  for (auto c : *this)
    c.set_green(green);
}
void ESPRangeView::set_blue(uint8_t blue) {
  for (auto c : *this)
    c.set_blue(blue);
}
void ESPRangeView::set_white(uint8_t white) {
  for (auto c : *this)
    c.set_white(white);
}
void ESPRangeView::set_effect_data(uint8_t effect_data) {
  for (auto c : *this)
    c.set_effect_data(effect_data);
}

void ESPRangeView::fade_to_white(uint8_t amnt) {
  for (auto c : *this)
    c.fade_to_white(amnt);
}
void ESPRangeView::fade_to_black(uint8_t amnt) {
  for (auto c : *this)
    c.fade_to_black(amnt);
}
void ESPRangeView::lighten(uint8_t delta) {
  for (auto c : *this)
    c.lighten(delta);
}
void ESPRangeView::darken(uint8_t delta) {
  for (auto c : *this)
    c.darken(delta);
}
ESPRangeView &ESPRangeView::operator=(const ESPRangeView &rhs) {  // NOLINT
  // If size doesn't match, error (todo warning)
  if (rhs.size() != this->size())
    return *this;

  if (this->parent_ != rhs.parent_) {
    for (int32_t i = 0; i < this->size(); i++)
      (*this)[i].set(rhs[i].get());
    return *this;
  }

  // If both equal, already done
  if (rhs.begin_ == this->begin_)
    return *this;

  // The source is read before the destination view is fetched: a partition's views share one correction
  if (rhs.begin_ > this->begin_) {
    // Copy from left
    for (int32_t i = 0; i < this->size(); i++) {
      const Color color = rhs[i].get();
      (*this)[i].set(color);
    }
  } else {
    // Copy from right
    for (int32_t i = this->size() - 1; i >= 0; i--) {
      const Color color = rhs[i].get();
      (*this)[i].set(color);
    }
  }

  return *this;
}

ESPColorView ESPRangeIterator::operator*() const { return this->range_.parent_->get(this->i_); }

}  // namespace esphome::light
