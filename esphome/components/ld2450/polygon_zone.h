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
 *
 * Like the hub's has_target binary sensors, presence stays on until presence_timeout has passed since a target was
 * last inside the zone.
 */
class PolygonZone : public text::Text {
 public:
  /// Config validation guarantees every zone gets exactly one presence binary sensor.
  void set_presence_binary_sensor(binary_sensor::BinarySensor *presence) { this->presence_ = presence; }

  /// Load the saved polygon and publish it. Called from the hub's setup().
  void setup();
  void dump_config();
  /** Publish presence for one radar frame. A zone without a polygon keeps presence unknown.
   *
   * @param target_inside whether any target is inside the zone in this frame.
   * @param now the frame's time, in ms.
   * @param timed_out whether presence_timeout has passed since get_last_inside_ms().
   */
  void update(bool target_inside, uint32_t now, bool timed_out) {
    if (this->polygon_.empty())
      return;
    if (target_inside) {
      this->last_inside_ms_ = now;
      this->presence_->publish_state(true);
    } else if (timed_out) {
      this->presence_->publish_state(false);
    }
  }
  /// When a target was last inside the zone, in ms; 0 if never since the polygon was set.
  uint32_t get_last_inside_ms() const { return this->last_inside_ms_; }
  bool contains(int16_t x, int16_t y) const { return this->polygon_.contains(x, y); }

 protected:
  void control(const std::string &value) override;
  void publish_polygon_();

  binary_sensor::BinarySensor *presence_{nullptr};
  Polygon polygon_;
  ESPPreferenceObject pref_;
  uint32_t last_inside_ms_{0};
};

}  // namespace esphome::ld2450

#endif  // LD2450_POLYGON_ZONE_COUNT
