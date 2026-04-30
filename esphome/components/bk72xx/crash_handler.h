#pragma once

#ifdef USE_BK72XX

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_CRASH_HANDLER

namespace esphome::bk72xx {

/// Read crash data from .noinit memory and clear the magic marker so the
/// next boot doesn't re-report it. Crash data is copied into a static and
/// remains accessible via crash_handler_log() / crash_handler_has_data() for
/// the rest of this boot session.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Returns true if crash data was found this boot.
bool crash_handler_has_data();

}  // namespace esphome::bk72xx

#endif  // USE_BK72XX_CRASH_HANDLER
#endif  // USE_BK72XX
