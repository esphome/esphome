#pragma once

#include <cstddef>
#include <cstdint>

static constexpr uint32_t MALLOC_CAP_SPIRAM = 1;
static constexpr uint32_t MALLOC_CAP_INTERNAL = 2;
inline size_t heap_caps_get_free_size(uint32_t) { return 1 << 20; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 1 << 20; }
