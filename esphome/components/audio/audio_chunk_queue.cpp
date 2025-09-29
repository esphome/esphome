#include "audio_chunk_queue.h"

#if defined(USE_ESP_IDF) && defined(USE_AUDIO)

namespace esphome {
namespace audio {

// Event group bits for signaling between producer and consumer
enum EventGroupBits : uint32_t {
  DATA_AVAILABLE = (1 << 0),   // Signal that data is available for consumer
  SPACE_AVAILABLE = (1 << 1),  // Signal that size-based space is available for producer
  SLOT_AVAILABLE = (1 << 2),   // Signal that a queue slot is available for producer
};

std::unique_ptr<AudioChunkQueue> AudioChunkQueue::create(size_t capacity, size_t max_data_size,
                                                         bool enable_dynamic_growth, size_t max_capacity) {
  auto queue = make_unique<AudioChunkQueue>();

  // Initialize the ring buffer with one extra slot
  // (to distinguish between full and empty states)
  queue->initial_capacity_ = capacity + 1;
  queue->capacity_.store(queue->initial_capacity_, std::memory_order_release);
  queue->buffer_.resize(queue->initial_capacity_);

  // Create event group for blocking operations
  queue->event_group_ = xEventGroupCreate();
  if (queue->event_group_ == nullptr) {
    return nullptr;
  }

  queue->max_data_size_ = max_data_size;
  queue->enable_dynamic_growth_ = enable_dynamic_growth;

  // Set max capacity if dynamic growth is enabled
  if (enable_dynamic_growth) {
    // Ensure max_capacity is reasonable
    if (max_capacity > 0 && max_capacity > queue->initial_capacity_) {
      queue->max_capacity_ = max_capacity;
    } else {
      // Default to 4x initial capacity if not specified
      queue->max_capacity_ = queue->initial_capacity_ * 4;
    }

    // Create growth mutex
    queue->growth_mutex_ = xSemaphoreCreateMutex();
    if (queue->growth_mutex_ == nullptr) {
      vEventGroupDelete(queue->event_group_);
      return nullptr;
    }
  }

  return queue;
}

AudioChunkQueue::~AudioChunkQueue() {
  reset();  // Drain all remaining chunks
  if (this->event_group_ != nullptr) {
    vEventGroupDelete(this->event_group_);
  }
  if (this->growth_mutex_ != nullptr) {
    vSemaphoreDelete(this->growth_mutex_);
  }
}

bool AudioChunkQueue::add_chunk(const std::shared_ptr<AudioChunk> &chunk, TickType_t ticks_to_wait) {
  if (!chunk) {
    return false;
  }

  // Handle queue-full backpressure with retry loop
  TickType_t remaining_ticks = ticks_to_wait;
  TickType_t start_ticks = xTaskGetTickCount();

  while (this->is_full_()) {
    // Try to grow the queue if enabled and conditions are met
    if (this->can_grow_()) {
      if (this->try_grow_capacity_()) {
        // Growth successful, re-check if still full
        continue;
      }
    }

    if (remaining_ticks == 0) {
      return false;  // No wait requested or timeout
    }

    // Wait for a slot to become available
    EventBits_t bits = xEventGroupWaitBits(this->event_group_, SLOT_AVAILABLE, pdTRUE, pdFALSE, remaining_ticks);

    if (!(bits & SLOT_AVAILABLE)) {
      return false;  // Timed out waiting for slot
    }

    // Update remaining time for next iteration if not waiting forever
    if (ticks_to_wait != portMAX_DELAY) {
      TickType_t elapsed_ticks = xTaskGetTickCount() - start_ticks;
      if (elapsed_ticks >= ticks_to_wait) {
        remaining_ticks = 0;
      } else {
        remaining_ticks = ticks_to_wait - elapsed_ticks;
      }
    }
    // Loop will re-check this->is_full_() condition
  }

  // Size-based backpressure logic with retry loop
  if (this->max_data_size_ > 0) {
    // Set pending size to help consumer know how much space we need
    this->pending_chunk_size_.store(chunk->size, std::memory_order_release);

    // Use same remaining_ticks from queue-full check if we already waited
    while (true) {
      size_t current_size = this->stored_data_size_.load(std::memory_order_acquire);
      if ((current_size + chunk->size) <= this->max_data_size_) {
        // Space is available, clear pending and continue
        this->pending_chunk_size_.store(0, std::memory_order_release);
        break;
      }

      if (remaining_ticks == 0) {
        // No wait requested or timeout
        this->pending_chunk_size_.store(0, std::memory_order_release);
        return false;
      }

      // Wait for space to become available
      EventBits_t bits = xEventGroupWaitBits(this->event_group_, SPACE_AVAILABLE, pdTRUE, pdFALSE, remaining_ticks);

      if (!(bits & SPACE_AVAILABLE)) {
        // Timed out waiting for space
        this->pending_chunk_size_.store(0, std::memory_order_release);
        return false;
      }

      // Update remaining time for next iteration if not waiting forever
      if (ticks_to_wait != portMAX_DELAY) {
        TickType_t elapsed_ticks = xTaskGetTickCount() - start_ticks;
        if (elapsed_ticks >= ticks_to_wait) {
          remaining_ticks = 0;
        } else {
          remaining_ticks = ticks_to_wait - elapsed_ticks;
        }
      }
      // Loop will re-check size condition
    }
  }

  // Add chunk to ring buffer (lock-free operation)
  size_t current_tail = this->tail_.load(std::memory_order_relaxed);

  // Store shared_ptr in buffer (producer owns this slot)
  this->buffer_[current_tail] = chunk;

  // Make chunk visible to consumer by advancing tail
  this->tail_.store(this->next_index_(current_tail), std::memory_order_release);

  // Update stored size and signal data available
  this->stored_data_size_.fetch_add(chunk->size, std::memory_order_acq_rel);
  xEventGroupSetBits(this->event_group_, DATA_AVAILABLE);

  return true;
}

std::shared_ptr<AudioChunk> AudioChunkQueue::receive_chunk(TickType_t ticks_to_wait) {
  // Check if queue is empty (lock-free check)
  if (this->is_empty_()) {
    if (ticks_to_wait == 0) {
      return nullptr;  // No wait requested
    }

    // Wait for data to become available
    EventBits_t bits = xEventGroupWaitBits(this->event_group_, DATA_AVAILABLE, pdTRUE, pdFALSE, ticks_to_wait);

    if (!(bits & DATA_AVAILABLE)) {
      return nullptr;  // Timeout
    }

    // Re-check after waking up (spurious wakeup protection)
    if (this->is_empty_()) {
      return nullptr;
    }
  }

  // Read from ring buffer (lock-free operation)
  size_t current_head = this->head_.load(std::memory_order_relaxed);

  // Get shared_ptr from buffer (consumer owns this slot)
  std::shared_ptr<AudioChunk> chunk = std::move(this->buffer_[current_head]);

  // Clear the slot (optional but helps with debugging)
  this->buffer_[current_head].reset();

  // Advance head to free slot for producer
  this->head_.store(this->next_index_(current_head), std::memory_order_release);

  // Signal that a slot is now available since we just freed one
  xEventGroupSetBits(this->event_group_, SLOT_AVAILABLE);

  if (chunk) {
    // Update stored size
    this->stored_data_size_.fetch_sub(chunk->size, std::memory_order_acq_rel);

    // Check for size-based backpressure
    size_t pending_size = this->pending_chunk_size_.load(std::memory_order_acquire);
    if (pending_size > 0) {
      // Check if the pending chunk can now fit
      size_t current_size = this->stored_data_size_.load(std::memory_order_acquire);
      if ((current_size + pending_size) <= this->max_data_size_) {
        // Signal size-based space is available
        xEventGroupSetBits(this->event_group_, SPACE_AVAILABLE);
      }
    }

    // If queue is not empty, ensure DATA_AVAILABLE is set for next receive
    if (!this->is_empty_()) {
      xEventGroupSetBits(this->event_group_, DATA_AVAILABLE);
    }
  }

  return chunk;
}

void AudioChunkQueue::reset() {
  // Drain all chunks using the thread-safe receive method
  while (std::shared_ptr<AudioChunk> chunk = receive_chunk(0)) {
    // shared_ptr automatically handles cleanup
  }

  // Reset counters
  this->stored_data_size_.store(0, std::memory_order_release);
  this->pending_chunk_size_.store(0, std::memory_order_release);

  // Wake up any threads that might be waiting
  // This ensures no threads get stuck after a reset
  xEventGroupSetBits(this->event_group_, SLOT_AVAILABLE | SPACE_AVAILABLE | DATA_AVAILABLE);
}

size_t AudioChunkQueue::messages() const {
  size_t h = this->head_.load(std::memory_order_acquire);
  size_t t = this->tail_.load(std::memory_order_acquire);
  size_t cap = this->capacity_.load(std::memory_order_acquire);

  if (t >= h) {
    return t - h;
  } else {
    return cap - h + t;
  }
}

bool AudioChunkQueue::can_grow_() const {
  if (!this->enable_dynamic_growth_) {
    return false;
  }

  // Check if we're already at max capacity
  size_t current_capacity = this->capacity_.load(std::memory_order_acquire);
  if (current_capacity >= this->max_capacity_) {
    return false;
  }

  // Check if we have data space available
  if (this->max_data_size_ > 0) {
    size_t current_data_size = this->stored_data_size_.load(std::memory_order_acquire);
    size_t pending_size = this->pending_chunk_size_.load(std::memory_order_acquire);

    // Only grow if we have reasonable data space available
    // Use a more conservative threshold to prevent excessive growth
    const size_t MIN_AVAILABLE_RATIO = 4;                          // Require at least 1/4 of max space available
    size_t space_needed = pending_size > 0 ? pending_size : 1024;  // Assume 1KB if no pending
    size_t max_allowed = this->max_data_size_ - (this->max_data_size_ / MIN_AVAILABLE_RATIO);
    if ((current_data_size + space_needed) > max_allowed) {
      return false;  // Too close to data limit
    }
  }

  // Check if another growth is already in progress
  if (this->is_growing_.load(std::memory_order_acquire)) {
    return false;
  }

  return true;
}

bool AudioChunkQueue::try_grow_capacity_() {
  // Atomic flag to prevent concurrent growth attempts
  bool expected = false;
  if (!this->is_growing_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return false;  // Another growth is already in progress
  }

  // Take the growth mutex to ensure exclusive access during resize
  if (xSemaphoreTake(this->growth_mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
    this->is_growing_.store(false, std::memory_order_release);
    return false;  // Couldn't acquire mutex quickly enough
  }

  // Double-check conditions under mutex
  size_t current_capacity = this->capacity_.load(std::memory_order_acquire);
  if (current_capacity >= this->max_capacity_ || !this->is_full_()) {
    xSemaphoreGive(this->growth_mutex_);
    this->is_growing_.store(false, std::memory_order_release);
    return false;
  }

  // Calculate new capacity (grow by 50% or to max, whichever is smaller)
  size_t new_capacity = current_capacity + (current_capacity / 2);
  if (new_capacity > this->max_capacity_) {
    new_capacity = this->max_capacity_;
  }

  // Get current indices
  size_t current_head = this->head_.load(std::memory_order_acquire);
  size_t current_tail = this->tail_.load(std::memory_order_acquire);

  // Create new buffer with the new capacity
  // Note: On embedded platforms without exceptions, resize could potentially fail
  // In practice, if allocation fails, the system is likely in a critical state anyway
  std::vector<std::shared_ptr<AudioChunk>> new_buffer;
  new_buffer.reserve(new_capacity);  // Pre-allocate capacity

  // Check if reservation succeeded by verifying capacity
  if (new_buffer.capacity() < new_capacity) {
    // Allocation failed, cleanup and return
    xSemaphoreGive(this->growth_mutex_);
    this->is_growing_.store(false, std::memory_order_release);
    return false;
  }

  // Now resize to the actual size (this should not allocate since we reserved)
  new_buffer.resize(new_capacity);

  // Copy existing chunks to the new buffer
  // We need to preserve the order and handle wraparound
  size_t read_idx = current_head;
  size_t write_idx = 0;

  while (read_idx != current_tail) {
    new_buffer[write_idx] = std::move(this->buffer_[read_idx]);
    read_idx = (read_idx + 1) % current_capacity;
    write_idx++;
  }

  // Update buffer and indices atomically
  this->buffer_ = std::move(new_buffer);
  this->head_.store(0, std::memory_order_release);
  this->tail_.store(write_idx, std::memory_order_release);
  this->capacity_.store(new_capacity, std::memory_order_release);

  // Signal that slots are now available
  xEventGroupSetBits(this->event_group_, SLOT_AVAILABLE);

  // Release mutex and flag
  xSemaphoreGive(this->growth_mutex_);
  this->is_growing_.store(false, std::memory_order_release);

  return true;
}

size_t AudioChunkQueue::next_index_(size_t idx) const {
  size_t cap = capacity_.load(std::memory_order_acquire);
  return (idx + 1) % cap;
}

bool AudioChunkQueue::is_empty_() const {
  // Consumer owns head (relaxed), observes tail (acquire to see producer's writes)
  return this->head_.load(std::memory_order_relaxed) == this->tail_.load(std::memory_order_acquire);
}

bool AudioChunkQueue::is_full_() const {
  // Producer owns tail (relaxed), observes head (acquire to see consumer's reads)
  size_t current_tail = this->tail_.load(std::memory_order_relaxed);
  size_t next_tail = this->next_index_(current_tail);
  return next_tail == this->head_.load(std::memory_order_acquire);
}

}  // namespace audio
}  // namespace esphome

#endif
