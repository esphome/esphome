#pragma once

#include "list_entities.h"

#include "esphome/components/web_server_base/web_server_base.h"
#ifdef USE_WEBSERVER
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/entity_base.h"

#include <map>
#include <vector>
#include <list>
#include <functional>
#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <deque>
#endif

#if USE_WEBSERVER_VERSION >= 2
extern const uint8_t ESPHOME_WEBSERVER_INDEX_HTML[] PROGMEM;
extern const size_t ESPHOME_WEBSERVER_INDEX_HTML_SIZE;
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
extern const uint8_t ESPHOME_WEBSERVER_CSS_INCLUDE[] PROGMEM;
extern const size_t ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE;
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
extern const uint8_t ESPHOME_WEBSERVER_JS_INCLUDE[] PROGMEM;
extern const size_t ESPHOME_WEBSERVER_JS_INCLUDE_SIZE;
#endif

namespace esphome {
namespace web_server {

/// Internal helper struct that is used to parse incoming URLs
struct UrlMatch {
  std::string domain;  ///< The domain of the component, for example "sensor"
  std::string id;      ///< The id of the device that's being accessed, for example "living_room_fan"
  std::string method;  ///< The method that's being called, for example "turn_on"
  bool valid;          ///< Whether this match is valid
};

struct SortingComponents {
  float weight;
};

enum JsonDetail { DETAIL_ALL, DETAIL_STATE };

/*
  This class holds a pointer to the event source, the event type, and a pointer to a lambda that will lazily 
  generate the event body.  The source and type allow dedup in the deferred queue and the lambda saves on
  having to store the message body upfront.  The lambda should point back into the DeferredEvent itself for 
  parameters so it only need to closure a single reference (4 bytes).

  The three pointers, plus the entry in the deq should equal 16 bytes per entry all-told.
*/
  class DeferredEvent {
  public:
    DeferredEvent(void* source, const char* event_type) {
      source_ = source;
      event_type_ = event_type;
    }
    DeferredEvent(DeferredEvent* to_clone) {
      source_ = to_clone->source_;
      event_type_ = to_clone->event_type_;
      message_generator_ = to_clone->message_generator_;
    }
    void* source_;
    const char* event_type_;
    std::function<const char* (WebServer* web_server, void* source)> message_generator_;
};

class DeferredUpdateSourceList;

class DeferredUpdateEventSource: public AsyncEventSource {
  friend class DeferredUpdateSourceList;

public:
  DeferredUpdateEventSource(WebServer *ws, const String& url)  :
    AsyncEventSource(url),
    entities_iterator_(ListEntitiesIterator(ws, this)),
    web_server_(ws) {}

  using AsyncEventSource::handleRequest;

  ListEntitiesIterator entities_iterator_;

protected:
  using AsyncEventSource::send;
  std::list<DeferredEvent*> deq_;
  WebServer * web_server_;

  // helper for allowing only unique entries in the queue
  void deq_clone_and_push_back_with_dedup(DeferredEvent* item);

  void process_deferred_queue();

public:
  void loop();

  void send(DeferredEvent* de);

  // mainly used for logs plus the initial ping
  void try_send_nodefer(const char *message, const char *event=NULL, uint32_t id=0, uint32_t reconnect=0);
};

class DeferredUpdateEventSourceList: public std::list<DeferredUpdateEventSource*> {
public:
  void loop();

  void send(DeferredEvent* event);
  void try_send_nodefer(const char *message, const char *event=NULL, uint32_t id=0, uint32_t reconnect=0);

  void add_new_client(WebServer* ws, AsyncWebServerRequest *request, std::function<const char* ()> generate_config_json, bool include_internal);

  void on_client_connect(DeferredUpdateEventSource* source, std::function<const char* ()> generate_config_json, bool include_internal);
  void remove_client(DeferredUpdateEventSource* source);
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
friend class DeferredUpdateEventSourceList;

 public:
  WebServer(web_server_base::WebServerBase *base);

#if USE_WEBSERVER_VERSION == 1
  /** Set the URL to the CSS <link> that's sent to each client. Defaults to
   * https://esphome.io/_static/webserver-v1.min.css
   *
   * @param css_url The url to the web server stylesheet.
   */
  void set_css_url(const char *css_url);

  /** Set the URL to the script that's embedded in the index page. Defaults to
   * https://esphome.io/_static/webserver-v1.min.js
   *
   * @param js_url The url to the web server script.
   */
  void set_js_url(const char *js_url);
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
  /** Set local path to the script that's embedded in the index page. Defaults to
   *
   * @param css_include Local path to web server script.
   */
  void set_css_include(const char *css_include);
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
  /** Set local path to the script that's embedded in the index page. Defaults to
   *
   * @param js_include Local path to web server script.
   */
  void set_js_include(const char *js_include);
#endif

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
  /** Set whether or not the webserver should expose the Log.
   *
   * @param expose_log.
   */
  void set_expose_log(bool expose_log) { this->expose_log_ = expose_log; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup the internal web server and register handlers.
  void setup() override;
  void loop() override;

  void dump_config() override;

  /// MQTT setup priority.
  float get_setup_priority() const override;

  /// Handle an index request under '/'.
  void handle_index_request(AsyncWebServerRequest *request);

  /// Return the webserver configuration as JSON.
  std::string get_config_json();

#ifdef USE_WEBSERVER_CSS_INCLUDE
  /// Handle included css request under '/0.css'.
  void handle_css_request(AsyncWebServerRequest *request);
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
  /// Handle included js request under '/0.js'.
  void handle_js_request(AsyncWebServerRequest *request);
#endif

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
  // Handle Private Network Access CORS OPTIONS request
  void handle_pna_cors_request(AsyncWebServerRequest *request);
#endif

#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj, float state) override;
  /// Handle a sensor request under '/sensor/<id>'.
  void handle_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the sensor state with its value as a JSON string.
  std::string sensor_json(sensor::Sensor *obj, float value, JsonDetail start_config);
#endif

#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj, bool state) override;

  /// Handle a switch request under '/switch/<id>/</turn_on/turn_off/toggle>'.
  void handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the switch state with its value as a JSON string.
  std::string switch_json(switch_::Switch *obj, bool value, JsonDetail start_config);
#endif

#ifdef USE_BUTTON
  /// Handle a button request under '/button/<id>/press'.
  void handle_button_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the button details with its value as a JSON string.
  std::string button_json(button::Button *obj, JsonDetail start_config);
#endif

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) override;

  /// Handle a binary sensor request under '/binary_sensor/<id>'.
  void handle_binary_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the binary sensor state with its value as a JSON string.
  std::string binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config);
#endif

#ifdef USE_FAN
  void on_fan_update(fan::Fan *obj) override;

  /// Handle a fan request under '/fan/<id>/</turn_on/turn_off/toggle>'.
  void handle_fan_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the fan state as a JSON string.
  std::string fan_json(fan::Fan *obj, JsonDetail start_config);
#endif

#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;

  /// Handle a light request under '/light/<id>/</turn_on/turn_off/toggle>'.
  void handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the light state as a JSON string.
  std::string light_json(light::LightState *obj, JsonDetail start_config);
#endif

#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) override;

  /// Handle a text sensor request under '/text_sensor/<id>'.
  void handle_text_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the text sensor state with its value as a JSON string.
  std::string text_sensor_json(text_sensor::TextSensor *obj, const std::string &value, JsonDetail start_config);
#endif

#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;

  /// Handle a cover request under '/cover/<id>/<open/close/stop/set>'.
  void handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the cover state as a JSON string.
  std::string cover_json(cover::Cover *obj, JsonDetail start_config);
#endif

#ifdef USE_NUMBER
  void on_number_update(number::Number *obj, float state) override;
  /// Handle a number request under '/number/<id>'.
  void handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the number state with its value as a JSON string.
  std::string number_json(number::Number *obj, float value, JsonDetail start_config);
#endif

#ifdef USE_DATETIME_DATE
  void on_date_update(datetime::DateEntity *obj) override;
  /// Handle a date request under '/date/<id>'.
  void handle_date_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the date state with its value as a JSON string.
  std::string date_json(datetime::DateEntity *obj, JsonDetail start_config);
#endif

#ifdef USE_DATETIME_TIME
  void on_time_update(datetime::TimeEntity *obj) override;
  /// Handle a time request under '/time/<id>'.
  void handle_time_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the time state with its value as a JSON string.
  std::string time_json(datetime::TimeEntity *obj, JsonDetail start_config);
#endif

#ifdef USE_DATETIME_DATETIME
  void on_datetime_update(datetime::DateTimeEntity *obj) override;
  /// Handle a datetime request under '/datetime/<id>'.
  void handle_datetime_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the datetime state with its value as a JSON string.
  std::string datetime_json(datetime::DateTimeEntity *obj, JsonDetail start_config);
#endif

#ifdef USE_TEXT
  void on_text_update(text::Text *obj, const std::string &state) override;
  /// Handle a text input request under '/text/<id>'.
  void handle_text_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the text state with its value as a JSON string.
  std::string text_json(text::Text *obj, const std::string &value, JsonDetail start_config);
#endif

#ifdef USE_SELECT
  void on_select_update(select::Select *obj, const std::string &state, size_t index) override;
  /// Handle a select request under '/select/<id>'.
  void handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the select state with its value as a JSON string.
  std::string select_json(select::Select *obj, const std::string &value, JsonDetail start_config);
#endif

#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
  /// Handle a climate request under '/climate/<id>'.
  void handle_climate_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the climate details
  std::string climate_json(climate::Climate *obj, JsonDetail start_config);
#endif

#ifdef USE_LOCK
  void on_lock_update(lock::Lock *obj) override;

  /// Handle a lock request under '/lock/<id>/</lock/unlock/open>'.
  void handle_lock_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the lock state with its value as a JSON string.
  std::string lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config);
#endif

#ifdef USE_VALVE
  void on_valve_update(valve::Valve *obj) override;

  /// Handle a valve request under '/valve/<id>/<open/close/stop/set>'.
  void handle_valve_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the valve state as a JSON string.
  std::string valve_json(valve::Valve *obj, JsonDetail start_config);
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) override;

  /// Handle a alarm_control_panel request under '/alarm_control_panel/<id>'.
  void handle_alarm_control_panel_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the alarm_control_panel state with its value as a JSON string.
  std::string alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                              alarm_control_panel::AlarmControlPanelState value, JsonDetail start_config);
#endif

#ifdef USE_EVENT
  void on_event(event::Event *obj, const std::string &event_type) override;

  /// Dump the event details with its value as a JSON string.
  std::string event_json(event::Event *obj, const std::string &event_type, JsonDetail start_config);
#endif

#ifdef USE_UPDATE
  void on_update(update::UpdateEntity *obj) override;

  /// Handle a update request under '/update/<id>'.
  void handle_update_request(AsyncWebServerRequest *request, const UrlMatch &match);

  /// Dump the update state with its value as a JSON string.
  std::string update_json(update::UpdateEntity *obj, JsonDetail start_config);
#endif

  /// Override the web handler's canHandle method.
  bool canHandle(AsyncWebServerRequest *request) override;
  /// Override the web handler's handleRequest method.
  void handleRequest(AsyncWebServerRequest *request) override;
  /// This web handle is not trivial.
  bool isRequestHandlerTrivial() override;  // NOLINT(readability-identifier-naming)

  void add_entity_to_sorting_list(EntityBase *entity, float weight);

 protected:
  void schedule_(std::function<void()> &&f);
  web_server_base::WebServerBase *base_;
  DeferredUpdateEventSourceList events_list_;
  std::map<EntityBase *, SortingComponents> sorting_entitys_;
#if USE_WEBSERVER_VERSION == 1
  const char *css_url_{nullptr};
  const char *js_url_{nullptr};
#endif
#ifdef USE_WEBSERVER_CSS_INCLUDE
  const char *css_include_{nullptr};
#endif
#ifdef USE_WEBSERVER_JS_INCLUDE
  const char *js_include_{nullptr};
#endif
  bool include_internal_{false};
  bool allow_ota_{true};
  bool expose_log_{true};
#ifdef USE_ESP32
  std::deque<std::function<void()>> to_schedule_;
  SemaphoreHandle_t to_schedule_lock_;
#endif
};

}  // namespace web_server
}  // namespace esphome
#endif
