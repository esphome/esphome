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

/**
 * Parses Nextion's partial upload response (0x08 message)
 * The response contains a 32-bit position indicating where to resume upload
 * @param response The response string from Nextion
 * @param range_end The end of the current range
 * @return The new position to resume from, or range_end + 1 if result is 0
 */
uint32_t parse_nextion_upload_response(const std::string &response, uint32_t range_end);

}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD
