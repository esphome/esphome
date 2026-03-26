#pragma once

#ifdef USE_RP2040
#include "esphome/core/helpers.h"

namespace esphome::logger {

// Single write with newline already in buffer (added by caller)
inline void HOT Logger::write_msg_(const char *msg, uint16_t len) { this->hw_serial_->write(msg, len); }

}  // namespace esphome::logger

#endif
