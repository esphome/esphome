#pragma once

#include "list_entities.h"

#include "esphome/components/web_server_base/web_server_base.h"
#ifdef USE_WEBSERVER
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/entity_base.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include <functional>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

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

namespace esphome::web_server {

// Type for parameter names that can be stored in flash on ESP8266
#ifdef USE_ESP8266
using ParamNameType = const __FlashStringHelper *;
#else
using ParamNameType = const char *;
#endif

/// Result of matching a URL against an entity
struct EntityMatchResult {
  bool matched;          ///< True if entity matched the URL
  bool action_is_empty;  ///< True if no action/method segment in URL
};

/// Internal helper struct that is used to parse incoming URLs
struct UrlMatch {
  StringRef domain;  ///< Domain within URL, for example "sensor"
  StringRef id;      ///< Entity name/id within URL, for example "Temperature"
  StringRef method;  ///< Method within URL, for example "turn_on"
#ifdef USE_DEVICES
  StringRef device_name;  ///< Device name within URL, empty for main device
#endif
  bool valid{false};  ///< Whether this match is valid

  // Helper methods for string comparisons
  bool domain_equals(const char *str) const { return this->domain == str; }
  bool method_equals(const char *str) const { return this->method == str; }

#ifdef USE_ESP8266
  // Overloads for flash strings on ESP8266
  bool domain_equals(const __FlashStringHelper *str) const { return this->domain == str; }
  bool method_equals(const __FlashStringHelper *str) const { return this->method == str; }
#endif

  /// Match entity by name first, then fall back to object_id with deprecation warning
  /// Returns EntityMatchResult with match status and whether action segment is empty
  EntityMatchResult match_entity(EntityBase *entity) const;
};

#ifdef USE_WEBSERVER_SORTING
struct SortingComponents {
  float weight;
  uint64_t group_id;
};

struct SortingGroup {
  std::string name;
  float weight;
};
#endif

enum JsonDetail { DETAIL_ALL, DETAIL_STATE };

/*
  In order to defer updates in arduino mode, we need to create one AsyncEventSource per incoming request to /events.
  This is because only minimal changes were made to the ESPAsyncWebServer lib_dep, it was undesirable to put deferred
  update logic into that library. We need one deferred queue per connection so instead of one AsyncEventSource with
  multiple clients, we have multiple event sources with one client each. This is slightly awkward which is why it's
  implemented in a more straightforward way for ESP-IDF. Arduino platform will eventually go away and this workaround
  can be forgotten.
*/
#if !defined(USE_ESP32) && defined(USE_ARDUINO)
using message_generator_t = std::string(WebServer *, void *);

class DeferredUpdateEventSourceList;
class DeferredUpdateEventSource : public AsyncEventSource {
  friend class DeferredUpdateEventSourceList;

  /*
    This class holds a pointer to the source component that wants to publish a state event, and a pointer to a function
    that will lazily generate that event.  The two pointers allow dedup in the deferred queue if multiple publishes for
    the same component are backed up, and take up only 8 bytes of memory.  The entry in the deferred queue (a
    std::vector) is the DeferredEvent instance itself (not a pointer to one elsewhere in heap) so still only 8 bytes per
    entry (and no heap fragmentation).  Even 100 backed up events (you'd have to have at least 100 sensors publishing
    because of dedup) would take up only 0.8 kB.
  */
  struct DeferredEvent {
    friend class DeferredUpdateEventSource;

   protected:
    void *source_;
    message_generator_t *message_generator_;

   public:
    DeferredEvent(void *source, message_generator_t *message_generator)
        : source_(source), message_generator_(message_generator) {}
    bool operator==(const DeferredEvent &test) const {
      return (source_ == test.source_ && message_generator_ == test.message_generator_);
    }
  } __attribute__((packed));

 protected:
  // surface a couple methods from the base class
  using AsyncEventSource::handleRequest;
  using AsyncEventSource::send;

  ListEntitiesIterator entities_iterator_;
  // vector is used very specifically for its zero memory overhead even though items are popped from the front (memory
  // footprint is more important than speed here)
  std::vector<DeferredEvent> deferred_queue_;
  WebServer *web_server_;
  uint16_t consecutive_send_failures_{0};
  static constexpr uint16_t MAX_CONSECUTIVE_SEND_FAILURES = 2500;  // ~20 seconds at 125Hz loop rate

  // helper for allowing only unique entries in the queue
  void deq_push_back_with_dedup_(void *source, message_generator_t *message_generator);

  void process_deferred_queue_();

 public:
  DeferredUpdateEventSource(WebServer *ws, const String &url)
      : AsyncEventSource(url), entities_iterator_(ListEntitiesIterator(ws, this)), web_server_(ws) {}

  void loop();

  void deferrable_send_state(void *source, const char *event_type, message_generator_t *message_generator);
  void try_send_nodefer(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);
};

class DeferredUpdateEventSourceList : public std::list<DeferredUpdateEventSource *> {
 protected:
  void on_client_connect_(DeferredUpdateEventSource *source);
  void on_client_disconnect_(DeferredUpdateEventSource *source);

 public:
  void loop();

  void deferrable_send_state(void *source, const char *event_type, message_generator_t *message_generator);
  void try_send_nodefer(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);

  void add_new_client(WebServer *ws, AsyncWebServerRequest *request);
};
#endif

/** This class allows users to create a web server with their ESP nodes.
 *
 * Behind the scenes it's using AsyncWebServer to set up the server. It exposes 3 things:
 * an index page under '/' that's used to show a simple web interface (the css/js is hosted
 * by esphome.io by default), an event source under '/events' that automatically sends
 * all state updates in real time + the debug log. Lastly, there's an REST API available
 * under the '/light/...', '/sensor/...', ... URLs. A full documentation for this API
 * can be found under https://esphome.io/web-api/.
 */
class WebServer : public Controller,
                  public Component,
                  public AsyncWebHandler
#ifdef USE_LOGGER
    ,
                  public logger::LogListener
#endif
{
#if !defined(USE_ESP32) && defined(USE_ARDUINO)
  friend class DeferredUpdateEventSourceList;
#endif

 public:
  WebServer(web_server_base::WebServerBase *base);

#if USE_WEBSERVER_VERSION == 1
  /** Set the URL to the CSS <link> that's sent to each client. Defaults to
   * https://oi.esphome.io/v1/webserver-v1.min.css
   *
   * @param css_url The url to the web server stylesheet.
   */
  void set_css_url(const char *css_url);

  /** Set the URL to the script that's embedded in the index page. Defaults to
   * https://oi.esphome.io/v1/webserver-v1.min.js
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

#ifdef USE_LOGGER
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override;
#endif

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
  void on_sensor_update(sensor::Sensor *obj) override;
  /// Handle a sensor request under '/sensor/<id>'.
  void handle_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string sensor_state_json_generator(WebServer *web_server, void *source);
  static std::string sensor_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj) override;

  /// Handle a switch request under '/switch/<id>/</turn_on/turn_off/toggle>'.
  void handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string switch_state_json_generator(WebServer *web_server, void *source);
  static std::string switch_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_BUTTON
  /// Handle a button request under '/button/<id>/press'.
  void handle_button_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string button_state_json_generator(WebServer *web_server, void *source);
  static std::string button_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj) override;

  /// Handle a binary sensor request under '/binary_sensor/<id>'.
  void handle_binary_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string binary_sensor_state_json_generator(WebServer *web_server, void *source);
  static std::string binary_sensor_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_FAN
  void on_fan_update(fan::Fan *obj) override;

  /// Handle a fan request under '/fan/<id>/</turn_on/turn_off/toggle>'.
  void handle_fan_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string fan_state_json_generator(WebServer *web_server, void *source);
  static std::string fan_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;

  /// Handle a light request under '/light/<id>/</turn_on/turn_off/toggle>'.
  void handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string light_state_json_generator(WebServer *web_server, void *source);
  static std::string light_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj) override;

  /// Handle a text sensor request under '/text_sensor/<id>'.
  void handle_text_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string text_sensor_state_json_generator(WebServer *web_server, void *source);
  static std::string text_sensor_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;

  /// Handle a cover request under '/cover/<id>/<open/close/stop/set>'.
  void handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string cover_state_json_generator(WebServer *web_server, void *source);
  static std::string cover_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_NUMBER
  void on_number_update(number::Number *obj) override;
  /// Handle a number request under '/number/<id>'.
  void handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string number_state_json_generator(WebServer *web_server, void *source);
  static std::string number_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_DATETIME_DATE
  void on_date_update(datetime::DateEntity *obj) override;
  /// Handle a date request under '/date/<id>'.
  void handle_date_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string date_state_json_generator(WebServer *web_server, void *source);
  static std::string date_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_DATETIME_TIME
  void on_time_update(datetime::TimeEntity *obj) override;
  /// Handle a time request under '/time/<id>'.
  void handle_time_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string time_state_json_generator(WebServer *web_server, void *source);
  static std::string time_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_DATETIME_DATETIME
  void on_datetime_update(datetime::DateTimeEntity *obj) override;
  /// Handle a datetime request under '/datetime/<id>'.
  void handle_datetime_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string datetime_state_json_generator(WebServer *web_server, void *source);
  static std::string datetime_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_TEXT
  void on_text_update(text::Text *obj) override;
  /// Handle a text input request under '/text/<id>'.
  void handle_text_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string text_state_json_generator(WebServer *web_server, void *source);
  static std::string text_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_SELECT
  void on_select_update(select::Select *obj) override;
  /// Handle a select request under '/select/<id>'.
  void handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string select_state_json_generator(WebServer *web_server, void *source);
  static std::string select_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
  /// Handle a climate request under '/climate/<id>'.
  void handle_climate_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string climate_state_json_generator(WebServer *web_server, void *source);
  static std::string climate_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_LOCK
  void on_lock_update(lock::Lock *obj) override;

  /// Handle a lock request under '/lock/<id>/</lock/unlock/open>'.
  void handle_lock_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string lock_state_json_generator(WebServer *web_server, void *source);
  static std::string lock_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_VALVE
  void on_valve_update(valve::Valve *obj) override;

  /// Handle a valve request under '/valve/<id>/<open/close/stop/set>'.
  void handle_valve_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string valve_state_json_generator(WebServer *web_server, void *source);
  static std::string valve_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) override;

  /// Handle a alarm_control_panel request under '/alarm_control_panel/<id>'.
  void handle_alarm_control_panel_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string alarm_control_panel_state_json_generator(WebServer *web_server, void *source);
  static std::string alarm_control_panel_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_WATER_HEATER
  void on_water_heater_update(water_heater::WaterHeater *obj) override;

  /// Handle a water_heater request under '/water_heater/<id>/<mode/set>'.
  void handle_water_heater_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string water_heater_state_json_generator(WebServer *web_server, void *source);
  static std::string water_heater_all_json_generator(WebServer *web_server, void *source);
#endif

#ifdef USE_EVENT
  void on_event(event::Event *obj) override;

  static std::string event_state_json_generator(WebServer *web_server, void *source);
  static std::string event_all_json_generator(WebServer *web_server, void *source);

  /// Handle a event request under '/event<id>'.
  void handle_event_request(AsyncWebServerRequest *request, const UrlMatch &match);
#endif

#ifdef USE_UPDATE
  void on_update(update::UpdateEntity *obj) override;

  /// Handle a update request under '/update/<id>'.
  void handle_update_request(AsyncWebServerRequest *request, const UrlMatch &match);

  static std::string update_state_json_generator(WebServer *web_server, void *source);
  static std::string update_all_json_generator(WebServer *web_server, void *source);
#endif

  /// Override the web handler's canHandle method.
  bool canHandle(AsyncWebServerRequest *request) const override;
  /// Override the web handler's handleRequest method.
  void handleRequest(AsyncWebServerRequest *request) override;
  /// This web handle is not trivial.
  bool isRequestHandlerTrivial() const override;  // NOLINT(readability-identifier-naming)

#ifdef USE_WEBSERVER_SORTING
  void add_entity_config(EntityBase *entity, float weight, uint64_t group);
  void add_sorting_group(uint64_t group_id, const std::string &group_name, float weight);

  std::map<EntityBase *, SortingComponents> sorting_entitys_;
  std::map<uint64_t, SortingGroup> sorting_groups_;
#endif

  bool include_internal_{false};

 protected:
  void add_sorting_info_(JsonObject &root, EntityBase *entity);

#ifdef USE_LIGHT
  // Helper to parse and apply a float parameter with optional scaling
  template<typename T, typename Ret>
  void parse_light_param_(AsyncWebServerRequest *request, ParamNameType param_name, T &call, Ret (T::*setter)(float),
                          float scale = 1.0f) {
    if (request->hasParam(param_name)) {
      auto value = parse_number<float>(request->getParam(param_name)->value().c_str());
      if (value.has_value()) {
        (call.*setter)(*value / scale);
      }
    }
  }

  // Helper to parse and apply a uint32_t parameter with optional scaling
  template<typename T, typename Ret>
  void parse_light_param_uint_(AsyncWebServerRequest *request, ParamNameType param_name, T &call,
                               Ret (T::*setter)(uint32_t), uint32_t scale = 1) {
    if (request->hasParam(param_name)) {
      auto value = parse_number<uint32_t>(request->getParam(param_name)->value().c_str());
      if (value.has_value()) {
        (call.*setter)(*value * scale);
      }
    }
  }
#endif

  // Generic helper to parse and apply a float parameter
  template<typename T, typename Ret>
  void parse_float_param_(AsyncWebServerRequest *request, ParamNameType param_name, T &call, Ret (T::*setter)(float)) {
    if (request->hasParam(param_name)) {
      auto value = parse_number<float>(request->getParam(param_name)->value().c_str());
      if (value.has_value()) {
        (call.*setter)(*value);
      }
    }
  }

  // Generic helper to parse and apply an int parameter
  template<typename T, typename Ret>
  void parse_int_param_(AsyncWebServerRequest *request, ParamNameType param_name, T &call, Ret (T::*setter)(int)) {
    if (request->hasParam(param_name)) {
      auto value = parse_number<int>(request->getParam(param_name)->value().c_str());
      if (value.has_value()) {
        (call.*setter)(*value);
      }
    }
  }

  // Generic helper to parse and apply a string parameter
  template<typename T, typename Ret>
  void parse_string_param_(AsyncWebServerRequest *request, ParamNameType param_name, T &call,
                           Ret (T::*setter)(const std::string &)) {
    if (request->hasParam(param_name)) {
      // .c_str() is required for Arduino framework where value() returns Arduino String instead of std::string
      std::string value = request->getParam(param_name)->value().c_str();  // NOLINT(readability-redundant-string-cstr)
      (call.*setter)(value);
    }
  }

  // Generic helper to parse and apply a bool parameter
  // Accepts: "on", "true", "1" (case-insensitive) as true
  // Accepts: "off", "false", "0" (case-insensitive) as false
  // Invalid values are ignored (setter not called)
  template<typename T, typename Ret>
  void parse_bool_param_(AsyncWebServerRequest *request, ParamNameType param_name, T &call, Ret (T::*setter)(bool)) {
    if (request->hasParam(param_name)) {
      auto param_value = request->getParam(param_name)->value();
      // First check on/off (default), then true/false (custom)
      auto val = parse_on_off(param_value.c_str());
      if (val == PARSE_NONE) {
        val = parse_on_off(param_value.c_str(), "true", "false");
      }
      if (val == PARSE_ON || param_value == "1") {
        (call.*setter)(true);
      } else if (val == PARSE_OFF || param_value == "0") {
        (call.*setter)(false);
      }
      // PARSE_NONE/PARSE_TOGGLE: ignore invalid values
    }
  }

  web_server_base::WebServerBase *base_;
#ifdef USE_ESP32
  AsyncEventSource events_{"/events", this};
#elif USE_ARDUINO
  DeferredUpdateEventSourceList events_;
#endif

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
  bool expose_log_{true};

 private:
#ifdef USE_SENSOR
  std::string sensor_json_(sensor::Sensor *obj, float value, JsonDetail start_config);
#endif
#ifdef USE_SWITCH
  std::string switch_json_(switch_::Switch *obj, bool value, JsonDetail start_config);
#endif
#ifdef USE_BUTTON
  std::string button_json_(button::Button *obj, JsonDetail start_config);
#endif
#ifdef USE_BINARY_SENSOR
  std::string binary_sensor_json_(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config);
#endif
#ifdef USE_FAN
  std::string fan_json_(fan::Fan *obj, JsonDetail start_config);
#endif
#ifdef USE_LIGHT
  std::string light_json_(light::LightState *obj, JsonDetail start_config);
#endif
#ifdef USE_TEXT_SENSOR
  std::string text_sensor_json_(text_sensor::TextSensor *obj, const std::string &value, JsonDetail start_config);
#endif
#ifdef USE_COVER
  std::string cover_json_(cover::Cover *obj, JsonDetail start_config);
#endif
#ifdef USE_NUMBER
  std::string number_json_(number::Number *obj, float value, JsonDetail start_config);
#endif
#ifdef USE_DATETIME_DATE
  std::string date_json_(datetime::DateEntity *obj, JsonDetail start_config);
#endif
#ifdef USE_DATETIME_TIME
  std::string time_json_(datetime::TimeEntity *obj, JsonDetail start_config);
#endif
#ifdef USE_DATETIME_DATETIME
  std::string datetime_json_(datetime::DateTimeEntity *obj, JsonDetail start_config);
#endif
#ifdef USE_TEXT
  std::string text_json_(text::Text *obj, const std::string &value, JsonDetail start_config);
#endif
#ifdef USE_SELECT
  std::string select_json_(select::Select *obj, StringRef value, JsonDetail start_config);
#endif
#ifdef USE_CLIMATE
  std::string climate_json_(climate::Climate *obj, JsonDetail start_config);
#endif
#ifdef USE_LOCK
  std::string lock_json_(lock::Lock *obj, lock::LockState value, JsonDetail start_config);
#endif
#ifdef USE_VALVE
  std::string valve_json_(valve::Valve *obj, JsonDetail start_config);
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  std::string alarm_control_panel_json_(alarm_control_panel::AlarmControlPanel *obj,
                                        alarm_control_panel::AlarmControlPanelState value, JsonDetail start_config);
#endif
#ifdef USE_EVENT
  std::string event_json_(event::Event *obj, StringRef event_type, JsonDetail start_config);
#endif
#ifdef USE_WATER_HEATER
  std::string water_heater_json_(water_heater::WaterHeater *obj, JsonDetail start_config);
#endif
#ifdef USE_UPDATE
  std::string update_json_(update::UpdateEntity *obj, JsonDetail start_config);
#endif
};

}  // namespace esphome::web_server
#endif
