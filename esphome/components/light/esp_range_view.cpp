#include "esp_range_view.h"

namespace esphome::light {

void ESPRangeView::set(Color color) {
  for (int32_t i = this->begin_; i < this->end_; i++)
    this->accessor_->set(i, color);
}
void ESPRangeView::set_channel(uint8_t channel, uint8_t value) {
  for (int32_t i = this->begin_; i < this->end_; i++)
    this->accessor_->set_channel(i, channel, value);
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

  if (this->accessor_ != rhs.accessor_) {
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

}  // namespace esphome::light
