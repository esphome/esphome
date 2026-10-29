#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "api_pb2.h"
#include "esphome/core/entity_base.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace esphome::api {

// Generic batching mechanism for both state updates and entity info
struct DeferredBatch {
  // Sentinel value for unused aux_data_index
  static constexpr uint8_t AUX_DATA_UNUSED = std::numeric_limits<uint8_t>::max();

  struct BatchItem {
    EntityBase *entity;                       // 4 bytes - Entity pointer
    uint16_t message_type;                    // 2 bytes - Message type for protocol and dispatch
    uint8_t estimated_size;                   // 1 byte - Estimated message size (max 255 bytes)
    uint8_t aux_data_index{AUX_DATA_UNUSED};  // 1 byte - For events: index into entity's event_types
  };

  std::vector<BatchItem> items;
  uint32_t batch_start_time{0};

  // No pre-allocation - log connections never use batching, and for
  // connections that do, buffers are released after initial sync anyway

  // O(n) scan but optimized for RAM over performance
  bool contains(EntityBase *entity, uint16_t message_type) const {
    for (const auto &item : this->items) {
      if (item.entity == entity && item.message_type == message_type)
        return true;
    }
    return false;
  }

  // Add item to the batch (with deduplication)
  void add_item(EntityBase *entity, uint16_t message_type, uint8_t estimated_size,
                uint8_t aux_data_index = AUX_DATA_UNUSED) {
    // Skip deduplication for events - they are edge-triggered, every occurrence matters
#ifdef USE_EVENT
    if (message_type != EventResponse::MESSAGE_TYPE)
#endif
    {
      if (this->contains(entity, message_type))
        return;  // Already queued
    }
    this->items.push_back({entity, message_type, estimated_size, aux_data_index});
  }
  // Add item to the front of the batch (for high priority messages like ping)
  void add_item_front(EntityBase *entity, uint16_t message_type, uint8_t estimated_size) {
    // Shift rather than swap, so the queued items keep their order (two events of one entity must not swap)
    this->items.push_back({});
    std::move_backward(this->items.begin(), this->items.end() - 1, this->items.end());
    this->items.front() = {entity, message_type, estimated_size, AUX_DATA_UNUSED};
  }

  // Clear all items
  void clear() {
    items.clear();
    batch_start_time = 0;
  }

  // Remove processed items from the front — noinline to keep memmove out of warm callers
  void remove_front(size_t count) __attribute__((noinline)) { items.erase(items.begin(), items.begin() + count); }

  bool empty() const { return items.empty(); }
  size_t size() const { return items.size(); }
  const BatchItem &operator[](size_t index) const { return items[index]; }

  // Release excess capacity - only releases if items already empty
  void release_buffer() {
    // Safe to call: batch is processed before release_buffer is called,
    // and if any items remain (partial processing), we must not clear them.
    // Use swap trick since shrink_to_fit() is non-binding and may be ignored.
    if (items.empty()) {
      std::vector<BatchItem>().swap(items);
    }
  }
};

}  // namespace esphome::api
#endif  // USE_API
