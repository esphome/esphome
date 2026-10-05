#pragma once

#include "esphome/core/defines.h"
#ifdef LD2450_POLYGON_ZONE_COUNT

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text/text.h"
#include "esphome/core/preferences.h"
#include "polygon.h"

namespace esphome::ld2450 {

/** A zone with any polygon shape. The text state holds the polygon, so it can be edited and read back from HA.
 *
 * The polygon is in the radar's raw coordinates (mm), and targets are checked against it as decoded from each
 * radar frame, before any sensor filter. Filters on the target X/Y sensors change only what they publish, so a
 * filter that changes their values (multiply, offset, lambda, ...) puts them in a different frame from the polygon.
 */
class PolygonZone : public text::Text {
 public:
  /// Config validation guarantees every zone gets exactly one presence binary sensor.
  void set_presence_binary_sensor(binary_sensor::BinarySensor *presence) { this->presence_ = presence; }

  /// Load the saved polygon and publish it. Called from the hub's setup().
  void setup();
  void dump_config();
  /// Publish whether any of the targets is inside the zone. A zone without a polygon keeps presence unknown.
  void update(bool target_inside) {
    if (!this->polygon_.empty())
      this->presence_->publish_state(target_inside);
  }
  bool contains(int16_t x, int16_t y) const { return this->polygon_.contains(x, y); }

 protected:
  void control(const std::string &value) override;
  void publish_polygon_();

  binary_sensor::BinarySensor *presence_{nullptr};
  Polygon polygon_;
  ESPPreferenceObject pref_;
};

}  // namespace esphome::ld2450

#endif  // LD2450_POLYGON_ZONE_COUNT
