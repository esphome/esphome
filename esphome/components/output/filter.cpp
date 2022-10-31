#include "filter.h"
#include "binary_output.h"
#include "float_output.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace output {

// Filter
void Filter::input(float value) {
  optional<float> out = this->new_value(value);
  if (out.has_value())
    this->output(*out);
}

void Filter::output(float value) {
  value = clamp(value, 0.0f, 1.0f);
  if (this->next_ == nullptr) {
#ifdef USE_POWER_SUPPLY
    if (value > 0.0f) {  // ON
      this->parent_->power_.request();
    } else {  // OFF
      this->parent_->power_.unrequest();
    }
#endif

    this->parent_->write_state(value);
  } else {
    this->next_->input(value);
  }
}

void Filter::initialize(BinaryOutput *parent, Filter *next) {
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

}  // namespace output
}  // namespace esphome
