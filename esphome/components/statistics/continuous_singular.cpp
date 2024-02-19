#include "continuous_singular.h"

#include "aggregate.h"

namespace esphome {
namespace statistics {

void ContinuousSingular::clear() {
  this->running_aggregate_->clear();
  this->size_ = 0;
};

void ContinuousSingular::insert(Aggregate value) {
  *this->running_aggregate_ += value;
  ++this->size_;
}

bool ContinuousSingular::configure_capacity(size_t capacity, TrackedStatisticsConfiguration tracked_statistics_config) {
  // Verify running_aggregate was successfully allocated during class construction
  if (this->running_aggregate_ == nullptr)
    return false;

  this->clear();
  return true;
}

}  // namespace statistics
}  // namespace esphome
