#pragma once

#include <initializer_list>
#include <utility>

#include "esphome/components/lvgl/lvgl_esphome.h"

using LvglTestWidgetName = std::pair<lv_obj_t *, const char *>;

// "none" for no widget, which is also what lv_obj_get_child() gives for a list item not yet added
inline const char *lvgl_test_name(lv_obj_t *obj, std::initializer_list<LvglTestWidgetName> names) {
  if (obj == nullptr)
    return "none";
  for (const auto &entry : names) {
    if (entry.first == obj)
      return entry.second;
  }
  return "other";
}

inline lv_obj_t *lvgl_test_focused(lv_indev_t *input) { return lv_group_get_focused(lv_indev_get_group(input)); }

inline int lvgl_test_pressed(lv_indev_t *input) { return lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED; }
