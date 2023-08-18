#include "continuous_singular.h"

#include "aggregate.h"

namespace esphome {
namespace statistics {

void ContinuousSingular::clear() {
  *this->running_aggregate_ = Aggregate(this->statistics_calculation_config_);
  this->size_ = 0;
};

void ContinuousSingular::insert(Aggregate value) {
  *this->running_aggregate_ = this->running_aggregate_->combine_with(value);
  ++this->size_;
}

bool ContinuousSingular::set_capacity(size_t capacity, TrackedStatisticsConfiguration tracked_statistics_config) {
  // Verify running_aggregate was successfully allocated during class construction
  if (this->running_aggregate_ == nullptr)
    return false;

  this->clear();
  return true;
}

}  // namespace statistics
}  // namespace esphome
