#include "web_server.h"
#ifdef USE_WEBSERVER
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#if !defined(USE_ESP32) && defined(USE_ARDUINO)
#include "StreamString.h"
#endif

#include <cstdlib>

#ifdef USE_LIGHT
#include "esphome/components/light/light_json_schema.h"
#endif

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif

#ifdef USE_WEBSERVER_LOCAL
#if USE_WEBSERVER_VERSION == 2
#include "server_index_v2.h"
#elif USE_WEBSERVER_VERSION == 3
#include "server_index_v3.h"
#endif
#endif

namespace esphome {
namespace web_server {

static const char *const TAG = "web_server";

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
static const char *const HEADER_PNA_NAME = "Private-Network-Access-Name";
static const char *const HEADER_PNA_ID = "Private-Network-Access-ID";
static const char *const HEADER_CORS_REQ_PNA = "Access-Control-Request-Private-Network";
static const char *const HEADER_CORS_ALLOW_PNA = "Access-Control-Allow-Private-Network";
#endif

// Parse URL and return match info
static UrlMatch match_url(const char *url_ptr, size_t url_len, bool only_domain) {
  UrlMatch match{};

  // URL must start with '/'
  if (url_len < 2 || url_ptr[0] != '/') {
    return match;
  }

  // Skip leading '/'
  const char *start = url_ptr + 1;
  const char *end = url_ptr + url_len;

  // Find domain (everything up to next '/' or end)
  const char *domain_end = (const char *) memchr(start, '/', end - start);
  if (!domain_end) {
    // No second slash found - original behavior returns invalid
    return match;
  }

  // Set domain
  match.domain = start;
  match.domain_len = domain_end - start;
  match.valid = true;

  if (only_domain) {
    return match;
  }

  // Parse ID if present
  if (domain_end + 1 >= end) {
    return match;  // Nothing after domain slash
  }

  const char *id_start = domain_end + 1;
  const char *id_end = (const char *) memchr(id_start, '/', end - id_start);

  if (!id_end) {
    // No more slashes, entire remaining string is ID
    match.id = id_start;
    match.id_len = end - id_start;
    return match;
  }

  // Set ID
  match.id = id_start;
  match.id_len = id_end - id_start;

  // Parse method if present
  if (id_end + 1 < end) {
    match.method = id_end + 1;
    match.method_len = end - (id_end + 1);
  }

  return match;
}

#if !defined(USE_ESP32) && defined(USE_ARDUINO)
// helper for allowing only unique entries in the queue
void DeferredUpdateEventSource::deq_push_back_with_dedup_(void *source, message_generator_t *message_generator) {
  DeferredEvent item(source, message_generator);

  // Use range-based for loop instead of std::find_if to reduce template instantiation overhead and binary size
  for (auto &event : this->deferred_queue_) {
    if (event == item) {
      event = item;
      return;
    }
  }
  this->deferred_queue_.push_back(item);
}

void DeferredUpdateEventSource::process_deferred_queue_() {
  while (!deferred_queue_.empty()) {
    DeferredEvent &de = deferred_queue_.front();
    std::string message = de.message_generator_(web_server_, de.source_);
    if (this->send(message.c_str(), "state") != DISCARDED) {
      // O(n) but memory efficiency is more important than speed here which is why std::vector was chosen
      deferred_queue_.erase(deferred_queue_.begin());
      this->consecutive_send_failures_ = 0;  // Reset failure count on successful send
    } else {
      // NOTE: Similar logic exists in web_server_idf/web_server_idf.cpp in AsyncEventSourceResponse::process_buffer_()
      // The implementations differ due to platform-specific APIs (DISCARDED vs HTTPD_SOCK_ERR_TIMEOUT, close() vs
      // fd_.store(0)), but the failure counting and timeout logic should be kept in sync. If you change this logic,
      // also update the ESP-IDF implementation.
      this->consecutive_send_failures_++;
      if (this->consecutive_send_failures_ >= MAX_CONSECUTIVE_SEND_FAILURES) {
        // Too many failures, connection is likely dead
        ESP_LOGW(TAG, "Closing stuck EventSource connection after %" PRIu16 " failed sends",
                 this->consecutive_send_failures_);
        this->close();
        this->deferred_queue_.clear();
      }
      break;
    }
  }
}

void DeferredUpdateEventSource::loop() {
  process_deferred_queue_();
  if (!this->entities_iterator_.completed())
    this->entities_iterator_.advance();
}

void DeferredUpdateEventSource::deferrable_send_state(void *source, const char *event_type,
                                                      message_generator_t *message_generator) {
  // Skip if no connected clients to avoid unnecessary deferred queue processing
  if (this->count() == 0)
    return;

  // allow all json "details_all" to go through before publishing bare state events, this avoids unnamed entries showing
  // up in the web GUI and reduces event load during initial connect
  if (!entities_iterator_.completed() && 0 != strcmp(event_type, "state_detail_all"))
    return;

  if (source == nullptr)
    return;
  if (event_type == nullptr)
    return;
  if (message_generator == nullptr)
    return;

  if (0 != strcmp(event_type, "state_detail_all") && 0 != strcmp(event_type, "state")) {
    ESP_LOGE(TAG, "Can't defer non-state event");
  }

  if (!deferred_queue_.empty())
    process_deferred_queue_();
  if (!deferred_queue_.empty()) {
    // deferred queue still not empty which means downstream event queue full, no point trying to send first
    deq_push_back_with_dedup_(source, message_generator);
  } else {
    std::string message = message_generator(web_server_, source);
    if (this->send(message.c_str(), "state") == DISCARDED) {
      deq_push_back_with_dedup_(source, message_generator);
    } else {
      this->consecutive_send_failures_ = 0;  // Reset failure count on successful send
    }
  }
}

// used for logs plus the initial ping/config
void DeferredUpdateEventSource::try_send_nodefer(const char *message, const char *event, uint32_t id,
                                                 uint32_t reconnect) {
  this->send(message, event, id, reconnect);
}

void DeferredUpdateEventSourceList::loop() {
  for (DeferredUpdateEventSource *dues : *this) {
    dues->loop();
  }
}

void DeferredUpdateEventSourceList::deferrable_send_state(void *source, const char *event_type,
                                                          message_generator_t *message_generator) {
  // Skip if no event sources (no connected clients) to avoid unnecessary iteration
  if (this->empty())
    return;
  for (DeferredUpdateEventSource *dues : *this) {
    dues->deferrable_send_state(source, event_type, message_generator);
  }
}

void DeferredUpdateEventSourceList::try_send_nodefer(const char *message, const char *event, uint32_t id,
                                                     uint32_t reconnect) {
  for (DeferredUpdateEventSource *dues : *this) {
    dues->try_send_nodefer(message, event, id, reconnect);
  }
}

void DeferredUpdateEventSourceList::add_new_client(WebServer *ws, AsyncWebServerRequest *request) {
  DeferredUpdateEventSource *es = new DeferredUpdateEventSource(ws, "/events");
  this->push_back(es);

  es->onConnect([this, ws, es](AsyncEventSourceClient *client) {
    ws->defer([this, ws, es]() { this->on_client_connect_(ws, es); });
  });

  es->onDisconnect([this, ws, es](AsyncEventSourceClient *client) {
    ws->defer([this, es]() { this->on_client_disconnect_((DeferredUpdateEventSource *) es); });
  });

  es->handleRequest(request);
}

void DeferredUpdateEventSourceList::on_client_connect_(WebServer *ws, DeferredUpdateEventSource *source) {
  // Configure reconnect timeout and send config
  // this should always go through since the AsyncEventSourceClient event queue is empty on connect
  std::string message = ws->get_config_json();
  source->try_send_nodefer(message.c_str(), "ping", millis(), 30000);

#ifdef USE_WEBSERVER_SORTING
  for (auto &group : ws->sorting_groups_) {
    json::JsonBuilder builder;
    JsonObject root = builder.root();
    root["name"] = group.second.name;
    root["sorting_weight"] = group.second.weight;
    message = builder.serialize();

    // up to 31 groups should be able to be queued initially without defer
    source->try_send_nodefer(message.c_str(), "sorting_group");
  }
#endif

  source->entities_iterator_.begin(ws->include_internal_);

  // just dump them all up-front and take advantage of the deferred queue
  //     on second thought that takes too long, but leaving the commented code here for debug purposes
  // while(!source->entities_iterator_.completed()) {
  //  source->entities_iterator_.advance();
  //}
}

void DeferredUpdateEventSourceList::on_client_disconnect_(DeferredUpdateEventSource *source) {
  // This method was called via WebServer->defer() and is no longer executing in the
  // context of the network callback. The object is now dead and can be safely deleted.
  this->remove(source);
  delete source;  // NOLINT
}
#endif

WebServer::WebServer(web_server_base::WebServerBase *base) : base_(base) {}

#ifdef USE_WEBSERVER_CSS_INCLUDE
void WebServer::set_css_include(const char *css_include) { this->css_include_ = css_include; }
#endif
#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::set_js_include(const char *js_include) { this->js_include_ = js_include; }
#endif

std::string WebServer::get_config_json() {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  root["title"] = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
  root["comment"] = App.get_comment();
#if defined(USE_WEBSERVER_OTA_DISABLED) || !defined(USE_WEBSERVER_OTA)
  root["ota"] = false;  // Note: USE_WEBSERVER_OTA_DISABLED only affects web_server, not captive_portal
#else
  root["ota"] = true;
#endif
  root["log"] = this->expose_log_;
  root["lang"] = "en";

  return builder.serialize();
}

void WebServer::setup() {
  this->setup_controller(this->include_internal_);
  this->base_->init();

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->expose_log_) {
    logger::global_logger->add_on_log_callback(
        // logs are not deferred, the memory overhead would be too large
        [this](int level, const char *tag, const char *message, size_t message_len) {
          (void) message_len;
          this->events_.try_send_nodefer(message, "log", millis());
        });
  }
#endif

#ifdef USE_ESP32
  this->base_->add_handler(&this->events_);
#endif
  this->base_->add_handler(this);

  // OTA is now handled by the web_server OTA platform

  // doesn't need defer functionality - if the queue is full, the client JS knows it's alive because it's clearly
  // getting a lot of events
  this->set_interval(10000, [this]() { this->events_.try_send_nodefer("", "ping", millis(), 30000); });
}
void WebServer::loop() { this->events_.loop(); }
void WebServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Web Server:\n"
                "  Address: %s:%u",
                network::get_use_address().c_str(), this->base_->get_port());
}
float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

#ifdef USE_WEBSERVER_LOCAL
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
#ifndef USE_ESP8266
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
#else
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
#endif
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
#elif USE_WEBSERVER_VERSION >= 2
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
#ifndef USE_ESP8266
  AsyncWebServerResponse *response =
      request->beginResponse(200, "text/html", ESPHOME_WEBSERVER_INDEX_HTML, ESPHOME_WEBSERVER_INDEX_HTML_SIZE);
#else
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/html", ESPHOME_WEBSERVER_INDEX_HTML, ESPHOME_WEBSERVER_INDEX_HTML_SIZE);
#endif
  // No gzip header here because the HTML file is so small
  request->send(response);
}
#endif

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
void WebServer::handle_pna_cors_request(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "");
  response->addHeader(HEADER_CORS_ALLOW_PNA, "true");
  response->addHeader(HEADER_PNA_NAME, App.get_name().c_str());
  std::string mac = get_mac_address_pretty();
  response->addHeader(HEADER_PNA_ID, mac.c_str());
  request->send(response);
}
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
void WebServer::handle_css_request(AsyncWebServerRequest *request) {
#ifndef USE_ESP8266
  AsyncWebServerResponse *response =
      request->beginResponse(200, "text/css", ESPHOME_WEBSERVER_CSS_INCLUDE, ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE);
#else
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/css", ESPHOME_WEBSERVER_CSS_INCLUDE, ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE);
#endif
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::handle_js_request(AsyncWebServerRequest *request) {
#ifndef USE_ESP8266
  AsyncWebServerResponse *response =
      request->beginResponse(200, "text/javascript", ESPHOME_WEBSERVER_JS_INCLUDE, ESPHOME_WEBSERVER_JS_INCLUDE_SIZE);
#else
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/javascript", ESPHOME_WEBSERVER_JS_INCLUDE, ESPHOME_WEBSERVER_JS_INCLUDE_SIZE);
#endif
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
#endif

// Helper functions to reduce code size by avoiding macro expansion
static void set_json_id(JsonObject &root, EntityBase *obj, const char *prefix, JsonDetail start_config) {
  char id_buf[160];  // object_id can be up to 128 chars + prefix + dash + null
  const auto &object_id = obj->get_object_id();
  snprintf(id_buf, sizeof(id_buf), "%s-%s", prefix, object_id.c_str());
  root["id"] = id_buf;
  if (start_config == DETAIL_ALL) {
    root["name"] = obj->get_name();
    root["icon"] = obj->get_icon_ref();
    root["entity_category"] = obj->get_entity_category();
    bool is_disabled = obj->is_disabled_by_default();
    if (is_disabled)
      root["is_disabled_by_default"] = is_disabled;
  }
}

// Keep as separate function even though only used once: reduces code size by ~48 bytes
// by allowing compiler to share code between template instantiations (bool, float, etc.)
template<typename T>
static void set_json_value(JsonObject &root, EntityBase *obj, const char *prefix, const T &value,
                           JsonDetail start_config) {
  set_json_id(root, obj, prefix, start_config);
  root["value"] = value;
}

template<typename T>
static void set_json_icon_state_value(JsonObject &root, EntityBase *obj, const char *prefix, const std::string &state,
                                      const T &value, JsonDetail start_config) {
  set_json_value(root, obj, prefix, value, start_config);
  root["state"] = state;
}

// Helper to get request detail parameter
static JsonDetail get_request_detail(AsyncWebServerRequest *request) {
  auto *param = request->getParam("detail");
  return (param && param->value() == "all") ? DETAIL_ALL : DETAIL_STATE;
}

#ifdef USE_SENSOR
void WebServer::on_sensor_update(sensor::Sensor *obj, float state) {
  this->events_.deferrable_send_state(obj, "state", sensor_state_json_generator);
}
void WebServer::handle_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (sensor::Sensor *obj : App.get_sensors()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->sensor_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
std::string WebServer::sensor_state_json_generator(WebServer *web_server, void *source) {
  return web_server->sensor_json((sensor::Sensor *) (source), ((sensor::Sensor *) (source))->state, DETAIL_STATE);
}
std::string WebServer::sensor_all_json_generator(WebServer *web_server, void *source) {
  return web_server->sensor_json((sensor::Sensor *) (source), ((sensor::Sensor *) (source))->state, DETAIL_ALL);
}
std::string WebServer::sensor_json(sensor::Sensor *obj, float value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  const auto uom_ref = obj->get_unit_of_measurement_ref();

  // Build JSON directly inline
  std::string state;
  if (std::isnan(value)) {
    state = "NA";
  } else {
    state = value_accuracy_with_uom_to_string(value, obj->get_accuracy_decimals(), uom_ref);
  }
  set_json_icon_state_value(root, obj, "sensor", state, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    if (!uom_ref.empty())
      root["uom"] = uom_ref;
  }

  return builder.serialize();
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
  this->events_.deferrable_send_state(obj, "state", text_sensor_state_json_generator);
}
void WebServer::handle_text_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (text_sensor::TextSensor *obj : App.get_text_sensors()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->text_sensor_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
std::string WebServer::text_sensor_state_json_generator(WebServer *web_server, void *source) {
  return web_server->text_sensor_json((text_sensor::TextSensor *) (source),
                                      ((text_sensor::TextSensor *) (source))->state, DETAIL_STATE);
}
std::string WebServer::text_sensor_all_json_generator(WebServer *web_server, void *source) {
  return web_server->text_sensor_json((text_sensor::TextSensor *) (source),
                                      ((text_sensor::TextSensor *) (source))->state, DETAIL_ALL);
}
std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value,
                                        JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "text_sensor", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_SWITCH
void WebServer::on_switch_update(switch_::Switch *obj, bool state) {
  this->events_.deferrable_send_state(obj, "state", switch_state_json_generator);
}
void WebServer::handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (switch_::Switch *obj : App.get_switches()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->switch_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    // Handle action methods with single defer and response
    enum SwitchAction { NONE, TOGGLE, TURN_ON, TURN_OFF };
    SwitchAction action = NONE;

    if (match.method_equals("toggle")) {
      action = TOGGLE;
    } else if (match.method_equals("turn_on")) {
      action = TURN_ON;
    } else if (match.method_equals("turn_off")) {
      action = TURN_OFF;
    }

    if (action != NONE) {
      this->defer([obj, action]() {
        switch (action) {
          case TOGGLE:
            obj->toggle();
            break;
          case TURN_ON:
            obj->turn_on();
            break;
          case TURN_OFF:
            obj->turn_off();
            break;
          default:
            break;
        }
      });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::switch_state_json_generator(WebServer *web_server, void *source) {
  return web_server->switch_json((switch_::Switch *) (source), ((switch_::Switch *) (source))->state, DETAIL_STATE);
}
std::string WebServer::switch_all_json_generator(WebServer *web_server, void *source) {
  return web_server->switch_json((switch_::Switch *) (source), ((switch_::Switch *) (source))->state, DETAIL_ALL);
}
std::string WebServer::switch_json(switch_::Switch *obj, bool value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "switch", value ? "ON" : "OFF", value, start_config);
  if (start_config == DETAIL_ALL) {
    root["assumed_state"] = obj->assumed_state();
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_BUTTON
void WebServer::handle_button_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (button::Button *obj : App.get_buttons()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->button_json(obj, detail);
      request->send(200, "application/json", data.c_str());
    } else if (match.method_equals("press")) {
      this->defer([obj]() { obj->press(); });
      request->send(200);
      return;
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::button_state_json_generator(WebServer *web_server, void *source) {
  return web_server->button_json((button::Button *) (source), DETAIL_STATE);
}
std::string WebServer::button_all_json_generator(WebServer *web_server, void *source) {
  return web_server->button_json((button::Button *) (source), DETAIL_ALL);
}
std::string WebServer::button_json(button::Button *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "button", start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_BINARY_SENSOR
void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj) {
  this->events_.deferrable_send_state(obj, "state", binary_sensor_state_json_generator);
}
void WebServer::handle_binary_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (binary_sensor::BinarySensor *obj : App.get_binary_sensors()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->binary_sensor_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
std::string WebServer::binary_sensor_state_json_generator(WebServer *web_server, void *source) {
  return web_server->binary_sensor_json((binary_sensor::BinarySensor *) (source),
                                        ((binary_sensor::BinarySensor *) (source))->state, DETAIL_STATE);
}
std::string WebServer::binary_sensor_all_json_generator(WebServer *web_server, void *source) {
  return web_server->binary_sensor_json((binary_sensor::BinarySensor *) (source),
                                        ((binary_sensor::BinarySensor *) (source))->state, DETAIL_ALL);
}
std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "binary_sensor", value ? "ON" : "OFF", value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_FAN
void WebServer::on_fan_update(fan::Fan *obj) {
  this->events_.deferrable_send_state(obj, "state", fan_state_json_generator);
}
void WebServer::handle_fan_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (fan::Fan *obj : App.get_fans()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->fan_json(obj, detail);
      request->send(200, "application/json", data.c_str());
    } else if (match.method_equals("toggle")) {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method_equals("turn_on") || match.method_equals("turn_off")) {
      auto call = match.method_equals("turn_on") ? obj->turn_on() : obj->turn_off();

      parse_int_param_(request, "speed_level", call, &decltype(call)::set_speed);

      if (request->hasParam("oscillation")) {
        auto speed = request->getParam("oscillation")->value();
        auto val = parse_on_off(speed.c_str());
        switch (val) {
          case PARSE_ON:
            call.set_oscillating(true);
            break;
          case PARSE_OFF:
            call.set_oscillating(false);
            break;
          case PARSE_TOGGLE:
            call.set_oscillating(!obj->oscillating);
            break;
          case PARSE_NONE:
            request->send(404);
            return;
        }
      }
      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::fan_state_json_generator(WebServer *web_server, void *source) {
  return web_server->fan_json((fan::Fan *) (source), DETAIL_STATE);
}
std::string WebServer::fan_all_json_generator(WebServer *web_server, void *source) {
  return web_server->fan_json((fan::Fan *) (source), DETAIL_ALL);
}
std::string WebServer::fan_json(fan::Fan *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "fan", obj->state ? "ON" : "OFF", obj->state, start_config);
  const auto traits = obj->get_traits();
  if (traits.supports_speed()) {
    root["speed_level"] = obj->speed;
    root["speed_count"] = traits.supported_speed_count();
  }
  if (obj->get_traits().supports_oscillation())
    root["oscillation"] = obj->oscillating;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_LIGHT
void WebServer::on_light_update(light::LightState *obj) {
  this->events_.deferrable_send_state(obj, "state", light_state_json_generator);
}
void WebServer::handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (light::LightState *obj : App.get_lights()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->light_json(obj, detail);
      request->send(200, "application/json", data.c_str());
    } else if (match.method_equals("toggle")) {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method_equals("turn_on")) {
      auto call = obj->turn_on();

      // Parse color parameters
      parse_light_param_(request, "brightness", call, &decltype(call)::set_brightness, 255.0f);
      parse_light_param_(request, "r", call, &decltype(call)::set_red, 255.0f);
      parse_light_param_(request, "g", call, &decltype(call)::set_green, 255.0f);
      parse_light_param_(request, "b", call, &decltype(call)::set_blue, 255.0f);
      parse_light_param_(request, "white_value", call, &decltype(call)::set_white, 255.0f);
      parse_light_param_(request, "color_temp", call, &decltype(call)::set_color_temperature);

      // Parse timing parameters
      parse_light_param_uint_(request, "flash", call, &decltype(call)::set_flash_length, 1000);
      parse_light_param_uint_(request, "transition", call, &decltype(call)::set_transition_length, 1000);

      parse_string_param_(request, "effect", call, &decltype(call)::set_effect);

      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    } else if (match.method_equals("turn_off")) {
      auto call = obj->turn_off();
      parse_light_param_uint_(request, "transition", call, &decltype(call)::set_transition_length, 1000);
      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::light_state_json_generator(WebServer *web_server, void *source) {
  return web_server->light_json((light::LightState *) (source), DETAIL_STATE);
}
std::string WebServer::light_all_json_generator(WebServer *web_server, void *source) {
  return web_server->light_json((light::LightState *) (source), DETAIL_ALL);
}
std::string WebServer::light_json(light::LightState *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "light", start_config);
  root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

  light::LightJSONSchema::dump_json(*obj, root);
  if (start_config == DETAIL_ALL) {
    JsonArray opt = root["effects"].to<JsonArray>();
    opt.add("None");
    for (auto const &option : obj->get_effects()) {
      opt.add(option->get_name());
    }
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_COVER
void WebServer::on_cover_update(cover::Cover *obj) {
  this->events_.deferrable_send_state(obj, "state", cover_state_json_generator);
}
void WebServer::handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (cover::Cover *obj : App.get_covers()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->cover_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    auto call = obj->make_call();

    // Lookup table for cover methods
    static const struct {
      const char *name;
      cover::CoverCall &(cover::CoverCall::*action)();
    } METHODS[] = {
        {"open", &cover::CoverCall::set_command_open},
        {"close", &cover::CoverCall::set_command_close},
        {"stop", &cover::CoverCall::set_command_stop},
        {"toggle", &cover::CoverCall::set_command_toggle},
    };

    bool found = false;
    for (const auto &method : METHODS) {
      if (match.method_equals(method.name)) {
        (call.*method.action)();
        found = true;
        break;
      }
    }

    if (!found && !match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto traits = obj->get_traits();
    if ((request->hasParam("position") && !traits.get_supports_position()) ||
        (request->hasParam("tilt") && !traits.get_supports_tilt())) {
      request->send(409);
      return;
    }

    parse_float_param_(request, "position", call, &decltype(call)::set_position);
    parse_float_param_(request, "tilt", call, &decltype(call)::set_tilt);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::cover_state_json_generator(WebServer *web_server, void *source) {
  return web_server->cover_json((cover::Cover *) (source), DETAIL_STATE);
}
std::string WebServer::cover_all_json_generator(WebServer *web_server, void *source) {
  return web_server->cover_json((cover::Cover *) (source), DETAIL_ALL);
}
std::string WebServer::cover_json(cover::Cover *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "cover", obj->is_fully_closed() ? "CLOSED" : "OPEN", obj->position,
                            start_config);
  root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

  if (obj->get_traits().get_supports_position())
    root["position"] = obj->position;
  if (obj->get_traits().get_supports_tilt())
    root["tilt"] = obj->tilt;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_NUMBER
void WebServer::on_number_update(number::Number *obj, float state) {
  this->events_.deferrable_send_state(obj, "state", number_state_json_generator);
}
void WebServer::handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_numbers()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->number_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();
    parse_float_param_(request, "value", call, &decltype(call)::set_value);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::number_state_json_generator(WebServer *web_server, void *source) {
  return web_server->number_json((number::Number *) (source), ((number::Number *) (source))->state, DETAIL_STATE);
}
std::string WebServer::number_all_json_generator(WebServer *web_server, void *source) {
  return web_server->number_json((number::Number *) (source), ((number::Number *) (source))->state, DETAIL_ALL);
}
std::string WebServer::number_json(number::Number *obj, float value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  const auto uom_ref = obj->traits.get_unit_of_measurement_ref();

  set_json_id(root, obj, "number", start_config);
  if (start_config == DETAIL_ALL) {
    root["min_value"] =
        value_accuracy_to_string(obj->traits.get_min_value(), step_to_accuracy_decimals(obj->traits.get_step()));
    root["max_value"] =
        value_accuracy_to_string(obj->traits.get_max_value(), step_to_accuracy_decimals(obj->traits.get_step()));
    root["step"] = value_accuracy_to_string(obj->traits.get_step(), step_to_accuracy_decimals(obj->traits.get_step()));
    root["mode"] = (int) obj->traits.get_mode();
    if (!uom_ref.empty())
      root["uom"] = uom_ref;
    this->add_sorting_info_(root, obj);
  }
  if (std::isnan(value)) {
    root["value"] = "\"NaN\"";
    root["state"] = "NA";
  } else {
    root["value"] = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
    root["state"] =
        value_accuracy_with_uom_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()), uom_ref);
  }

  return builder.serialize();
}
#endif

#ifdef USE_DATETIME_DATE
void WebServer::on_date_update(datetime::DateEntity *obj) {
  this->events_.deferrable_send_state(obj, "state", date_state_json_generator);
}
void WebServer::handle_date_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_dates()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->date_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (!request->hasParam("value")) {
      request->send(409);
      return;
    }

    parse_string_param_(request, "value", call, &decltype(call)::set_date);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::date_state_json_generator(WebServer *web_server, void *source) {
  return web_server->date_json((datetime::DateEntity *) (source), DETAIL_STATE);
}
std::string WebServer::date_all_json_generator(WebServer *web_server, void *source) {
  return web_server->date_json((datetime::DateEntity *) (source), DETAIL_ALL);
}
std::string WebServer::date_json(datetime::DateEntity *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "date", start_config);
  std::string value = str_sprintf("%d-%02d-%02d", obj->year, obj->month, obj->day);
  root["value"] = value;
  root["state"] = value;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif  // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
void WebServer::on_time_update(datetime::TimeEntity *obj) {
  this->events_.deferrable_send_state(obj, "state", time_state_json_generator);
}
void WebServer::handle_time_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_times()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->time_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (!request->hasParam("value")) {
      request->send(409);
      return;
    }

    parse_string_param_(request, "value", call, &decltype(call)::set_time);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::time_state_json_generator(WebServer *web_server, void *source) {
  return web_server->time_json((datetime::TimeEntity *) (source), DETAIL_STATE);
}
std::string WebServer::time_all_json_generator(WebServer *web_server, void *source) {
  return web_server->time_json((datetime::TimeEntity *) (source), DETAIL_ALL);
}
std::string WebServer::time_json(datetime::TimeEntity *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "time", start_config);
  std::string value = str_sprintf("%02d:%02d:%02d", obj->hour, obj->minute, obj->second);
  root["value"] = value;
  root["state"] = value;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif  // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
void WebServer::on_datetime_update(datetime::DateTimeEntity *obj) {
  this->events_.deferrable_send_state(obj, "state", datetime_state_json_generator);
}
void WebServer::handle_datetime_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_datetimes()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;
    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->datetime_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (!request->hasParam("value")) {
      request->send(409);
      return;
    }

    parse_string_param_(request, "value", call, &decltype(call)::set_datetime);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::datetime_state_json_generator(WebServer *web_server, void *source) {
  return web_server->datetime_json((datetime::DateTimeEntity *) (source), DETAIL_STATE);
}
std::string WebServer::datetime_all_json_generator(WebServer *web_server, void *source) {
  return web_server->datetime_json((datetime::DateTimeEntity *) (source), DETAIL_ALL);
}
std::string WebServer::datetime_json(datetime::DateTimeEntity *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "datetime", start_config);
  std::string value =
      str_sprintf("%d-%02d-%02d %02d:%02d:%02d", obj->year, obj->month, obj->day, obj->hour, obj->minute, obj->second);
  root["value"] = value;
  root["state"] = value;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif  // USE_DATETIME_DATETIME

#ifdef USE_TEXT
void WebServer::on_text_update(text::Text *obj, const std::string &state) {
  this->events_.deferrable_send_state(obj, "state", text_state_json_generator);
}
void WebServer::handle_text_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_texts()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->text_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();
    parse_string_param_(request, "value", call, &decltype(call)::set_value);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::text_state_json_generator(WebServer *web_server, void *source) {
  return web_server->text_json((text::Text *) (source), ((text::Text *) (source))->state, DETAIL_STATE);
}
std::string WebServer::text_all_json_generator(WebServer *web_server, void *source) {
  return web_server->text_json((text::Text *) (source), ((text::Text *) (source))->state, DETAIL_ALL);
}
std::string WebServer::text_json(text::Text *obj, const std::string &value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "text", start_config);
  root["min_length"] = obj->traits.get_min_length();
  root["max_length"] = obj->traits.get_max_length();
  root["pattern"] = obj->traits.get_pattern();
  if (obj->traits.get_mode() == text::TextMode::TEXT_MODE_PASSWORD) {
    root["state"] = "********";
  } else {
    root["state"] = value;
  }
  root["value"] = value;
  if (start_config == DETAIL_ALL) {
    root["mode"] = (int) obj->traits.get_mode();
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_SELECT
void WebServer::on_select_update(select::Select *obj, const std::string &state, size_t index) {
  this->events_.deferrable_send_state(obj, "state", select_state_json_generator);
}
void WebServer::handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_selects()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->select_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();
    parse_string_param_(request, "option", call, &decltype(call)::set_option);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::select_state_json_generator(WebServer *web_server, void *source) {
  return web_server->select_json((select::Select *) (source), ((select::Select *) (source))->state, DETAIL_STATE);
}
std::string WebServer::select_all_json_generator(WebServer *web_server, void *source) {
  return web_server->select_json((select::Select *) (source), ((select::Select *) (source))->state, DETAIL_ALL);
}
std::string WebServer::select_json(select::Select *obj, const std::string &value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "select", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    JsonArray opt = root["option"].to<JsonArray>();
    for (auto &option : obj->traits.get_options()) {
      opt.add(option);
    }
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

// Longest: HORIZONTAL
#define PSTR_LOCAL(mode_s) ESPHOME_strncpy_P(buf, (ESPHOME_PGM_P) ((mode_s)), 15)

#ifdef USE_CLIMATE
void WebServer::on_climate_update(climate::Climate *obj) {
  this->events_.deferrable_send_state(obj, "state", climate_state_json_generator);
}
void WebServer::handle_climate_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_climates()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->climate_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    if (!match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    // Parse string mode parameters
    parse_string_param_(request, "mode", call, &decltype(call)::set_mode);
    parse_string_param_(request, "fan_mode", call, &decltype(call)::set_fan_mode);
    parse_string_param_(request, "swing_mode", call, &decltype(call)::set_swing_mode);

    // Parse temperature parameters
    parse_float_param_(request, "target_temperature_high", call, &decltype(call)::set_target_temperature_high);
    parse_float_param_(request, "target_temperature_low", call, &decltype(call)::set_target_temperature_low);
    parse_float_param_(request, "target_temperature", call, &decltype(call)::set_target_temperature);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::climate_state_json_generator(WebServer *web_server, void *source) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return web_server->climate_json((climate::Climate *) (source), DETAIL_STATE);
}
std::string WebServer::climate_all_json_generator(WebServer *web_server, void *source) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return web_server->climate_json((climate::Climate *) (source), DETAIL_ALL);
}
std::string WebServer::climate_json(climate::Climate *obj, JsonDetail start_config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  json::JsonBuilder builder;
  JsonObject root = builder.root();
  set_json_id(root, obj, "climate", start_config);
  const auto traits = obj->get_traits();
  int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
  int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
  char buf[16];

  if (start_config == DETAIL_ALL) {
    JsonArray opt = root["modes"].to<JsonArray>();
    for (climate::ClimateMode m : traits.get_supported_modes())
      opt.add(PSTR_LOCAL(climate::climate_mode_to_string(m)));
    if (!traits.get_supported_custom_fan_modes().empty()) {
      JsonArray opt = root["fan_modes"].to<JsonArray>();
      for (climate::ClimateFanMode m : traits.get_supported_fan_modes())
        opt.add(PSTR_LOCAL(climate::climate_fan_mode_to_string(m)));
    }

    if (!traits.get_supported_custom_fan_modes().empty()) {
      JsonArray opt = root["custom_fan_modes"].to<JsonArray>();
      for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
        opt.add(custom_fan_mode);
    }
    if (traits.get_supports_swing_modes()) {
      JsonArray opt = root["swing_modes"].to<JsonArray>();
      for (auto swing_mode : traits.get_supported_swing_modes())
        opt.add(PSTR_LOCAL(climate::climate_swing_mode_to_string(swing_mode)));
    }
    if (traits.get_supports_presets() && obj->preset.has_value()) {
      JsonArray opt = root["presets"].to<JsonArray>();
      for (climate::ClimatePreset m : traits.get_supported_presets())
        opt.add(PSTR_LOCAL(climate::climate_preset_to_string(m)));
    }
    if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
      JsonArray opt = root["custom_presets"].to<JsonArray>();
      for (auto const &custom_preset : traits.get_supported_custom_presets())
        opt.add(custom_preset);
    }
    this->add_sorting_info_(root, obj);
  }

  bool has_state = false;
  root["mode"] = PSTR_LOCAL(climate_mode_to_string(obj->mode));
  root["max_temp"] = value_accuracy_to_string(traits.get_visual_max_temperature(), target_accuracy);
  root["min_temp"] = value_accuracy_to_string(traits.get_visual_min_temperature(), target_accuracy);
  root["step"] = traits.get_visual_target_temperature_step();
  if (traits.get_supports_action()) {
    root["action"] = PSTR_LOCAL(climate_action_to_string(obj->action));
    root["state"] = root["action"];
    has_state = true;
  }
  if (traits.get_supports_fan_modes() && obj->fan_mode.has_value()) {
    root["fan_mode"] = PSTR_LOCAL(climate_fan_mode_to_string(obj->fan_mode.value()));
  }
  if (!traits.get_supported_custom_fan_modes().empty() && obj->custom_fan_mode.has_value()) {
    root["custom_fan_mode"] = obj->custom_fan_mode.value().c_str();
  }
  if (traits.get_supports_presets() && obj->preset.has_value()) {
    root["preset"] = PSTR_LOCAL(climate_preset_to_string(obj->preset.value()));
  }
  if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
    root["custom_preset"] = obj->custom_preset.value().c_str();
  }
  if (traits.get_supports_swing_modes()) {
    root["swing_mode"] = PSTR_LOCAL(climate_swing_mode_to_string(obj->swing_mode));
  }
  if (traits.get_supports_current_temperature()) {
    if (!std::isnan(obj->current_temperature)) {
      root["current_temperature"] = value_accuracy_to_string(obj->current_temperature, current_accuracy);
    } else {
      root["current_temperature"] = "NA";
    }
  }
  if (traits.get_supports_two_point_target_temperature()) {
    root["target_temperature_low"] = value_accuracy_to_string(obj->target_temperature_low, target_accuracy);
    root["target_temperature_high"] = value_accuracy_to_string(obj->target_temperature_high, target_accuracy);
    if (!has_state) {
      root["state"] = value_accuracy_to_string((obj->target_temperature_high + obj->target_temperature_low) / 2.0f,
                                               target_accuracy);
    }
  } else {
    root["target_temperature"] = value_accuracy_to_string(obj->target_temperature, target_accuracy);
    if (!has_state)
      root["state"] = root["target_temperature"];
  }

  return builder.serialize();
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
#endif

#ifdef USE_LOCK
void WebServer::on_lock_update(lock::Lock *obj) {
  this->events_.deferrable_send_state(obj, "state", lock_state_json_generator);
}
void WebServer::handle_lock_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (lock::Lock *obj : App.get_locks()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->lock_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    // Handle action methods with single defer and response
    enum LockAction { NONE, LOCK, UNLOCK, OPEN };
    LockAction action = NONE;

    if (match.method_equals("lock")) {
      action = LOCK;
    } else if (match.method_equals("unlock")) {
      action = UNLOCK;
    } else if (match.method_equals("open")) {
      action = OPEN;
    }

    if (action != NONE) {
      this->defer([obj, action]() {
        switch (action) {
          case LOCK:
            obj->lock();
            break;
          case UNLOCK:
            obj->unlock();
            break;
          case OPEN:
            obj->open();
            break;
          default:
            break;
        }
      });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::lock_state_json_generator(WebServer *web_server, void *source) {
  return web_server->lock_json((lock::Lock *) (source), ((lock::Lock *) (source))->state, DETAIL_STATE);
}
std::string WebServer::lock_all_json_generator(WebServer *web_server, void *source) {
  return web_server->lock_json((lock::Lock *) (source), ((lock::Lock *) (source))->state, DETAIL_ALL);
}
std::string WebServer::lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "lock", lock::lock_state_to_string(value), value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_VALVE
void WebServer::on_valve_update(valve::Valve *obj) {
  this->events_.deferrable_send_state(obj, "state", valve_state_json_generator);
}
void WebServer::handle_valve_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (valve::Valve *obj : App.get_valves()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->valve_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    auto call = obj->make_call();

    // Lookup table for valve methods
    static const struct {
      const char *name;
      valve::ValveCall &(valve::ValveCall::*action)();
    } METHODS[] = {
        {"open", &valve::ValveCall::set_command_open},
        {"close", &valve::ValveCall::set_command_close},
        {"stop", &valve::ValveCall::set_command_stop},
        {"toggle", &valve::ValveCall::set_command_toggle},
    };

    bool found = false;
    for (const auto &method : METHODS) {
      if (match.method_equals(method.name)) {
        (call.*method.action)();
        found = true;
        break;
      }
    }

    if (!found && !match.method_equals("set")) {
      request->send(404);
      return;
    }

    auto traits = obj->get_traits();
    if (request->hasParam("position") && !traits.get_supports_position()) {
      request->send(409);
      return;
    }

    parse_float_param_(request, "position", call, &decltype(call)::set_position);

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::valve_state_json_generator(WebServer *web_server, void *source) {
  return web_server->valve_json((valve::Valve *) (source), DETAIL_STATE);
}
std::string WebServer::valve_all_json_generator(WebServer *web_server, void *source) {
  return web_server->valve_json((valve::Valve *) (source), DETAIL_ALL);
}
std::string WebServer::valve_json(valve::Valve *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "valve", obj->is_fully_closed() ? "CLOSED" : "OPEN", obj->position,
                            start_config);
  root["current_operation"] = valve::valve_operation_to_str(obj->current_operation);

  if (obj->get_traits().get_supports_position())
    root["position"] = obj->position;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
void WebServer::on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) {
  this->events_.deferrable_send_state(obj, "state", alarm_control_panel_state_json_generator);
}
void WebServer::handle_alarm_control_panel_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (alarm_control_panel::AlarmControlPanel *obj : App.get_alarm_control_panels()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->alarm_control_panel_json(obj, obj->get_state(), detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    auto call = obj->make_call();
    parse_string_param_(request, "code", call, &decltype(call)::set_code);

    // Lookup table for alarm control panel methods
    static const struct {
      const char *name;
      alarm_control_panel::AlarmControlPanelCall &(alarm_control_panel::AlarmControlPanelCall::*action)();
    } METHODS[] = {
        {"disarm", &alarm_control_panel::AlarmControlPanelCall::disarm},
        {"arm_away", &alarm_control_panel::AlarmControlPanelCall::arm_away},
        {"arm_home", &alarm_control_panel::AlarmControlPanelCall::arm_home},
        {"arm_night", &alarm_control_panel::AlarmControlPanelCall::arm_night},
        {"arm_vacation", &alarm_control_panel::AlarmControlPanelCall::arm_vacation},
    };

    bool found = false;
    for (const auto &method : METHODS) {
      if (match.method_equals(method.name)) {
        (call.*method.action)();
        found = true;
        break;
      }
    }

    if (!found) {
      request->send(404);
      return;
    }

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::alarm_control_panel_state_json_generator(WebServer *web_server, void *source) {
  return web_server->alarm_control_panel_json((alarm_control_panel::AlarmControlPanel *) (source),
                                              ((alarm_control_panel::AlarmControlPanel *) (source))->get_state(),
                                              DETAIL_STATE);
}
std::string WebServer::alarm_control_panel_all_json_generator(WebServer *web_server, void *source) {
  return web_server->alarm_control_panel_json((alarm_control_panel::AlarmControlPanel *) (source),
                                              ((alarm_control_panel::AlarmControlPanel *) (source))->get_state(),
                                              DETAIL_ALL);
}
std::string WebServer::alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                                alarm_control_panel::AlarmControlPanelState value,
                                                JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  char buf[16];
  set_json_icon_state_value(root, obj, "alarm-control-panel", PSTR_LOCAL(alarm_control_panel_state_to_string(value)),
                            value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_EVENT
void WebServer::on_event(event::Event *obj, const std::string &event_type) {
  this->events_.deferrable_send_state(obj, "state", event_state_json_generator);
}

void WebServer::handle_event_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (event::Event *obj : App.get_events()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->event_json(obj, "", detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}

static std::string get_event_type(event::Event *event) {
  return (event && event->last_event_type) ? *event->last_event_type : "";
}

std::string WebServer::event_state_json_generator(WebServer *web_server, void *source) {
  auto *event = static_cast<event::Event *>(source);
  return web_server->event_json(event, get_event_type(event), DETAIL_STATE);
}
std::string WebServer::event_all_json_generator(WebServer *web_server, void *source) {
  auto *event = static_cast<event::Event *>(source);
  return web_server->event_json(event, get_event_type(event), DETAIL_ALL);
}
std::string WebServer::event_json(event::Event *obj, const std::string &event_type, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "event", start_config);
  if (!event_type.empty()) {
    root["event_type"] = event_type;
  }
  if (start_config == DETAIL_ALL) {
    JsonArray event_types = root["event_types"].to<JsonArray>();
    for (auto const &event_type : obj->get_event_types()) {
      event_types.add(event_type);
    }
    root["device_class"] = obj->get_device_class_ref();
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_UPDATE
static const char *update_state_to_string(update::UpdateState state) {
  switch (state) {
    case update::UPDATE_STATE_NO_UPDATE:
      return "NO UPDATE";
    case update::UPDATE_STATE_AVAILABLE:
      return "UPDATE AVAILABLE";
    case update::UPDATE_STATE_INSTALLING:
      return "INSTALLING";
    default:
      return "UNKNOWN";
  }
}

void WebServer::on_update(update::UpdateEntity *obj) {
  this->events_.deferrable_send_state(obj, "state", update_state_json_generator);
}
void WebServer::handle_update_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (update::UpdateEntity *obj : App.get_updates()) {
    if (!match.id_equals(obj->get_object_id()))
      continue;

    if (request->method() == HTTP_GET && match.method_empty()) {
      auto detail = get_request_detail(request);
      std::string data = this->update_json(obj, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    if (!match.method_equals("install")) {
      request->send(404);
      return;
    }

    this->defer([obj]() mutable { obj->perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::update_state_json_generator(WebServer *web_server, void *source) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return web_server->update_json((update::UpdateEntity *) (source), DETAIL_STATE);
}
std::string WebServer::update_all_json_generator(WebServer *web_server, void *source) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return web_server->update_json((update::UpdateEntity *) (source), DETAIL_STATE);
}
std::string WebServer::update_json(update::UpdateEntity *obj, JsonDetail start_config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "update", start_config);
  root["value"] = obj->update_info.latest_version;
  root["state"] = update_state_to_string(obj->state);
  if (start_config == DETAIL_ALL) {
    root["current_version"] = obj->update_info.current_version;
    root["title"] = obj->update_info.title;
    root["summary"] = obj->update_info.summary;
    root["release_url"] = obj->update_info.release_url;
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
#endif

bool WebServer::canHandle(AsyncWebServerRequest *request) const {
  const auto &url = request->url();
  const auto method = request->method();

  // Static URL checks
  static const char *const STATIC_URLS[] = {
    "/",
#if !defined(USE_ESP32) && defined(USE_ARDUINO)
    "/events",
#endif
#ifdef USE_WEBSERVER_CSS_INCLUDE
    "/0.css",
#endif
#ifdef USE_WEBSERVER_JS_INCLUDE
    "/0.js",
#endif
  };

  for (const auto &static_url : STATIC_URLS) {
    if (url == static_url)
      return true;
  }

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
  if (method == HTTP_OPTIONS && request->hasHeader(HEADER_CORS_REQ_PNA))
    return true;
#endif

  // Parse URL for component checks
  UrlMatch match = match_url(url.c_str(), url.length(), true);
  if (!match.valid)
    return false;

  // Common pattern check
  bool is_get = method == HTTP_GET;
  bool is_post = method == HTTP_POST;
  bool is_get_or_post = is_get || is_post;

  if (!is_get_or_post)
    return false;

  // Use lookup tables for domain checks
  static const char *const GET_ONLY_DOMAINS[] = {
#ifdef USE_SENSOR
      "sensor",
#endif
#ifdef USE_BINARY_SENSOR
      "binary_sensor",
#endif
#ifdef USE_TEXT_SENSOR
      "text_sensor",
#endif
#ifdef USE_EVENT
      "event",
#endif
  };

  static const char *const GET_POST_DOMAINS[] = {
#ifdef USE_SWITCH
      "switch",
#endif
#ifdef USE_BUTTON
      "button",
#endif
#ifdef USE_FAN
      "fan",
#endif
#ifdef USE_LIGHT
      "light",
#endif
#ifdef USE_COVER
      "cover",
#endif
#ifdef USE_NUMBER
      "number",
#endif
#ifdef USE_DATETIME_DATE
      "date",
#endif
#ifdef USE_DATETIME_TIME
      "time",
#endif
#ifdef USE_DATETIME_DATETIME
      "datetime",
#endif
#ifdef USE_TEXT
      "text",
#endif
#ifdef USE_SELECT
      "select",
#endif
#ifdef USE_CLIMATE
      "climate",
#endif
#ifdef USE_LOCK
      "lock",
#endif
#ifdef USE_VALVE
      "valve",
#endif
#ifdef USE_ALARM_CONTROL_PANEL
      "alarm_control_panel",
#endif
#ifdef USE_UPDATE
      "update",
#endif
  };

  // Check GET-only domains
  if (is_get) {
    for (const auto &domain : GET_ONLY_DOMAINS) {
      if (match.domain_equals(domain))
        return true;
    }
  }

  // Check GET+POST domains
  if (is_get_or_post) {
    for (const auto &domain : GET_POST_DOMAINS) {
      if (match.domain_equals(domain))
        return true;
    }
  }

  return false;
}
void WebServer::handleRequest(AsyncWebServerRequest *request) {
  const auto &url = request->url();

  // Handle static routes first
  if (url == "/") {
    this->handle_index_request(request);
    return;
  }

#if !defined(USE_ESP32) && defined(USE_ARDUINO)
  if (url == "/events") {
    this->events_.add_new_client(this, request);
    return;
  }
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
  if (url == "/0.css") {
    this->handle_css_request(request);
    return;
  }
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
  if (url == "/0.js") {
    this->handle_js_request(request);
    return;
  }
#endif

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
  if (request->method() == HTTP_OPTIONS && request->hasHeader(HEADER_CORS_REQ_PNA)) {
    this->handle_pna_cors_request(request);
    return;
  }
#endif

  // Parse URL for component routing
  UrlMatch match = match_url(url.c_str(), url.length(), false);

  // Component routing using minimal code repetition
  struct ComponentRoute {
    const char *domain;
    void (WebServer::*handler)(AsyncWebServerRequest *, const UrlMatch &);
  };

  static const ComponentRoute ROUTES[] = {
#ifdef USE_SENSOR
      {"sensor", &WebServer::handle_sensor_request},
#endif
#ifdef USE_SWITCH
      {"switch", &WebServer::handle_switch_request},
#endif
#ifdef USE_BUTTON
      {"button", &WebServer::handle_button_request},
#endif
#ifdef USE_BINARY_SENSOR
      {"binary_sensor", &WebServer::handle_binary_sensor_request},
#endif
#ifdef USE_FAN
      {"fan", &WebServer::handle_fan_request},
#endif
#ifdef USE_LIGHT
      {"light", &WebServer::handle_light_request},
#endif
#ifdef USE_TEXT_SENSOR
      {"text_sensor", &WebServer::handle_text_sensor_request},
#endif
#ifdef USE_COVER
      {"cover", &WebServer::handle_cover_request},
#endif
#ifdef USE_NUMBER
      {"number", &WebServer::handle_number_request},
#endif
#ifdef USE_DATETIME_DATE
      {"date", &WebServer::handle_date_request},
#endif
#ifdef USE_DATETIME_TIME
      {"time", &WebServer::handle_time_request},
#endif
#ifdef USE_DATETIME_DATETIME
      {"datetime", &WebServer::handle_datetime_request},
#endif
#ifdef USE_TEXT
      {"text", &WebServer::handle_text_request},
#endif
#ifdef USE_SELECT
      {"select", &WebServer::handle_select_request},
#endif
#ifdef USE_CLIMATE
      {"climate", &WebServer::handle_climate_request},
#endif
#ifdef USE_LOCK
      {"lock", &WebServer::handle_lock_request},
#endif
#ifdef USE_VALVE
      {"valve", &WebServer::handle_valve_request},
#endif
#ifdef USE_ALARM_CONTROL_PANEL
      {"alarm_control_panel", &WebServer::handle_alarm_control_panel_request},
#endif
#ifdef USE_UPDATE
      {"update", &WebServer::handle_update_request},
#endif
  };

  // Check each route
  for (const auto &route : ROUTES) {
    if (match.domain_equals(route.domain)) {
      (this->*route.handler)(request, match);
      return;
    }
  }

  // No matching handler found - send 404
  ESP_LOGV(TAG, "Request for unknown URL: %s", url.c_str());
  request->send(404, "text/plain", "Not Found");
}

bool WebServer::isRequestHandlerTrivial() const { return false; }

void WebServer::add_sorting_info_(JsonObject &root, EntityBase *entity) {
#ifdef USE_WEBSERVER_SORTING
  if (this->sorting_entitys_.find(entity) != this->sorting_entitys_.end()) {
    root["sorting_weight"] = this->sorting_entitys_[entity].weight;
    if (this->sorting_groups_.find(this->sorting_entitys_[entity].group_id) != this->sorting_groups_.end()) {
      root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[entity].group_id].name;
    }
  }
#endif
}

#ifdef USE_WEBSERVER_SORTING
void WebServer::add_entity_config(EntityBase *entity, float weight, uint64_t group) {
  this->sorting_entitys_[entity] = SortingComponents{weight, group};
}

void WebServer::add_sorting_group(uint64_t group_id, const std::string &group_name, float weight) {
  this->sorting_groups_[group_id] = SortingGroup{group_name, weight};
}
#endif

}  // namespace web_server
}  // namespace esphome
#endif
