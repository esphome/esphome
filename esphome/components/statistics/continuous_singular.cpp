#include "continuous_singular.h"

#include "aggregate.h"

namespace esphome {
namespace statistics {

void ContinuousSingular::clear() {
  this->running_aggregate_ = Aggregate();
  this->size_ = 0;
};

void ContinuousSingular::insert(Aggregate value) {
  this->running_aggregate_ = this->running_aggregate_.combine_with(value, this->time_weighted_);
  ++this->size_;
}

bool ContinuousSingular::set_capacity(size_t capacity, EnabledAggregatesConfiguration config) {
  this->clear();
  return true;
}

}  // namespace statistics
}  // namespace esphome
