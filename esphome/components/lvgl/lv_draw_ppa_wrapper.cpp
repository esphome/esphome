/**
 * @file lv_draw_ppa_wrapper.cpp
 * Wrapper to compile the fixed PPA C source files within ESPHome's build system.
 * ESPHome only auto-compiles .cpp files from the component directory.
 * This wrapper includes all PPA .c files so they get linked properly.
 */

#include "esphome/core/defines.h"

#ifdef USE_LVGL_PPA

extern "C" {
#include "lv_draw_ppa.c"
#include "lv_draw_ppa_fill.c"
#include "lv_draw_ppa_img.c"
#include "lv_draw_ppa_buf.c"
#include "lvgl_ppa_accel_v9.c"
}

#endif /* USE_LVGL_PPA */
