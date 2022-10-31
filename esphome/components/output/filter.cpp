#include "filter.h"
#include "binary_output.h"
#include "float_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.filter";

// Filter
void Filter::input(float value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%f)", this, value);
  optional<float> out = this->new_value(value);
  if (out.has_value())
    this->output(*out);
}

void Filter::output(float value) {
  value = clamp(value, 0.0f, 1.0f);
  if (this->next_ == nullptr) {
    ESP_LOGVV(TAG, "Filter(%p)::output(%f) -> OUTPUT", this, value);
#ifdef USE_POWER_SUPPLY
    if (value > 0.0f) {  // ON
      this->parent_->power_.request();
    } else {  // OFF
      this->parent_->power_.unrequest();
    }
#endif

    this->parent_->write_state(value);
  } else {
    ESP_LOGVV(TAG, "Filter(%p)::output(%f) -> %p", this, value, this->next_);
    this->next_->input(value);
  }
}

void Filter::initialize(BinaryOutput *parent, Filter *next) {
  ESP_LOGVV(TAG, "Filter(%p)::initialize(parent=%p next=%p)", this, parent, next);
  this->parent_ = parent;
  this->next_ = next;
}

// InvertedFilter
InvertedFilter::InvertedFilter(bool inverted) { this->inverted_ = inverted_; }

optional<float> InvertedFilter::new_value(float value) {
  if (this->inverted_) {
    return 1.0f - value;
  } else {
    return value;
  }
}

// RangeFilter
RangeFilter::RangeFilter(float min_power, float max_power, bool zero_means_zero) {
  this->min_power_ = min_power;
  this->max_power_ = max_power;
  this->zero_means_zero_ = zero_means_zero;
}

optional<float> RangeFilter::new_value(float value) {
  if (value == 0.0f && this->zero_means_zero_)
    return value;

  return lerp(value, this->min_power_, this->max_power_);
}

}  // namespace output
}  // namespace esphome
