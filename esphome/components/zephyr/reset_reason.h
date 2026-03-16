#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ZEPHYR) && (defined(USE_LOGGER_EARLY_MESSAGE) || defined(USE_DEBUG))

#include <cstddef>
#include <span>

namespace esphome::zephyr {

static constexpr size_t RESET_REASON_BUFFER_SIZE = 128;

const char *get_reset_reason(std::span<char, RESET_REASON_BUFFER_SIZE> buffer);

}  // namespace esphome::zephyr

#endif
