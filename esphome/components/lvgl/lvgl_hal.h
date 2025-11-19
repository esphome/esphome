//
// Created by Clyde Stubbs on 20/9/2023.
//

#pragma once
#ifdef USE_LVGL
#include <lvgl.h>

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

EXTERNC void *lv_malloc_core(size_t size);
EXTERNC void lv_free_core(void *ptr);
EXTERNC void *lv_realloc_core(void *ptr, size_t size);
EXTERNC lv_result_t lv_mem_test_core(void);

EXTERNC void lv_init_core(void);
EXTERNC void lv_deinit_core(void);
EXTERNC void lv_mem_monitor_core(lv_mem_monitor_t *mon_p);

#endif  // USE_LVGL
