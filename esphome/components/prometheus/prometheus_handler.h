#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/controller.h"
#include "esphome/core/component.h"

namespace esphome {
namespace prometheus {

class PrometheusHandler : public AsyncWebHandler, public Component {
 public:
  PrometheusHandler(web_server_base::WebServerBase *base) : base_(base) {}

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
  void fan_row_(AsyncResponseStream *stream, fan::FanState *obj);
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

  web_server_base::WebServerBase *base_;
};

}  // namespace prometheus
}  // namespace esphome

#endif  // USE_ARDUINO
