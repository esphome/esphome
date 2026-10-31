#pragma once

#ifdef USE_LIBRETINY

#include "esphome/core/defines.h"

#ifdef USE_LIBRETINY_CRASH_HANDLER

#if defined(USE_LIBRETINY_VARIANT_RTL8720C) || defined(USE_LIBRETINY_VARIANT_RTL8720D) || defined(USE_LN882X)
// Cortex-M families: the recorder goes in front of the SDK's fault handlers through the RAM vector table.
#define USE_LIBRETINY_CRASH_HANDLER_VECTOR_PATCH
#endif

namespace esphome::libretiny {

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Returns true if crash data was found this boot, reading it first if needed.
bool crash_handler_has_data();

#ifdef USE_LIBRETINY_CRASH_HANDLER_VECTOR_PATCH
/// Put the crash recorder in front of the SDK's fault handlers.
void crash_handler_install();
#endif

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY_CRASH_HANDLER
#endif  // USE_LIBRETINY
