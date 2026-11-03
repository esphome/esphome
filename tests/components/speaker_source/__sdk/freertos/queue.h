#pragma once

#include "FreeRTOS.h"
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

// A single-threaded FIFO with the FreeRTOS queue calls the player uses. It never blocks, and it logs every
// item sent so a test can see what the player queued for itself.
inline constexpr BaseType_t pdTRUE = 1;
inline constexpr BaseType_t pdFALSE = 0;
inline constexpr TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
inline void vTaskDelay(TickType_t /*ticks*/) {}

struct QueueDefinition {
  size_t length;
  size_t item_size;
  std::deque<std::vector<uint8_t>> items;
};
using QueueHandle_t = QueueDefinition *;

namespace queue_test {
inline std::vector<std::vector<uint8_t>> sent;
}  // namespace queue_test

inline QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size) {
  return new QueueDefinition{length, item_size, {}};
}

inline BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t /*ticks*/) {
  if (queue->items.size() >= queue->length)
    return pdFALSE;
  const auto *bytes = static_cast<const uint8_t *>(item);
  queue->items.emplace_back(bytes, bytes + queue->item_size);
  queue_test::sent.push_back(queue->items.back());
  return pdTRUE;
}

inline BaseType_t xQueuePeek(QueueHandle_t queue, void *item, TickType_t /*ticks*/) {
  if (queue->items.empty())
    return pdFALSE;
  std::memcpy(item, queue->items.front().data(), queue->item_size);
  return pdTRUE;
}

inline BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t ticks) {
  if (xQueuePeek(queue, item, ticks) != pdTRUE)
    return pdFALSE;
  queue->items.pop_front();
  return pdTRUE;
}
