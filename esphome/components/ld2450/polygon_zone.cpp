#include "polygon_zone.h"
#ifdef LD2450_POLYGON_ZONE_COUNT

#include "esphome/core/log.h"

namespace esphome::ld2450 {

static const char *const TAG = "ld2450.polygon_zone";

void PolygonZone::setup() {
  this->pref_ = this->make_entity_preference<Polygon>();
  Polygon saved;
  if (this->pref_.load(&saved) && saved.is_valid()) {
    this->polygon_ = saved;
  }
  this->publish_polygon_();
  this->presence_->publish_initial_state(false);
}

void PolygonZone::dump_config() {
  char polygon_s[POLYGON_STR_SIZE];
  this->polygon_.format(polygon_s);
  LOG_TEXT("  ", "PolygonZone", this);
  ESP_LOGCONFIG(TAG, "    Polygon: '%s'", polygon_s);
  LOG_BINARY_SENSOR("    ", "Presence", this->presence_);
}

void PolygonZone::control(const std::string &value) {
  if (this->polygon_.parse(value.c_str(), value.size())) {
    this->pref_.save(&this->polygon_);
  } else {
    ESP_LOGW(TAG, "'%s': invalid polygon '%s'; expected 'x,y;x,y;...' with %u to %u points", this->get_name().c_str(),
             value.c_str(), MIN_POLYGON_POINTS, MAX_POLYGON_POINTS);
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
