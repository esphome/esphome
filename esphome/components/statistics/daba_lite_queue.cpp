#include "daba_lite_queue.h"

#include "aggregate.h"

namespace esphome {
namespace statistics {

//////////////////////////////////////////////////////////
// Overridden virtual methods from AggregateQueue class //
//////////////////////////////////////////////////////////

Aggregate DABALiteQueue::compute_current_aggregate() {
  if (this->size() > 0) {
    Aggregate alpha = this->get_alpha_();
    Aggregate back = this->get_back_();

    return alpha.combine_with(back);
  }
  return Aggregate(this->statistics_calculation_config_);
}

void DABALiteQueue::clear() {
  this->size_ = 0;  // set count of inserted aggregates to 0

  // Reset indices in the circular queue
  this->f_ = CircularQueueIndex(0, this->window_size_);
  this->l_ = CircularQueueIndex(0, this->window_size_);
  this->r_ = CircularQueueIndex(0, this->window_size_);
  this->a_ = CircularQueueIndex(0, this->window_size_);
  this->b_ = CircularQueueIndex(0, this->window_size_);
  this->e_ = CircularQueueIndex(0, this->window_size_);

  *this->mid_sum_ = *this->back_sum_ = Aggregate(this->statistics_calculation_config_);
}

void DABALiteQueue::evict() {
  ++this->f_;
  --this->size_;

  this->step_();
}

void DABALiteQueue::insert(Aggregate value) {
  *this->back_sum_ = this->back_sum_->combine_with(value);
  this->emplace(value, this->e_.get_index());

  ++this->e_;
  ++this->size_;
  this->step_();
}

bool DABALiteQueue::set_capacity(size_t capacity, TrackedStatisticsConfiguration tracked_statistics_config) {
  this->window_size_ = capacity;

  // Verify additional aggregates were successfully allocated during class construction
  if ((this->back_sum_ == nullptr) || (this->mid_sum_ == nullptr))
    return false;

  if (!this->allocate_memory_(this->window_size_, tracked_statistics_config))
    return false;

  this->clear();

  return true;
}

//////////////////////////////////////////////
// Internal Methods for DABA Lite algorithm //
//////////////////////////////////////////////

void DABALiteQueue::step_() {
  if (this->l_ == this->b_) {
    this->flip_();
  }

  if (this->size_ > 0) {
    if (this->a_ != this->r_) {
      Aggregate prev_delta = this->get_delta_();

      --this->a_;
      Aggregate old_a = this->lower(this->a_.get_index());

      this->emplace(old_a.combine_with(prev_delta), this->a_.get_index());
    }

    if (this->l_ != this->r_) {
      Aggregate old_l = this->lower(this->l_.get_index());

      this->emplace(old_l.combine_with(*this->mid_sum_), this->l_.get_index());
      ++this->l_;
    } else {
      ++this->l_;
      ++this->r_;
      ++this->a_;
      *this->mid_sum_ = get_delta_();
    }
  } else {
    *this->back_sum_ = *this->mid_sum_ = Aggregate(this->statistics_calculation_config_);
  }
}

void DABALiteQueue::flip_() {
  this->l_ = this->f_;
  this->r_ = this->b_;
  this->a_ = this->e_;
  this->b_ = this->e_;

  *this->mid_sum_ = *this->back_sum_;
  *this->back_sum_ = Aggregate(this->statistics_calculation_config_);
}

// Checks if the b_ index is equal to the front index f_;
//  - If window size == size of the queue, then the front and end indices point to the same index,
//    so we verify that this is not the case
inline bool DABALiteQueue::is_front_empty_() { return (this->b_ == this->f_) && (this->size_ != this->window_size_); }

inline bool DABALiteQueue::is_delta_empty_() { return this->a_ == this->b_; }
inline Aggregate DABALiteQueue::get_back_() { return *this->back_sum_; }
inline Aggregate DABALiteQueue::get_alpha_() {
  return this->is_front_empty_() ? Aggregate(this->statistics_calculation_config_) : this->lower(this->f_.get_index());
}
inline Aggregate DABALiteQueue::get_delta_() {
  return this->is_delta_empty_() ? Aggregate(this->statistics_calculation_config_) : this->lower(this->a_.get_index());
}

//////////////////////////////////
// Circular Queue Index Methods //
//////////////////////////////////

CircularQueueIndex::CircularQueueIndex() {
  this->index_ = 0;
  this->capacity_ = 0;
}

CircularQueueIndex::CircularQueueIndex(size_t index, size_t capacity) {
  this->index_ = index;
  this->capacity_ = capacity;
}

CircularQueueIndex &CircularQueueIndex::operator++() {
  // Rollover
  if (this->index_ == (this->capacity_ - 1)) {
    this->index_ = 0;
    return *this;
  }

  ++this->index_;
  return *this;
}

CircularQueueIndex &CircularQueueIndex::operator--() {
  // Rollover
  if (this->index_ == 0) {
    this->index_ = this->capacity_ - 1;
    return *this;
  }

  --this->index_;
  return *this;
}

CircularQueueIndex &CircularQueueIndex::operator=(const CircularQueueIndex &rhs) {
  if (this == &rhs)
    return *this;
  this->index_ = rhs.get_index();
  this->capacity_ = rhs.get_capacity();

  return *this;
}

bool CircularQueueIndex::operator==(const CircularQueueIndex &rhs) const {
  return (this->index_ == rhs.get_index()) && this->capacity_ == rhs.get_capacity();
}

bool CircularQueueIndex::operator!=(const CircularQueueIndex &rhs) const {
  return (this->index_ != rhs.get_index()) || this->capacity_ != rhs.get_capacity();
}

}  // namespace statistics
}  // namespace esphome
