#include "continuous_queue.h"

#include "aggregate.h"

namespace esphome {
namespace statistics {

//////////////////////////////////////////////////////////
// Overridden virtual methods from AggregateQueue class //
//////////////////////////////////////////////////////////

Aggregate ContinuousQueue::compute_current_aggregate() {
  Aggregate total = Aggregate(this->statistics_calculation_config_);

  // Starts with the most recent Aggregates so that the combine operations have as close to equal weights as possible
  //  - helps avoid floating point precision issues
  for (int i = this->index_ - 1; i >= 0; --i) {
    total = total.combine_with(this->lower(i));
  }

  return total;
}

void ContinuousQueue::clear() {
  this->index_ = 0;  // reset the index
  this->size_ = 0;   // reset the count of aggregate chunks stored
}

void ContinuousQueue::insert(Aggregate value) {
  Aggregate most_recent = this->get_end_();

  // If the most recently stored Aggregate has less than or equal to the number of Aggregates in value, we
  // consolidate
  while ((this->index_ > 0) && (most_recent.get_count() <= value.get_count())) {
    // Combine value with most_recent
    value = value.combine_with(most_recent);

    // Store the next most recent Aggregate in the queue in most_recent
    --this->index_;
    most_recent = this->get_end_();
  }

  // If the queue is full, consolidate every Aggregate in the queue into a single Aggregate, even if the counts do not
  // dictate to do so. If repeatedly done, then numerical stability is potentially an issue. Set the capacity larger to
  // avoid this overflow handling.
  if (this->index_ == this->max_index_) {
    Aggregate total = this->compute_current_aggregate();
    this->emplace(total, 0);
    this->index_ = 1;
  }

  // Store the new Aggregate (which may have been combined with previous ones)
  this->emplace(value, this->index_);

  ++this->index_;  // increase index for next insertion
  ++this->size_;   // increase the count of number of stored aggregate chunks
}

bool ContinuousQueue::set_capacity(size_t capacity, TrackedStatisticsConfiguration tracked_statistics_config) {
  uint8_t queue_capacity = QUEUE_CAPACITY_IF_NONE_SPECIFIED;
  if (capacity > 0)
    queue_capacity = std::ceil(std::log2(capacity)) + 1;

  if (!this->allocate_memory_(queue_capacity, tracked_statistics_config))
    return false;

  this->max_index_ = queue_capacity;

  this->clear();

  return true;
}

//////////////////////////////////////////
// Internal Methods for Continous Queue //
//////////////////////////////////////////

inline Aggregate ContinuousQueue::get_end_() {
  return (this->index_ == 0) ? Aggregate(this->statistics_calculation_config_) : this->lower(this->index_ - 1);
}

}  // namespace statistics
}  // namespace esphome
