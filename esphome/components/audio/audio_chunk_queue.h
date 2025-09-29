#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_AUDIO)

#include "audio_chunk.h"
#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace esphome {
namespace audio {

// SPSC (Single Producer, Single Consumer) queue for AudioChunk shared pointers with optional dynamic growth
// IMPORTANT: This queue is designed for single-producer, single-consumer use only.
// - Exactly one thread should call add_chunk() (producer)
// - Exactly one thread should call receive_chunk() (consumer)
// Multiple producers or consumers will cause race conditions and undefined behavior.
//
// Thread Safety Model:
// - Normal operations (add/receive without growth) are lock-free for performance
// - Dynamic growth operations use mutex synchronization to ensure safety
// - During growth, both producer and consumer are temporarily blocked
// - This hybrid approach provides fast normal operation with safe capacity expansion
//
// This implementation is a ring buffer that stores std::shared_ptr for automatic
// memory management, eliminating the need for manual reference counting.
class AudioChunkQueue {
 public:
  // Create a new queue with specified capacity and optional max data size
  // If enable_dynamic_growth is true and max_capacity > capacity, the queue can grow
  static std::unique_ptr<AudioChunkQueue> create(size_t capacity, size_t max_data_size = 0,
                                                 bool enable_dynamic_growth = false, size_t max_capacity = 0);

  ~AudioChunkQueue();

  // Add chunk to the queue (producer thread only)
  // Returns true if successful, false if queue is full or timeout
  bool add_chunk(const std::shared_ptr<AudioChunk> &chunk, TickType_t ticks_to_wait);

  // Receive chunk from the queue (consumer thread only)
  // Returns nullptr if queue is empty or timeout
  std::shared_ptr<AudioChunk> receive_chunk(TickType_t ticks_to_wait);

  // Safely drain all chunks from the queue
  void reset();

  // Return number of chunks in queue
  size_t messages() const;

  // Return number of chunks in queue (alias for compatibility)
  size_t size() const { return messages(); }

  // Return total size of data in queue (bytes)
  size_t total_bytes() const { return this->stored_data_size_.load(std::memory_order_acquire); }

 private:
  // Ring buffer of shared pointers
  std::vector<std::shared_ptr<AudioChunk>> buffer_;
  std::atomic<size_t> capacity_;  // Actual capacity is buffer size (atomic for dynamic growth)

  // Lock-free indices for SPSC pattern
  std::atomic<size_t> head_{0};  // Consumer reads from head
  std::atomic<size_t> tail_{0};  // Producer writes to tail

  // FreeRTOS event group for blocking operations
  EventGroupHandle_t event_group_{nullptr};

  // Size-based flow control
  size_t max_data_size_{0};                    // Maximum allowed data size (0 = unlimited)
  std::atomic<size_t> pending_chunk_size_{0};  // Size of chunk waiting to be added
  std::atomic<size_t> stored_data_size_{0};    // Current total size of data in queue

  // Dynamic growth parameters
  bool enable_dynamic_growth_{false};        // Whether dynamic growth is enabled
  size_t max_capacity_{0};                   // Maximum capacity if dynamic growth is enabled
  size_t initial_capacity_{0};               // Initial capacity for potential reset
  std::atomic<bool> is_growing_{false};      // Flag to indicate growth in progress
  SemaphoreHandle_t growth_mutex_{nullptr};  // Mutex for coordinating growth between producer/consumer

  // Helper to calculate next index with wraparound
  size_t next_index_(size_t idx) const;

  // Check if queue is empty (consumer perspective - owns head, observes tail)
  bool is_empty_() const;

  // Check if queue is full (producer perspective - owns tail, observes head)
  bool is_full_() const;

  // Try to grow the queue capacity
  bool try_grow_capacity_();

  // Check if growth is possible based on current conditions
  bool can_grow_() const;
};

}  // namespace audio
}  // namespace esphome
#endif
