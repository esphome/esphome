#pragma once

#ifdef USE_NEXTION_TFT_UPLOAD

#include <string>
#include <cinttypes>

namespace esphome {
namespace nextion {

// Internal upload helper functions - not part of public API

/**
 * Builds HTTP range header string
 * @param buffer Output buffer for the header string
 * @param buffer_size Size of output buffer
 * @param range_start Start byte position
 * @param range_end End byte position (inclusive)
 */
void build_range_header(char *buffer, size_t buffer_size, uint32_t range_start, uint32_t range_end);

}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD
