#pragma once
#include <cstdint>
#include <ctime>

// Build information functions that provide config hash and build time.
// The actual values are provided by linker-defined symbols to avoid
// unnecessary rebuilds when only the build time changes.
// This is kept in its own file so that only files that need build-specific
// information have to include it explicitly.

namespace esphome::buildinfo {

extern const char CONFIG_HASH_STR[];
extern const char BUILD_TIME_STR[];
extern const uintptr_t BUILD_TIME;

static inline const char *get_config_hash() { return CONFIG_HASH_STR; }

static inline time_t get_build_time() { return (time_t) BUILD_TIME; }

static inline const char *get_build_time_string() { return BUILD_TIME_STR; }

}  // namespace esphome::buildinfo
