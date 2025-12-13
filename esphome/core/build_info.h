#pragma once
#include <cstdint>
#include <ctime>
#include <span>

namespace esphome {

/// Size of buffer required for build time string (including null terminator)
static constexpr size_t BUILD_TIME_STR_SIZE = 24;

/// Get the config hash as a 32-bit integer
uint32_t get_config_hash();

/// Get the build time as a Unix timestamp
time_t get_build_time();

/// Copy the build time string into the provided buffer
/// Buffer must be BUILD_TIME_STR_SIZE bytes (compile-time enforced)
void get_build_time_string(std::span<char, BUILD_TIME_STR_SIZE> buffer);

}  // namespace esphome
