//
// Created by Clyde Stubbs on 20/9/2023.
//

#pragma once

#ifdef __cplusplus
#define EXTERNC extern "C"
#include <cstddef>
namespace esphome {
namespace lvgl {}
}  // namespace esphome
#else
#define EXTERNC extern
#include <stddef.h>
#endif

EXTERNC size_t lv_millis(void);
EXTERNC void *lv_custom_mem_alloc(size_t size);
EXTERNC void lv_custom_mem_free(void *ptr);
EXTERNC void *lv_custom_mem_realloc(void *ptr, size_t size);
