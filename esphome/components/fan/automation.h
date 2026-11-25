#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "fan.h"

namespace esphome::fan {

/// Codegen use only: step `fan` to its next speed. Past the top speed it turns off when `off_speed_cycle` is set,
/// otherwise it goes back to speed 1. A fan without speeds is toggled.
void cycle_speed(Fan *fan, bool off_speed_cycle);

// Codegen use only: the change triggers' test of `now` against the value the trigger last saw, which they keep.
template<typename T> bool update_changed(T &last, T now) {
  bool changed = now != last;
  last = now;
  return changed;
}
inline bool update_turned_on(bool &last, bool now) {
  bool turned_on = now && !last;
  last = now;
  return turned_on;
}
inline bool update_turned_off(bool &last, bool now) {
  bool turned_off = !now && last;
  last = now;
  return turned_off;
}
inline bool update_preset_changed(const char *&last, StringRef now) {
  bool changed = now != StringRef::from_maybe_nullptr(last);
  last = now.c_str();
  return changed;
}

}  // namespace esphome::fan
