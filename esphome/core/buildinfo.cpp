// Build information using linker-provided symbols
//
// Including build information into the build is fun, because we *don't*
// want the mere fact of changing the build time to *itself* cause a
// rebuild if nothing else had changed. If we do the naïve thing of
// just putting #defines in a header like version.h, we'll cause exactly
// that.
//
// So instead we provide the config hash and build time in a linker
// script, so they get pulled in only if the firmware is already being
// rebuilt.
#include "esphome/core/buildinfo.h"
#include <cstdio>

// Linker-provided symbols - declare as extern variables, not functions
extern "C" {
extern const char ESPHOME_CONFIG_HASH[];
extern const char ESPHOME_BUILD_TIME[];
}

namespace esphome {
namespace buildinfo {

#if __SIZEOF_POINTER__ > 4
// On 64-bit platforms, reference the linker symbols as uintptr_t from the *data* section to
// avoid issues with pc-relative relocations. Don't let the compiler know they're const or
// it'll optimise away the whole thing and emit a relocation to the ESPHOME_XXX symbols
// directly, which defeats the whole point!
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static uintptr_t config_hash_ptr = (uintptr_t) &ESPHOME_CONFIG_HASH;
static uintptr_t build_time_ptr = (uintptr_t) &ESPHOME_BUILD_TIME;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
#define config_hash config_hash_ptr
#define build_time build_time_ptr
#else
#define config_hash ((uintptr_t) &ESPHOME_CONFIG_HASH)
#define build_time ((uintptr_t) &ESPHOME_BUILD_TIME)
#endif

const char *get_config_hash() {
  static char hash_str[9];
  if (!hash_str[0]) {
    snprintf(hash_str, sizeof(hash_str), "%08x", (uint32_t) config_hash);
  }
  return hash_str;
}

time_t get_build_time() { return (time_t) build_time; }

const char *get_build_time_string() {
  static char time_str[32];
  if (!time_str[0]) {
    time_t bt = get_build_time();
    struct tm *tm_info = localtime(&bt);
    strftime(time_str, sizeof(time_str), "%b %d %Y, %H:%M:%S", tm_info);
  }
  return time_str;
}

}  // namespace buildinfo
}  // namespace esphome
