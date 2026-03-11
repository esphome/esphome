#pragma once
/**
* This header is for use in components that might or might not use LVGL. There is a platformio bug where
the mere mention of a header file, even if ifdefed, causes the build to fail. This is a workaround, since if this
file is included in the build, LVGL is always included.
*/
#ifdef USE_LVGL
// required for clang-tidy
#ifndef LV_CONF_H
#define LV_CONF_SKIP 1  // NOLINT
#endif                  // LV_CONF_H

#include <lvgl.h>
namespace esphome {
namespace lvgl {}  // namespace lvgl
}  // namespace esphome
#endif  // USE_LVGL
