#include "resol_deltasol_cs_plus.h"

#ifdef USE_VBUS_CS_PLUS_ENTITIES
#include <cmath>

namespace esphome::vbus {

void ResolDeltaSolCSPlus::register_setting_listener(CSPlusSettingListener *listener) {
  this->setting_listeners_.push_back(listener);
  if (this->has_fresh_snapshot())
    this->notify_entities_();
}

float ResolDeltaSolCSPlus::setting_value(uint16_t index) const {
  int32_t value;
  const auto spec = cs_plus::find_setting(index);
  if (!spec.has_value() || !this->get_value(index, value) ||
      !cs_plus::setting_allowed(*spec, value, [this](uint16_t id) {
        int32_t current = 0;
        this->get_value(id, current);
        return current;
      }))
    return NAN;
  return value * spec->scale;
}

bool ResolDeltaSolCSPlus::setting_range(uint16_t index, float &minimum, float &maximum, float &step) const {
  const auto spec = cs_plus::find_setting(index);
  if (!this->has_fresh_snapshot() || !spec.has_value() || spec->enum_mask != 0)
    return false;
  const auto get = [this](uint16_t id) {
    int32_t current = 0;
    this->get_value(id, current);
    return current;
  };
  if (!cs_plus::setting_visible(*spec, get))
    return false;
  int32_t first = spec->minimum;
  int32_t last = spec->maximum - (spec->maximum - spec->minimum) % spec->step;
  while (first <= last && !cs_plus::setting_allowed(*spec, first, get))
    first += spec->step;
  if (first > last)
    return false;
  while (last > first && !cs_plus::setting_allowed(*spec, last, get))
    last -= spec->step;
  minimum = first * spec->scale;
  maximum = last * spec->scale;
  step = spec->step * spec->scale;
  return true;
}

}  // namespace esphome::vbus
#endif
