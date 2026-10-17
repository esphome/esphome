#include "polygon_zone.h"
#ifdef LD2450_POLYGON_ZONE_COUNT

#include "esphome/core/log.h"

namespace esphome::ld2450 {

ESPHOME_LOG_TAG(TAG, "ld2450.polygon_zone");

void PolygonZone::setup() {
  // Bump the version when Polygon's layout changes
  this->pref_ = this->make_entity_preference<Polygon>(1);
  Polygon saved;
  if (this->pref_.load(&saved)) {
    if (saved.is_valid()) {
      this->polygon_ = saved;
    } else {
      ESP_LOGW(TAG, "'%s': saved polygon is invalid; zone disabled", this->get_name().c_str());
    }
  }
  // Presence stays unknown until the first radar frame
  this->publish_polygon_();
}

void PolygonZone::dump_config() {
  char polygon_s[POLYGON_STR_SIZE];
  this->polygon_.format(polygon_s);
  LOG_TEXT("  ", "PolygonZone", this);
  ESP_LOGCONFIG(TAG, "    Polygon: '%s'", polygon_s);
  LOG_BINARY_SENSOR("    ", "Presence", this->presence_);
}

void PolygonZone::control(const std::string &value) {
  Polygon polygon;
  if (!polygon.parse(value.c_str(), value.size())) {
    ESP_LOGW(TAG, "'%s': invalid polygon '%s'; expected 'x,y;x,y;...' with %u to %u points", this->get_name().c_str(),
             value.c_str(), MIN_POLYGON_POINTS, MAX_POLYGON_POINTS);
  } else if (!this->pref_.save(&polygon)) {
    // On ESP8266, the space for saved settings is small and can run out with several zones
    ESP_LOGW(TAG, "'%s': could not save the polygon; keeping the previous one", this->get_name().c_str());
  } else {
    this->polygon_ = polygon;
    // A target seen inside the previous polygon must not keep the new one on
    this->last_inside_ms_ = 0;
    if (this->polygon_.empty())
      this->presence_->invalidate_state();
  }
  // On error this publishes the previous polygon again, so the frontend shows the value in use
  this->publish_polygon_();
}

void PolygonZone::publish_polygon_() {
  char polygon_s[POLYGON_STR_SIZE];
  this->polygon_.format(polygon_s);
  this->publish_state(polygon_s);
}

}  // namespace esphome::ld2450

#endif  // LD2450_POLYGON_ZONE_COUNT
