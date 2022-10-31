#include "binary_output.h"

namespace esphome {
namespace output {

void BinaryOutput::add_filter(Filter *filter) {
  if (this->filter_list_ == nullptr) {
    this->filter_list_ = filter;
  } else {
    Filter *last_filter = this->filter_list_;
    while (last_filter->next_ != nullptr)
      last_filter = last_filter->next_;
    last_filter->initialize(this, filter);
  }
  filter->initialize(this, nullptr);
}
void BinaryOutput::add_filters(const std::vector<Filter *> &filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
void BinaryOutput::set_filters(const std::vector<Filter *> &filters) {
  this->clear_filters();
  this->add_filters(filters);
}
void BinaryOutput::clear_filters() { this->filter_list_ = nullptr; }

void BinaryOutput::write_state(float state) {
  if (state > 0.0f) {  // ON
    this->write_state(true);
  } else {
    this->write_state(false);
  }
}

}  // namespace output
}  // namespace esphome
