#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::unicode {

/**
 *  Attempt to extract a 32 bit Unicode codepoint from a UTF-8 string.
 *  If successful, return the codepoint and set the length to the number of bytes read.
 *  If the end of the string has been reached and a valid codepoint has not been found, return 0 and set the length to
 * 0.
 *
 * @param utf8_str The input string
 * @param length Pointer to length storage
 * @return The extracted code point
 */
uint32_t extract_unicode_codepoint(const char *utf8_str, size_t *length);

}  // namespace esphome::unicode
