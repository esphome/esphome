#pragma once

#ifdef USE_ESP32

// Deprecated: include "esphome/components/ring_buffer/ring_buffer.h" and use
// esphome::ring_buffer::RingBuffer. This shim will be removed in 2026.11.0.
#if __has_include("esphome/components/ring_buffer/ring_buffer.h")
#include "esphome/components/ring_buffer/ring_buffer.h"
#else
#error \
    "esphome/components/ring_buffer/ring_buffer.h not found. Add 'ring_buffer' to your component's AUTO_LOAD list to use esphome::ring_buffer::RingBuffer."
#endif
#include "esphome/core/helpers.h"  // for ESPDEPRECATED

namespace esphome {

using RingBuffer ESPDEPRECATED("Use esphome::ring_buffer::RingBuffer instead. Removed in 2026.11.0.",
                               "2026.5.0") = ring_buffer::RingBuffer;

}  // namespace esphome

#endif
