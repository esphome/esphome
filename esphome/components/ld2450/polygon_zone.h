#pragma once

#include "esphome/core/defines.h"
#ifdef LD2450_POLYGON_ZONE_COUNT

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text/text.h"
#include "esphome/core/preferences.h"
#include "polygon.h"

namespace esphome::ld2450 {

/// A zone with any polygon shape. The text state holds the polygon, so it can be edited and read back from HA.
class PolygonZone : public text::Text {
 public:
  explicit PolygonZone(binary_sensor::BinarySensor *presence) : presence_(presence) {}

  /// Load the saved polygon and publish the initial states. Called from the hub's setup().
  void setup();
  void dump_config();
  /// Publish whether any of the targets is inside the zone.
  void update(bool target_inside) { this->presence_->publish_state(target_inside); }
  bool contains(int16_t x, int16_t y) const { return this->polygon_.contains(x, y); }

 protected:
  void control(const std::string &value) override;
  void publish_polygon_();

  binary_sensor::BinarySensor *presence_;
  Polygon polygon_;
  ESPPreferenceObject pref_;
};

}  // namespace esphome::ld2450

#endif  // LD2450_POLYGON_ZONE_COUNT
