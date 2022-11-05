#pragma once

#ifdef USE_ARDUINO

#include <map>
#include <utility>

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/controller.h"
#include "esphome/core/component.h"

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

#ifdef USE_SENSOR
  /// Return the type for prometheus
  void sensor_type_(AsyncResponseStream *stream);
  /// Return the sensor state as prometheus data point
  void sensor_row_(AsyncResponseStream *stream, sensor::Sensor *obj);
#endif

#ifdef USE_BINARY_SENSOR
  /// Return the type for prometheus
  void binary_sensor_type_(AsyncResponseStream *stream);
  /// Return the sensor state as prometheus data point
  void binary_sensor_row_(AsyncResponseStream *stream, binary_sensor::BinarySensor *obj);
#endif

#ifdef USE_FAN
  /// Return the type for prometheus
  void fan_type_(AsyncResponseStream *stream);
  /// Return the sensor state as prometheus data point
  void fan_row_(AsyncResponseStream *stream, fan::Fan *obj);
#endif

#ifdef USE_LIGHT
  /// Return the type for prometheus
  void light_type_(AsyncResponseStream *stream);
  /// Return the Light Values state as prometheus data point
  void light_row_(AsyncResponseStream *stream, light::LightState *obj);
#endif

#ifdef USE_COVER
  /// Return the type for prometheus
  void cover_type_(AsyncResponseStream *stream);
  /// Return the switch Values state as prometheus data point
  void cover_row_(AsyncResponseStream *stream, cover::Cover *obj);
#endif

#ifdef USE_SWITCH
  /// Return the type for prometheus
  void switch_type_(AsyncResponseStream *stream);
  /// Return the switch Values state as prometheus data point
  void switch_row_(AsyncResponseStream *stream, switch_::Switch *obj);
#endif

#ifdef USE_LOCK
  /// Return the type for prometheus
  void lock_type_(AsyncResponseStream *stream);
  /// Return the lock Values state as prometheus data point
  void lock_row_(AsyncResponseStream *stream, lock::Lock *obj);
#endif

  web_server_base::WebServerBase *base_;
  bool include_internal_{false};
  std::map<EntityBase *, std::string> relabel_map_id_;
  std::map<EntityBase *, std::string> relabel_map_name_;
};

}  // namespace prometheus
}  // namespace esphome

#endif  // USE_ARDUINO
