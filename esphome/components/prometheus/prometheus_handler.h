#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <map>
#include <utility>

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/entity_base.h"

namespace esphome {
namespace prometheus {

class PrometheusHandler : public AsyncWebHandler, public Component {
 public:
  PrometheusHandler(web_server_base::WebServerBase *base) : base_(base) {}

  /** Determine whether internal components should be exported as metrics.
   * Defaults to false.
   *
   * @param include_internal Whether internal components should be exported.
   */
  void set_include_internal(bool include_internal) { include_internal_ = include_internal; }

  /** Add the value for an entity's "id" label.
   *
   * @param obj The entity for which to set the "id" label
   * @param value The value for the "id" label
   */
  void add_label_id(EntityBase *obj, const std::string &value) { relabel_map_id_.insert({obj, value}); }

  /** Add the value for an entity's "name" label.
   *
   * @param obj The entity for which to set the "name" label
   * @param value The value for the "name" label
   */
  void add_label_name(EntityBase *obj, const std::string &value) { relabel_map_name_.insert({obj, value}); }

  bool canHandle(AsyncWebServerRequest *request) override {
    if (request->method() == HTTP_GET) {
      if (request->url() == "/metrics")
        return true;
    }

    return false;
  }

  void handleRequest(AsyncWebServerRequest *req) override;

  void setup() override {
    this->base_->init();
    this->base_->add_handler(this);
  }
  float get_setup_priority() const override {
    // After WiFi
    return setup_priority::WIFI - 1.0f;
  }

 protected:
  std::string relabel_id_(EntityBase *obj);
  std::string relabel_name_(EntityBase *obj);
  void add_area_label_(AsyncResponseStream *stream, std::string &area);
  void add_node_label_(AsyncResponseStream *stream, std::string &node);
  void add_friendly_name_label_(AsyncResponseStream *stream, std::string &friendly_name);

#ifdef USE_SENSOR
  /// Return the type for prometheus
  void sensor_type_(AsyncResponseStream *stream);
  /// Return the sensor state as prometheus data point
  void sensor_row_(AsyncResponseStream *stream, sensor::Sensor *obj, std::string &area, std::string &node,
                   std::string &friendly_name);
#endif

#ifdef USE_BINARY_SENSOR
  /// Return the type for prometheus
  void binary_sensor_type_(AsyncResponseStream *stream);
  /// Return the binary sensor state as prometheus data point
  void binary_sensor_row_(AsyncResponseStream *stream, binary_sensor::BinarySensor *obj, std::string &area,
                          std::string &node, std::string &friendly_name);
#endif

#ifdef USE_FAN
  /// Return the type for prometheus
  void fan_type_(AsyncResponseStream *stream);
  /// Return the fan state as prometheus data point
  void fan_row_(AsyncResponseStream *stream, fan::Fan *obj, std::string &area, std::string &node,
                std::string &friendly_name);
#endif

#ifdef USE_LIGHT
  /// Return the type for prometheus
  void light_type_(AsyncResponseStream *stream);
  /// Return the light values state as prometheus data point
  void light_row_(AsyncResponseStream *stream, light::LightState *obj, std::string &area, std::string &node,
                  std::string &friendly_name);
#endif

#ifdef USE_COVER
  /// Return the type for prometheus
  void cover_type_(AsyncResponseStream *stream);
  /// Return the cover values state as prometheus data point
  void cover_row_(AsyncResponseStream *stream, cover::Cover *obj, std::string &area, std::string &node,
                  std::string &friendly_name);
#endif

#ifdef USE_SWITCH
  /// Return the type for prometheus
  void switch_type_(AsyncResponseStream *stream);
  /// Return the switch values state as prometheus data point
  void switch_row_(AsyncResponseStream *stream, switch_::Switch *obj, std::string &area, std::string &node,
                   std::string &friendly_name);
#endif

#ifdef USE_LOCK
  /// Return the type for prometheus
  void lock_type_(AsyncResponseStream *stream);
  /// Return the lock values state as prometheus data point
  void lock_row_(AsyncResponseStream *stream, lock::Lock *obj, std::string &area, std::string &node,
                 std::string &friendly_name);
#endif

#ifdef USE_TEXT_SENSOR
  /// Return the type for prometheus
  void text_sensor_type_(AsyncResponseStream *stream);
  /// Return the text sensor values state as prometheus data point
  void text_sensor_row_(AsyncResponseStream *stream, text_sensor::TextSensor *obj, std::string &area, std::string &node,
                        std::string &friendly_name);
#endif

#ifdef USE_NUMBER
  /// Return the type for prometheus
  void number_type_(AsyncResponseStream *stream);
  /// Return the number state as prometheus data point
  void number_row_(AsyncResponseStream *stream, number::Number *obj, std::string &area, std::string &node,
                   std::string &friendly_name);
#endif

#ifdef USE_SELECT
  /// Return the type for prometheus
  void select_type_(AsyncResponseStream *stream);
  /// Return the select state as prometheus data point
  void select_row_(AsyncResponseStream *stream, select::Select *obj, std::string &area, std::string &node,
                   std::string &friendly_name);
#endif

#ifdef USE_MEDIA_PLAYER
  /// Return the type for prometheus
  void media_player_type_(AsyncResponseStream *stream);
  /// Return the media player state as prometheus data point
  void media_player_row_(AsyncResponseStream *stream, media_player::MediaPlayer *obj, std::string &area,
                         std::string &node, std::string &friendly_name);
#endif

#ifdef USE_UPDATE
  /// Return the type for prometheus
  void update_entity_type_(AsyncResponseStream *stream);
  /// Return the update state and info as prometheus data point
  void update_entity_row_(AsyncResponseStream *stream, update::UpdateEntity *obj, std::string &area, std::string &node,
                          std::string &friendly_name);
  void handle_update_state_(AsyncResponseStream *stream, update::UpdateState state);
#endif

#ifdef USE_VALVE
  /// Return the type for prometheus
  void valve_type_(AsyncResponseStream *stream);
  /// Return the valve state as prometheus data point
  void valve_row_(AsyncResponseStream *stream, valve::Valve *obj, std::string &area, std::string &node,
                  std::string &friendly_name);
#endif

  web_server_base::WebServerBase *base_;
  bool include_internal_{false};
  std::map<EntityBase *, std::string> relabel_map_id_;
  std::map<EntityBase *, std::string> relabel_map_name_;
};

}  // namespace prometheus
}  // namespace esphome
#endif
