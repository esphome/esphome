#pragma once
// The named guard also lets other copies of this double share one build
#ifndef ESPHOME_TEST_FREERTOS_H
#define ESPHOME_TEST_FREERTOS_H
#include <cstdint>
using TickType_t = uint32_t;
using UBaseType_t = unsigned;
using BaseType_t = int;
inline constexpr BaseType_t pdPASS = 1;
#endif  // ESPHOME_TEST_FREERTOS_H
