/**
 * @file atomic.h
 * @brief Shim header for LVGL FreeRTOS support on ESP-IDF
 *
 * LVGL's lv_freertos.c includes "atomic.h" but on ESP-IDF
 * the file is located at "freertos/atomic.h". This shim
 * redirects to the correct location.
 *
 * See: https://github.com/lvgl/lvgl/issues/7589
 */

#pragma once

#ifdef ESP_PLATFORM
#include "freertos/atomic.h"
#endif
