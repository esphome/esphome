#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/components/web_server_base/web_server_base.h"

#include <vector>

namespace esphome {
namespace web_server {

/// Internal helper struct that is used to parse incoming URLs
struct UrlMatch {
  std::string domain;  ///< The domain of the component, for example "sensor"
  std::string id;      ///< The id of the device that's being accessed, for example "living_room_fan"
  std::string method;  ///< The method that's being called, for example "turn_on"
  bool valid;          ///< Whether this match is valid
};

/** This class allows users to create a web server with their ESP nodes.
 *
 * Behind the scenes it's using AsyncWebServer to set up the server. It exposes 3 things:
 * an index page under '/' that's used to show a simple web interface (the css/js is hosted
 * by esphome.io by default), an event source under '/events' that automatically sends
 * all state updates in real time + the debug log. Lastly, there's an REST API available
 * under the '/light/...', '/sensor/...', ... URLs. A full documentation for this API
 * can be found under https://esphome.io/web-api/index.html.
 */
class WebServer : public Controller, public Component, public AsyncWebHandler {
 public:
  WebServer(web_server_base::WebServerBase *base) : base_(base) {}

  /** Set the URL to the CSS <link> that's sent to each client. Defaults to
   * https://esphome.io/_static/webserver-v1.min.css
   *
   * @param css_url The url to the web server stylesheet.
   */
  void set_css_url(const char *css_url);

  /** Set local path to the script that's embedded in the index page. Defaults to
   *
   * @param css_include Local path to web server script.
   */
  void set_css_include(const char *css_include);

  /** Set the URL to the script that's embedded in the index page. Defaults to
   * https://esphome.io/_static/webserver-v1.min.js
   *
   * @param js_url The url to the web server script.
   */
  void set_js_url(const char *js_url);

  /** Set local path to the script that's embedded in the index page. Defaults to
   *
   * @param js_include Local path to web server script.
   */
  void set_js_include(const char *js_include);

  /** Determine whether internal components should be displayed on the web server.
   * Defaults to false.
   *
   * @param include_internal Whether internal components should be displayed.
   */
  void set_include_internal(bool include_internal) { include_internal_ = include_internal; }
  /** Set whether or not the webserver should expose the OTA form and handler.
   *
   * @param allow_ota.
   */
  void set_allow_ota(bool allow_ota) { this->allow_ota_ = allow_ota; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup the internal web server and register handlers.
  void setup() override;

  void dump_config() override;

  /// MQTT setup priority.
  float get_setup_priority() const override;

  /// Handle an index request under '/'.
  void handle_index_request(AsyncWebServerRequest *request);

#ifdef WEBSERVER_CSS_INCLUDE
  /// Handle included css request under '/0.css'.
  void handle_css_request(AsyncWebServerRequest *request);
#endif

#ifdef WEBSERVER_JS_INCLUDE
  /// Handle included js request under '/0.js'.
  void handle_js_request(AsyncWebServerRequest *request);
#endif

#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj, float state) override;
  /// Handle a sensor request under '/sensor/<id>'.
  void handle_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the sensor state with its value as a JSON string.
  std::string sensor_json(sensor::Sensor *obj, float value);
#endif

#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj, bool state) override;

  /// Handle a switch request under '/switch/<id>/</turn_on/turn_off/toggle>'.
  void handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the switch state with its value as a JSON string.
  std::string switch_json(switch_::Switch *obj, bool value);
#endif

#ifdef USE_BUTTON
  /// Handle a button request under '/button/<id>/press'.
  void handle_button_request(AsyncWebServerRequest *request, const UrlMatch &match);
#endif

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) override;

  /// Handle a binary sensor request under '/binary_sensor/<id>'.
  void handle_binary_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the binary sensor state with its value as a JSON string.
  std::string binary_sensor_json(binary_sensor::BinarySensor *obj, bool value);
#endif

#ifdef USE_FAN
  void on_fan_update(fan::FanState *obj) override;

  /// Handle a fan request under '/fan/<id>/</turn_on/turn_off/toggle>'.
  void handle_fan_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the fan state as a JSON string.
  std::string fan_json(fan::FanState *obj);
#endif

#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;

  /// Handle a light request under '/light/<id>/</turn_on/turn_off/toggle>'.
  void handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the light state as a JSON string.
  std::string light_json(light::LightState *obj);
#endif

#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) override;

  /// Handle a text sensor request under '/text_sensor/<id>'.
  void handle_text_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the text sensor state with its value as a JSON string.
  std::string text_sensor_json(text_sensor::TextSensor *obj, const std::string &value);
#endif

#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;

  /// Handle a cover request under '/cover/<id>/<open/close/stop/set>'.
  void handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the cover state as a JSON string.
  std::string cover_json(cover::Cover *obj);
#endif

#ifdef USE_NUMBER
  void on_number_update(number::Number *obj, float state) override;
  /// Handle a number request under '/number/<id>'.
  void handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the number state with its value as a JSON string.
  std::string number_json(number::Number *obj, float value);
#endif

#ifdef USE_SELECT
  void on_select_update(select::Select *obj, const std::string &state) override;
  /// Handle a select request under '/select/<id>'.
  void handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the number state with its value as a JSON string.
  std::string select_json(select::Select *obj, const std::string &value);
#endif

  /// Override the web handler's canHandle method.
  bool canHandle(AsyncWebServerRequest *request) override;
  /// Override the web handler's handleRequest method.
  void handleRequest(AsyncWebServerRequest *request) override;
  /// This web handle is not trivial.
  bool isRequestHandlerTrivial() override;

 protected:
  web_server_base::WebServerBase *base_;
  AsyncEventSource events_{"/events"};
  const char *css_url_{nullptr};
  const char *css_include_{nullptr};
  const char *js_url_{nullptr};
  const char *js_include_{nullptr};
  bool include_internal_{false};
  bool allow_ota_{true};
};

}  // namespace web_server
}  // namespace esphome

#endif  // USE_ARDUINO
