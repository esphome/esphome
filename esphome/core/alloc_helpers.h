#pragma once

/// @file alloc_helpers.h
/// @brief Heap-allocating helper functions.
///
/// These functions return std::string and allocate heap memory on every call.
/// On long-running embedded devices, repeated heap allocations fragment memory
/// over time, eventually causing crashes even with free memory available.
///
/// Prefer the stack-based alternatives documented on each function instead.
/// New code should avoid using these functions.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {

// --- String helpers (allocating) ---

/// Truncate a string to a specific length.
/// @warning Allocates heap memory. Avoid in new code - causes heap fragmentation on long-running devices.
std::string str_truncate(const std::string &str, size_t length);

/// Extract the part of the string until either the first occurrence of the specified character, or the end
/// (requires str to be null-terminated).
/// @warning Allocates heap memory. Avoid in new code - causes heap fragmentation on long-running devices.
std::string str_until(const char *str, char ch);
/// Extract the part of the string until either the first occurrence of the specified character, or the end.
/// @warning Allocates heap memory. Avoid in new code - causes heap fragmentation on long-running devices.
std::string str_until(const std::string &str, char ch);

/// Convert the string to lower case.
/// @warning Allocates heap memory. Avoid in new code - causes heap fragmentation on long-running devices.
std::string str_lower_case(const std::string &str);

/// Convert the string to upper case.
/// @warning Allocates heap memory. Avoid in new code - causes heap fragmentation on long-running devices.
std::string str_upper_case(const std::string &str);

/// Convert the string to snake case (lowercase with underscores).
/// @warning Allocates heap memory. Avoid in new code - causes heap fragmentation on long-running devices.
std::string str_snake_case(const std::string &str);

/// Sanitizes the input string by removing all characters but alphanumerics, dashes and underscores.
/// @warning Allocates heap memory. Use str_sanitize_to() with a stack buffer instead.
std::string str_sanitize(const std::string &str);

/// snprintf-like function returning std::string of maximum length \p len (excluding null terminator).
/// @warning Allocates heap memory. Use snprintf() with a stack buffer instead.
std::string __attribute__((format(printf, 1, 3))) str_snprintf(const char *fmt, size_t len, ...);

/// sprintf-like function returning std::string.
/// @warning Allocates heap memory. Use snprintf() with a stack buffer instead.
std::string __attribute__((format(printf, 1, 2))) str_sprintf(const char *fmt, ...);

// --- Hex/binary formatting helpers (allocating) ---

/// Format the six-byte array \p mac into a MAC address string.
/// @warning Allocates heap memory. Use format_mac_addr_upper() with a stack buffer instead.
std::string format_mac_address_pretty(const uint8_t mac[6]);

/// Format the byte array \p data of length \p len in lowercased hex.
/// @warning Allocates heap memory. Use format_hex_to() with a stack buffer instead.
std::string format_hex(const uint8_t *data, size_t length);

/// Format the vector \p data in lowercased hex.
/// @warning Allocates heap memory. Use format_hex_to() with a stack buffer instead.
std::string format_hex(const std::vector<uint8_t> &data);

/// Format a byte array in pretty-printed, human-readable hex format.
/// @warning Allocates heap memory. Use format_hex_pretty_to() with a stack buffer instead.
std::string format_hex_pretty(const uint8_t *data, size_t length, char separator = '.', bool show_length = true);

/// Format a 16-bit word array in pretty-printed, human-readable hex format.
/// @warning Allocates heap memory. Use format_hex_pretty_to() with a stack buffer instead.
std::string format_hex_pretty(const uint16_t *data, size_t length, char separator = '.', bool show_length = true);

/// Format a byte vector in pretty-printed, human-readable hex format.
/// @warning Allocates heap memory. Use format_hex_pretty_to() with a stack buffer instead.
std::string format_hex_pretty(const std::vector<uint8_t> &data, char separator = '.', bool show_length = true);

/// Format a 16-bit word vector in pretty-printed, human-readable hex format.
/// @warning Allocates heap memory. Use format_hex_pretty_to() with a stack buffer instead.
std::string format_hex_pretty(const std::vector<uint16_t> &data, char separator = '.', bool show_length = true);

/// Format a string's bytes in pretty-printed, human-readable hex format.
/// @warning Allocates heap memory. Use format_hex_pretty_to() with a stack buffer instead.
std::string format_hex_pretty(const std::string &data, char separator = '.', bool show_length = true);

/// Format the byte array \p data of length \p len in binary.
/// @warning Allocates heap memory. Use format_bin_to() with a stack buffer instead.
std::string format_bin(const uint8_t *data, size_t length);

// --- Value formatting helpers (allocating) ---

/// Format a float value with accuracy decimals to a string.
/// @deprecated Allocates heap memory. Use value_accuracy_to_buf() instead. Removed in 2026.7.0.
__attribute__((deprecated("Allocates heap memory. Use value_accuracy_to_buf() instead. Removed in 2026.7.0.")))
std::string
value_accuracy_to_string(float value, int8_t accuracy_decimals);

// --- Base64 helpers (allocating) ---

/// Encode a byte buffer to base64 string.
/// @warning Allocates heap memory.
std::string base64_encode(const uint8_t *buf, size_t buf_len);
/// Encode a byte vector to base64 string.
/// @warning Allocates heap memory.
std::string base64_encode(const std::vector<uint8_t> &buf);

/// Decode a base64 string to a byte vector.
/// @warning Allocates heap memory. Use base64_decode(data, len, buf, buf_len) with a pre-allocated buffer instead.
std::vector<uint8_t> base64_decode(const std::string &encoded_string);

// --- MAC address helpers (allocating) ---

/// Get the device MAC address as a string, in lowercase hex notation.
/// @warning Allocates heap memory. Use get_mac_address_into_buffer() instead.
std::string get_mac_address();

/// Get the device MAC address as a string, in colon-separated uppercase hex notation.
/// @warning Allocates heap memory. Use get_mac_address_pretty_into_buffer() instead.
std::string get_mac_address_pretty();

}  // namespace esphome
