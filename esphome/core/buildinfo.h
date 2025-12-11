#pragma once
#include <cstdint>
#include <ctime>

// Build information functions that provide config hash and build time.
// The actual values are provided by linker-defined symbols to avoid
// unnecessary rebuilds when only the build time changes.
// This is kept in its own file so that only files that need build-specific
// information have to include it explicitly.

namespace esphome {
namespace buildinfo {

const char *get_config_hash();
time_t get_build_time();
const char *get_build_time_string();

}  // namespace buildinfo
}  // namespace esphome
