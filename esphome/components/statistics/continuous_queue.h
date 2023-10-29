/*
 * This class stores an array of aggregates, which combined give summary statistics for all measurements inserted.
 * It has two main functions
 *  - Combine with a new Aggregate (insert function)
 *  - Reset to a null Aggregate (clear function)
 *
 * This is used to aggregate continuously collected measurements into summary statistics. Aggregates in the array
 * are combined when they aggregate the same number of measurements. As Aggregates combine only with the same size
 * samples, this approach is numerically stable for any quantity of measurements. It has a slight penalty in terms of
 * computational complexity and memory usage. Memory is pre-allocated. If no specific capacity is specified, it
 * allocates enough memory for 2^(QUEUE_CAPACITY_IF_NONE_SPECIFIED)) Aggregates to be inserted. If that capacity is
 * exceeded, then an overflow is handled by aggregating everything in the queue into one Aggregate. Repeated overflow
 * handling can cause numerical instability in the long term.
 *
 *
 * Example run: At each numbered step, insert a new Aggregate with count=1. queue_ is the set of aggregates in the
 * queue, and each Aggregate is denoted by its count.
 *
 *  1) queue_ = {1}
 *
 *  2) queue_ = {1,1}
 *     queue_ = {2}     // right-most elements have the same count, so they are combined
 *
 *  3) queue_ = {2,1}
 *
 *  4) queue_ = {2,1,1}
 *     queue_ = {2,2}   // right-most elements have the same count, so they are combined
 *     queue_ = {4}     // right-most elements have the same count, so they are combined
 *
 *  5) queue_ = {4,1}
 *
 *  6) queue_ = {4,1,1}
 *     queue_ = {4,2}   // right-most elements have the same count, so they are combined
 *
 *  7) queue_ = {4,2,1}
 *
 *  8) queue_ = {4,2,1,1}
 *     queue_ = {4,2,2} // right-most elements have the same count, so they are combined
 *     queue_ = {4,4}   // right-most elements have the same count, so they are combined
 *     queue_ = {8}     // right-most elements have the same count, so they are combined
 *
 *
 * Time complexity for n aggregate chunks:
 *  - insertion of new measurement: worst case O(log(n))
 *  - clear queue: O(1)
 *  - computing current aggregate: worst case O(log(n))
 *
 * Memory usage for n aggregate chunks:
 *  - log(n)+1 aggregates
 *
 *
 * Implemented by Kevin Ahrendt for the ESPHome project, 2023
 */

#pragma once

#include "aggregate.h"
#include "aggregate_queue.h"

namespace esphome {
namespace statistics {

// If no capacity is specified, we allocate memory for this number of insertable Aggregates in our queue.
// The queue can insert 2^(QUEUE_CAPACITY_IF_NONE_SPECIFIED) aggregates before the overflow handling occurs.
static const uint8_t QUEUE_CAPACITY_IF_NONE_SPECIFIED = 32;

class ContinuousQueue : public AggregateQueue {
 public:
  ContinuousQueue(StatisticsCalculationConfig statistics_calculation_config)
      : AggregateQueue(statistics_calculation_config) {}

  //////////////////////////////////////////////////////////
  // Overridden virtual methods from AggregateQueue class //
  //////////////////////////////////////////////////////////

  /** Compute the Aggregate summarizing all entries in the queue.
   *
   * Compute the summary statistics of all aggregates in the queue by combining them.
   * @return the aggregate summary of all elements in the queue
   */
  Aggregate compute_current_aggregate() override;

  /// @brief Clear all Aggregates in the queue.
  void clear() override;

  /// @brief Equivalent to clearing all Aggregates in the queue
  void evict() override { this->clear(); }

  /** Insert Aggregate at the end of the queue.
   *
   * A new Aggregate is added to the queue, and previous queue elements consolidate if necessary
   * @param value the Aggregate to be inserted
   */
  void insert(Aggregate value) override;

  /** Configure the queue's size and pre-allocate memory.
   *
   * This queue uses at most log_2(<window_size>)+1 aggregates to store <chunk_capacity> aggregate chunks.
   * @param capacity the total amount of Aggregate chunks that can be inserted into the queue before an overflow
   * @param tracked_statistics_config which summary statistics are saved in the queue
   * @return true if memory was successfully allocated, false otherwise
   */
  bool configure_capacity(size_t capacity, TrackedStatisticsConfiguration tracked_statistics_config) override;

 protected:
  // Largest possible index before running out of memory
  size_t max_index_;

  // Stores the index one past the most recently inserted Aggregate chunk
  uint8_t index_{0};

  //////////////////////////////////////////
  // Internal Methods for Continous Queue //
  //////////////////////////////////////////

  /// @brief Returns the most recent Aggregate chunk stored in the queue
  inline Aggregate get_end_();
};

}  // namespace statistics
}  // namespace esphome
