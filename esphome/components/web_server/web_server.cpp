#include "web_server.h"
#ifdef USE_WEBSERVER
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
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

namespace esphome::web_server {

static const char *const TAG = "web_server";

// Longest: UPDATE AVAILABLE (16 chars + null terminator, rounded up)
static constexpr size_t PSTR_LOCAL_SIZE = 18;
#define PSTR_LOCAL(mode_s) ESPHOME_strncpy_P(buf, (ESPHOME_PGM_P) ((mode_s)), PSTR_LOCAL_SIZE - 1)

// Parse URL and return match info
// URL formats (disambiguated by HTTP method for 3-segment case):
//   GET  /{domain}/{entity_name} - main device state
//   POST /{domain}/{entity_name}/{action} - main device action
//   GET  /{domain}/{device_name}/{entity_name} - sub-device state (USE_DEVICES only)
//   POST /{domain}/{device_name}/{entity_name}/{action} - sub-device action (USE_DEVICES only)
static UrlMatch match_url(const char *url_ptr, size_t url_len, bool only_domain, bool is_post = false) {
  // URL must start with '/' and have content after it
  if (url_len < 2 || url_ptr[0] != '/')
    return UrlMatch{};

  const char *p = url_ptr + 1;
  const char *end = url_ptr + url_len;

  // Helper to find next segment: returns pointer after '/' or nullptr if no more slashes
  auto next_segment = [&end](const char *start) -> const char * {
    const char *slash = (const char *) memchr(start, '/', end - start);
    return slash ? slash + 1 : nullptr;
  };

  // Helper to make StringRef from segment start to next segment (or end)
  auto make_ref = [&end](const char *start, const char *next_start) -> StringRef {
    return StringRef(start, (next_start ? next_start - 1 : end) - start);
  };

  // Parse domain segment
  const char *s1 = p;
  const char *s2 = next_segment(s1);

  // Must have domain with trailing slash
  if (!s2)
    return UrlMatch{};

  UrlMatch match{};
  match.domain = make_ref(s1, s2);
  match.valid = true;

  if (only_domain || s2 >= end)
    return match;

  // Parse remaining segments only when needed
  const char *s3 = next_segment(s2);
  const char *s4 = s3 ? next_segment(s3) : nullptr;

  StringRef seg2 = make_ref(s2, s3);
  StringRef seg3 = s3 ? make_ref(s3, s4) : StringRef();
  StringRef seg4 = s4 ? make_ref(s4, nullptr) : StringRef();

  // Reject empty segments
  if (seg2.empty() || (s3 && seg3.empty()) || (s4 && seg4.empty()))
    return UrlMatch{};

  // Interpret based on segment count
  if (!s3) {
    // 1 segment after domain: /{domain}/{entity}
    match.id = seg2;
  } else if (!s4) {
    // 2 segments after domain: /{domain}/{X}/{Y}
    // HTTP method disambiguates: GET = device/entity, POST = entity/action
    if (is_post) {
      match.id = seg2;
      match.method = seg3;
      return match;
    }
#ifdef USE_DEVICES
    match.device_name = seg2;
    match.id = seg3;
#else
    return UrlMatch{};  // 3-segment GET not supported without USE_DEVICES
#endif
  } else {
    // 3 segments after domain: /{domain}/{device}/{entity}/{action}
#ifdef USE_DEVICES
    if (!is_post) {
      return UrlMatch{};  // 4-segment GET not supported (action requires POST)
    }
    match.device_name = seg2;
    match.id = seg3;
    match.method = seg4;
#else
    return UrlMatch{};  // Not supported without USE_DEVICES
#endif
  }

  return match;
}

EntityMatchResult UrlMatch::match_entity(EntityBase *entity) const {
  EntityMatchResult result{false, this->method.empty()};

#ifdef USE_DEVICES
  Device *entity_device = entity->get_device();
  bool url_has_device = !this->device_name.empty();
  bool entity_has_device = (entity_device != nullptr);

  // Device matching: URL device segment must match entity's device
  if (url_has_device != entity_has_device) {
    return result;  // Mismatch: one has device, other doesn't
  }
  if (url_has_device && this->device_name != entity_device->get_name()) {
    return result;  // Device name doesn't match
  }
#endif

  // Try matching by entity name (new format)
  if (this->id == entity->get_name()) {
    result.matched = true;
    return result;
  }

  // Fall back to object_id (deprecated format)
  char object_id_buf[OBJECT_ID_MAX_LEN];
  StringRef object_id = entity->get_object_id_to(object_id_buf);
  if (this->id == object_id) {
    result.matched = true;
    // Log deprecation warning
#ifdef USE_DEVICES
    Device *device = entity->get_device();
    if (device != nullptr) {
      ESP_LOGW(TAG,
               "Deprecated URL format: /%.*s/%.*s/%.*s - use entity name '/%.*s/%s/%s' instead. "
               "Object ID URLs will be removed in 2026.7.0.",
               (int) this->domain.size(), this->domain.c_str(), (int) this->device_name.size(),
               this->device_name.c_str(), (int) this->id.size(), this->id.c_str(), (int) this->domain.size(),
               this->domain.c_str(), device->get_name(), entity->get_name().c_str());
    } else
#endif
    {
      ESP_LOGW(TAG,
               "Deprecated URL format: /%.*s/%.*s - use entity name '/%.*s/%s' instead. "
               "Object ID URLs will be removed in 2026.7.0.",
               (int) this->domain.size(), this->domain.c_str(), (int) this->id.size(), this->id.c_str(),
               (int) this->domain.size(), this->domain.c_str(), entity->get_name().c_str());
    }
  }

  return result;
}

#if !defined(USE_ESP32) && defined(USE_ARDUINO)
// helper for allowing only unique entries in the queue
void DeferredUpdateEventSource::deq_push_back_with_dedup_(void *source, message_generator_t *message_generator) {
  DeferredEvent item(source, message_generator);

  // Use range-based for loop instead of std::find_if to reduce template instantiation overhead and binary size
  for (auto &event : this->deferred_queue_) {
    if (event == item) {
      return;  // Already in queue, no need to update since items are equal
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

  es->onConnect([this, es](AsyncEventSourceClient *client) { this->on_client_connect_(es); });

  es->onDisconnect([this, es](AsyncEventSourceClient *client) { this->on_client_disconnect_(es); });

  es->handleRequest(request);
}

void DeferredUpdateEventSourceList::on_client_connect_(DeferredUpdateEventSource *source) {
  WebServer *ws = source->web_server_;
  ws->defer([ws, source]() {
    // Configure reconnect timeout and send config
    // this should always go through since the AsyncEventSourceClient event queue is empty on connect
    std::string message = ws->get_config_json();
    source->try_send_nodefer(message.c_str(), "ping", millis(), 30000);

#ifdef USE_WEBSERVER_SORTING
    for (auto &group : ws->sorting_groups_) {
      json::JsonBuilder builder;
      JsonObject root = builder.root();
      root[ESPHOME_F("name")] = group.second.name;
      root[ESPHOME_F("sorting_weight")] = group.second.weight;
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
  });
}

void DeferredUpdateEventSourceList::on_client_disconnect_(DeferredUpdateEventSource *source) {
  source->web_server_->defer([this, source]() {
    // This method was called via WebServer->defer() and is no longer executing in the
    // context of the network callback. The object is now dead and can be safely deleted.
    this->remove(source);
    delete source;  // NOLINT
  });
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

  root[ESPHOME_F("title")] = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
  char comment_buffer[ESPHOME_COMMENT_SIZE];
  App.get_comment_string(comment_buffer);
  root[ESPHOME_F("comment")] = comment_buffer;
#if defined(USE_WEBSERVER_OTA_DISABLED) || !defined(USE_WEBSERVER_OTA)
  root[ESPHOME_F("ota")] = false;  // Note: USE_WEBSERVER_OTA_DISABLED only affects web_server, not captive_portal
#else
  root[ESPHOME_F("ota")] = true;
#endif
  root[ESPHOME_F("log")] = this->expose_log_;
  root[ESPHOME_F("lang")] = "en";

  return builder.serialize();
}

void WebServer::setup() {
  ControllerRegistry::register_controller(this);
  this->base_->init();

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->expose_log_) {
    logger::global_logger->add_log_listener(this);
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

#ifdef USE_LOGGER
void WebServer::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  (void) level;
  (void) tag;
  (void) message_len;
  this->events_.try_send_nodefer(message, "log", millis());
}
#endif

void WebServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Web Server:\n"
                "  Address: %s:%u",
                network::get_use_address(), this->base_->get_port());
}
float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

#ifdef USE_WEBSERVER_LOCAL
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
#ifndef USE_ESP8266
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
#else
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
#endif
  response->addHeader(ESPHOME_F("Content-Encoding"), ESPHOME_F("gzip"));
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
  response->addHeader(ESPHOME_F("Access-Control-Allow-Private-Network"), ESPHOME_F("true"));
  response->addHeader(ESPHOME_F("Private-Network-Access-Name"), App.get_name().c_str());
  char mac_s[18];
  response->addHeader(ESPHOME_F("Private-Network-Access-ID"), get_mac_address_pretty_into_buffer(mac_s));
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
  response->addHeader(ESPHOME_F("Content-Encoding"), ESPHOME_F("gzip"));
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
  response->addHeader(ESPHOME_F("Content-Encoding"), ESPHOME_F("gzip"));
  request->send(response);
}
#endif

// Helper functions to reduce code size by avoiding macro expansion
// Build unique id as: {domain}/{device_name}/{entity_name} or {domain}/{entity_name}
// Uses names (not object_id) to avoid UTF-8 collision issues
static void set_json_id(JsonObject &root, EntityBase *obj, const char *prefix, JsonDetail start_config) {
  const StringRef &name = obj->get_name();
  size_t prefix_len = strlen(prefix);
  size_t name_len = name.size();

#ifdef USE_DEVICES
  Device *device = obj->get_device();
  const char *device_name = device ? device->get_name() : nullptr;
  size_t device_len = device_name ? strlen(device_name) : 0;
#endif

  // Build id into stack buffer - ArduinoJson copies the string
  // Format: {prefix}/{device?}/{name}
  // Buffer size guaranteed by schema validation (NAME_MAX_LENGTH=120):
  //   With devices: domain(20) + "/" + device(120) + "/" + name(120) + null = 263, rounded up to 280 for safety margin
  //   Without devices: domain(20) + "/" + name(120) + null = 142, rounded up to 150 for safety margin
#ifdef USE_DEVICES
  char id_buf[280];
#else
  char id_buf[150];
#endif
  char *p = id_buf;
  memcpy(p, prefix, prefix_len);
  p += prefix_len;
  *p++ = '/';
#ifdef USE_DEVICES
  if (device_name) {
    memcpy(p, device_name, device_len);
    p += device_len;
    *p++ = '/';
  }
#endif
  memcpy(p, name.c_str(), name_len);
  p[name_len] = '\0';

  root[ESPHOME_F("id")] = id_buf;

  if (start_config == DETAIL_ALL) {
    root[ESPHOME_F("domain")] = prefix;
    root[ESPHOME_F("name")] = name;
#ifdef USE_DEVICES
    if (device_name) {
      root[ESPHOME_F("device")] = device_name;
    }
#endif
    root[ESPHOME_F("icon")] = obj->get_icon_ref();
    root[ESPHOME_F("entity_category")] = obj->get_entity_category();
    bool is_disabled = obj->is_disabled_by_default();
    if (is_disabled)
      root[ESPHOME_F("is_disabled_by_default")] = is_disabled;
  }
}

// Keep as separate function even though only used once: reduces code size by ~48 bytes
// by allowing compiler to share code between template instantiations (bool, float, etc.)
template<typename T>
static void set_json_value(JsonObject &root, EntityBase *obj, const char *prefix, const T &value,
                           JsonDetail start_config) {
  set_json_id(root, obj, prefix, start_config);
  root[ESPHOME_F("value")] = value;
}

template<typename T>
static void set_json_icon_state_value(JsonObject &root, EntityBase *obj, const char *prefix, const char *state,
                                      const T &value, JsonDetail start_config) {
  set_json_value(root, obj, prefix, value, start_config);
  root[ESPHOME_F("state")] = state;
}

// Helper to get request detail parameter
static JsonDetail get_request_detail(AsyncWebServerRequest *request) {
  auto *param = request->getParam("detail");
  return (param && param->value() == "all") ? DETAIL_ALL : DETAIL_STATE;
}

#ifdef USE_SENSOR
void WebServer::on_sensor_update(sensor::Sensor *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", sensor_state_json_generator);
}
void WebServer::handle_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (sensor::Sensor *obj : App.get_sensors()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    // Note: request->method() is always HTTP_GET here (canHandle ensures this)
    if (entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->sensor_json_(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
std::string WebServer::sensor_state_json_generator(WebServer *web_server, void *source) {
  return web_server->sensor_json_((sensor::Sensor *) (source), ((sensor::Sensor *) (source))->state, DETAIL_STATE);
}
std::string WebServer::sensor_all_json_generator(WebServer *web_server, void *source) {
  return web_server->sensor_json_((sensor::Sensor *) (source), ((sensor::Sensor *) (source))->state, DETAIL_ALL);
}
std::string WebServer::sensor_json_(sensor::Sensor *obj, float value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  const auto uom_ref = obj->get_unit_of_measurement_ref();
  char buf[VALUE_ACCURACY_MAX_LEN];
  const char *state = std::isnan(value)
                          ? "NA"
                          : (value_accuracy_with_uom_to_buf(buf, value, obj->get_accuracy_decimals(), uom_ref), buf);
  set_json_icon_state_value(root, obj, "sensor", state, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    if (!uom_ref.empty())
      root[ESPHOME_F("uom")] = uom_ref;
  }

  return builder.serialize();
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", text_sensor_state_json_generator);
}
void WebServer::handle_text_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (text_sensor::TextSensor *obj : App.get_text_sensors()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    // Note: request->method() is always HTTP_GET here (canHandle ensures this)
    if (entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->text_sensor_json_(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
std::string WebServer::text_sensor_state_json_generator(WebServer *web_server, void *source) {
  return web_server->text_sensor_json_((text_sensor::TextSensor *) (source),
                                       ((text_sensor::TextSensor *) (source))->state, DETAIL_STATE);
}
std::string WebServer::text_sensor_all_json_generator(WebServer *web_server, void *source) {
  return web_server->text_sensor_json_((text_sensor::TextSensor *) (source),
                                       ((text_sensor::TextSensor *) (source))->state, DETAIL_ALL);
}
std::string WebServer::text_sensor_json_(text_sensor::TextSensor *obj, const std::string &value,
                                         JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "text_sensor", value.c_str(), value.c_str(), start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_SWITCH
void WebServer::on_switch_update(switch_::Switch *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", switch_state_json_generator);
}
void WebServer::handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (switch_::Switch *obj : App.get_switches()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->switch_json_(obj, obj->state, detail);
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
  return web_server->switch_json_((switch_::Switch *) (source), ((switch_::Switch *) (source))->state, DETAIL_STATE);
}
std::string WebServer::switch_all_json_generator(WebServer *web_server, void *source) {
  return web_server->switch_json_((switch_::Switch *) (source), ((switch_::Switch *) (source))->state, DETAIL_ALL);
}
std::string WebServer::switch_json_(switch_::Switch *obj, bool value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "switch", value ? "ON" : "OFF", value, start_config);
  if (start_config == DETAIL_ALL) {
    root[ESPHOME_F("assumed_state")] = obj->assumed_state();
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_BUTTON
void WebServer::handle_button_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (button::Button *obj : App.get_buttons()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->button_json_(obj, detail);
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
  return web_server->button_json_((button::Button *) (source), DETAIL_STATE);
}
std::string WebServer::button_all_json_generator(WebServer *web_server, void *source) {
  return web_server->button_json_((button::Button *) (source), DETAIL_ALL);
}
std::string WebServer::button_json_(button::Button *obj, JsonDetail start_config) {
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
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", binary_sensor_state_json_generator);
}
void WebServer::handle_binary_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (binary_sensor::BinarySensor *obj : App.get_binary_sensors()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    // Note: request->method() is always HTTP_GET here (canHandle ensures this)
    if (entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->binary_sensor_json_(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
std::string WebServer::binary_sensor_state_json_generator(WebServer *web_server, void *source) {
  return web_server->binary_sensor_json_((binary_sensor::BinarySensor *) (source),
                                         ((binary_sensor::BinarySensor *) (source))->state, DETAIL_STATE);
}
std::string WebServer::binary_sensor_all_json_generator(WebServer *web_server, void *source) {
  return web_server->binary_sensor_json_((binary_sensor::BinarySensor *) (source),
                                         ((binary_sensor::BinarySensor *) (source))->state, DETAIL_ALL);
}
std::string WebServer::binary_sensor_json_(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
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
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", fan_state_json_generator);
}
void WebServer::handle_fan_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (fan::Fan *obj : App.get_fans()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->fan_json_(obj, detail);
      request->send(200, "application/json", data.c_str());
    } else if (match.method_equals("toggle")) {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else {
      bool is_on = match.method_equals("turn_on");
      bool is_off = match.method_equals("turn_off");
      if (!is_on && !is_off) {
        request->send(404);
        return;
      }
      auto call = is_on ? obj->turn_on() : obj->turn_off();

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
    }
    return;
  }
  request->send(404);
}
std::string WebServer::fan_state_json_generator(WebServer *web_server, void *source) {
  return web_server->fan_json_((fan::Fan *) (source), DETAIL_STATE);
}
std::string WebServer::fan_all_json_generator(WebServer *web_server, void *source) {
  return web_server->fan_json_((fan::Fan *) (source), DETAIL_ALL);
}
std::string WebServer::fan_json_(fan::Fan *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "fan", obj->state ? "ON" : "OFF", obj->state, start_config);
  const auto traits = obj->get_traits();
  if (traits.supports_speed()) {
    root[ESPHOME_F("speed_level")] = obj->speed;
    root[ESPHOME_F("speed_count")] = traits.supported_speed_count();
  }
  if (obj->get_traits().supports_oscillation())
    root[ESPHOME_F("oscillation")] = obj->oscillating;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_LIGHT
void WebServer::on_light_update(light::LightState *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", light_state_json_generator);
}
void WebServer::handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (light::LightState *obj : App.get_lights()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->light_json_(obj, detail);
      request->send(200, "application/json", data.c_str());
    } else if (match.method_equals("toggle")) {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else {
      bool is_on = match.method_equals("turn_on");
      bool is_off = match.method_equals("turn_off");
      if (!is_on && !is_off) {
        request->send(404);
        return;
      }
      auto call = is_on ? obj->turn_on() : obj->turn_off();

      if (is_on) {
        // Parse color parameters
        parse_light_param_(request, "brightness", call, &decltype(call)::set_brightness, 255.0f);
        parse_light_param_(request, "r", call, &decltype(call)::set_red, 255.0f);
        parse_light_param_(request, "g", call, &decltype(call)::set_green, 255.0f);
        parse_light_param_(request, "b", call, &decltype(call)::set_blue, 255.0f);
        parse_light_param_(request, "white_value", call, &decltype(call)::set_white, 255.0f);
        parse_light_param_(request, "color_temp", call, &decltype(call)::set_color_temperature);

        // Parse timing parameters
        parse_light_param_uint_(request, "flash", call, &decltype(call)::set_flash_length, 1000);
      }
      parse_light_param_uint_(request, "transition", call, &decltype(call)::set_transition_length, 1000);

      if (is_on) {
        parse_string_param_(request, "effect", call, &decltype(call)::set_effect);
      }

      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::light_state_json_generator(WebServer *web_server, void *source) {
  return web_server->light_json_((light::LightState *) (source), DETAIL_STATE);
}
std::string WebServer::light_all_json_generator(WebServer *web_server, void *source) {
  return web_server->light_json_((light::LightState *) (source), DETAIL_ALL);
}
std::string WebServer::light_json_(light::LightState *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_value(root, obj, "light", obj->remote_values.is_on() ? "ON" : "OFF", start_config);

  light::LightJSONSchema::dump_json(*obj, root);
  if (start_config == DETAIL_ALL) {
    JsonArray opt = root[ESPHOME_F("effects")].to<JsonArray>();
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
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", cover_state_json_generator);
}
void WebServer::handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (cover::Cover *obj : App.get_covers()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->cover_json_(obj, detail);
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
  return web_server->cover_json_((cover::Cover *) (source), DETAIL_STATE);
}
std::string WebServer::cover_all_json_generator(WebServer *web_server, void *source) {
  return web_server->cover_json_((cover::Cover *) (source), DETAIL_ALL);
}
std::string WebServer::cover_json_(cover::Cover *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "cover", obj->is_fully_closed() ? "CLOSED" : "OPEN", obj->position,
                            start_config);
  char buf[PSTR_LOCAL_SIZE];
  root[ESPHOME_F("current_operation")] = PSTR_LOCAL(cover::cover_operation_to_str(obj->current_operation));

  if (obj->get_traits().get_supports_position())
    root[ESPHOME_F("position")] = obj->position;
  if (obj->get_traits().get_supports_tilt())
    root[ESPHOME_F("tilt")] = obj->tilt;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_NUMBER
void WebServer::on_number_update(number::Number *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", number_state_json_generator);
}
void WebServer::handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_numbers()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->number_json_(obj, obj->state, detail);
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
  return web_server->number_json_((number::Number *) (source), ((number::Number *) (source))->state, DETAIL_STATE);
}
std::string WebServer::number_all_json_generator(WebServer *web_server, void *source) {
  return web_server->number_json_((number::Number *) (source), ((number::Number *) (source))->state, DETAIL_ALL);
}
std::string WebServer::number_json_(number::Number *obj, float value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  const auto uom_ref = obj->traits.get_unit_of_measurement_ref();
  const int8_t accuracy = step_to_accuracy_decimals(obj->traits.get_step());

  // Need two buffers: one for value, one for state with UOM
  char val_buf[VALUE_ACCURACY_MAX_LEN];
  char state_buf[VALUE_ACCURACY_MAX_LEN];
  const char *val_str = std::isnan(value) ? "\"NaN\"" : (value_accuracy_to_buf(val_buf, value, accuracy), val_buf);
  const char *state_str =
      std::isnan(value) ? "NA" : (value_accuracy_with_uom_to_buf(state_buf, value, accuracy, uom_ref), state_buf);
  set_json_icon_state_value(root, obj, "number", state_str, val_str, start_config);
  if (start_config == DETAIL_ALL) {
    // ArduinoJson copies the string immediately, so we can reuse val_buf
    root[ESPHOME_F("min_value")] = (value_accuracy_to_buf(val_buf, obj->traits.get_min_value(), accuracy), val_buf);
    root[ESPHOME_F("max_value")] = (value_accuracy_to_buf(val_buf, obj->traits.get_max_value(), accuracy), val_buf);
    root[ESPHOME_F("step")] = (value_accuracy_to_buf(val_buf, obj->traits.get_step(), accuracy), val_buf);
    root[ESPHOME_F("mode")] = (int) obj->traits.get_mode();
    if (!uom_ref.empty())
      root[ESPHOME_F("uom")] = uom_ref;
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_DATETIME_DATE
void WebServer::on_date_update(datetime::DateEntity *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", date_state_json_generator);
}
void WebServer::handle_date_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_dates()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->date_json_(obj, detail);
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
  return web_server->date_json_((datetime::DateEntity *) (source), DETAIL_STATE);
}
std::string WebServer::date_all_json_generator(WebServer *web_server, void *source) {
  return web_server->date_json_((datetime::DateEntity *) (source), DETAIL_ALL);
}
std::string WebServer::date_json_(datetime::DateEntity *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  // Format: YYYY-MM-DD (max 10 chars + null)
  char value[12];
#ifdef USE_ESP8266
  snprintf_P(value, sizeof(value), PSTR("%d-%02d-%02d"), obj->year, obj->month, obj->day);
#else
  snprintf(value, sizeof(value), "%d-%02d-%02d", obj->year, obj->month, obj->day);
#endif
  set_json_icon_state_value(root, obj, "date", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif  // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
void WebServer::on_time_update(datetime::TimeEntity *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", time_state_json_generator);
}
void WebServer::handle_time_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_times()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->time_json_(obj, detail);
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
  return web_server->time_json_((datetime::TimeEntity *) (source), DETAIL_STATE);
}
std::string WebServer::time_all_json_generator(WebServer *web_server, void *source) {
  return web_server->time_json_((datetime::TimeEntity *) (source), DETAIL_ALL);
}
std::string WebServer::time_json_(datetime::TimeEntity *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  // Format: HH:MM:SS (8 chars + null)
  char value[12];
#ifdef USE_ESP8266
  snprintf_P(value, sizeof(value), PSTR("%02d:%02d:%02d"), obj->hour, obj->minute, obj->second);
#else
  snprintf(value, sizeof(value), "%02d:%02d:%02d", obj->hour, obj->minute, obj->second);
#endif
  set_json_icon_state_value(root, obj, "time", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif  // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
void WebServer::on_datetime_update(datetime::DateTimeEntity *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", datetime_state_json_generator);
}
void WebServer::handle_datetime_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_datetimes()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;
    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->datetime_json_(obj, detail);
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
  return web_server->datetime_json_((datetime::DateTimeEntity *) (source), DETAIL_STATE);
}
std::string WebServer::datetime_all_json_generator(WebServer *web_server, void *source) {
  return web_server->datetime_json_((datetime::DateTimeEntity *) (source), DETAIL_ALL);
}
std::string WebServer::datetime_json_(datetime::DateTimeEntity *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  // Format: YYYY-MM-DD HH:MM:SS (max 19 chars + null)
  char value[24];
#ifdef USE_ESP8266
  snprintf_P(value, sizeof(value), PSTR("%d-%02d-%02d %02d:%02d:%02d"), obj->year, obj->month, obj->day, obj->hour,
             obj->minute, obj->second);
#else
  snprintf(value, sizeof(value), "%d-%02d-%02d %02d:%02d:%02d", obj->year, obj->month, obj->day, obj->hour, obj->minute,
           obj->second);
#endif
  set_json_icon_state_value(root, obj, "datetime", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif  // USE_DATETIME_DATETIME

#ifdef USE_TEXT
void WebServer::on_text_update(text::Text *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", text_state_json_generator);
}
void WebServer::handle_text_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_texts()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->text_json_(obj, obj->state, detail);
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
  return web_server->text_json_((text::Text *) (source), ((text::Text *) (source))->state, DETAIL_STATE);
}
std::string WebServer::text_all_json_generator(WebServer *web_server, void *source) {
  return web_server->text_json_((text::Text *) (source), ((text::Text *) (source))->state, DETAIL_ALL);
}
std::string WebServer::text_json_(text::Text *obj, const std::string &value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  const char *state = obj->traits.get_mode() == text::TextMode::TEXT_MODE_PASSWORD ? "********" : value.c_str();
  set_json_icon_state_value(root, obj, "text", state, value.c_str(), start_config);
  root[ESPHOME_F("min_length")] = obj->traits.get_min_length();
  root[ESPHOME_F("max_length")] = obj->traits.get_max_length();
  root[ESPHOME_F("pattern")] = obj->traits.get_pattern_c_str();
  if (start_config == DETAIL_ALL) {
    root[ESPHOME_F("mode")] = (int) obj->traits.get_mode();
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_SELECT
void WebServer::on_select_update(select::Select *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", select_state_json_generator);
}
void WebServer::handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_selects()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->select_json_(obj, obj->has_state() ? obj->current_option() : "", detail);
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
  auto *obj = (select::Select *) (source);
  return web_server->select_json_(obj, obj->has_state() ? obj->current_option() : "", DETAIL_STATE);
}
std::string WebServer::select_all_json_generator(WebServer *web_server, void *source) {
  auto *obj = (select::Select *) (source);
  return web_server->select_json_(obj, obj->has_state() ? obj->current_option() : "", DETAIL_ALL);
}
std::string WebServer::select_json_(select::Select *obj, const char *value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "select", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    JsonArray opt = root[ESPHOME_F("option")].to<JsonArray>();
    for (auto &option : obj->traits.get_options()) {
      opt.add(option);
    }
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_CLIMATE
void WebServer::on_climate_update(climate::Climate *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", climate_state_json_generator);
}
void WebServer::handle_climate_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_climates()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->climate_json_(obj, detail);
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
  return web_server->climate_json_((climate::Climate *) (source), DETAIL_STATE);
}
std::string WebServer::climate_all_json_generator(WebServer *web_server, void *source) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return web_server->climate_json_((climate::Climate *) (source), DETAIL_ALL);
}
std::string WebServer::climate_json_(climate::Climate *obj, JsonDetail start_config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  json::JsonBuilder builder;
  JsonObject root = builder.root();
  set_json_id(root, obj, "climate", start_config);
  const auto traits = obj->get_traits();
  int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
  int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
  char buf[PSTR_LOCAL_SIZE];
  char temp_buf[VALUE_ACCURACY_MAX_LEN];

  if (start_config == DETAIL_ALL) {
    JsonArray opt = root[ESPHOME_F("modes")].to<JsonArray>();
    for (climate::ClimateMode m : traits.get_supported_modes())
      opt.add(PSTR_LOCAL(climate::climate_mode_to_string(m)));
    if (!traits.get_supported_custom_fan_modes().empty()) {
      JsonArray opt = root[ESPHOME_F("fan_modes")].to<JsonArray>();
      for (climate::ClimateFanMode m : traits.get_supported_fan_modes())
        opt.add(PSTR_LOCAL(climate::climate_fan_mode_to_string(m)));
    }

    if (!traits.get_supported_custom_fan_modes().empty()) {
      JsonArray opt = root[ESPHOME_F("custom_fan_modes")].to<JsonArray>();
      for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
        opt.add(custom_fan_mode);
    }
    if (traits.get_supports_swing_modes()) {
      JsonArray opt = root[ESPHOME_F("swing_modes")].to<JsonArray>();
      for (auto swing_mode : traits.get_supported_swing_modes())
        opt.add(PSTR_LOCAL(climate::climate_swing_mode_to_string(swing_mode)));
    }
    if (traits.get_supports_presets() && obj->preset.has_value()) {
      JsonArray opt = root[ESPHOME_F("presets")].to<JsonArray>();
      for (climate::ClimatePreset m : traits.get_supported_presets())
        opt.add(PSTR_LOCAL(climate::climate_preset_to_string(m)));
    }
    if (!traits.get_supported_custom_presets().empty() && obj->has_custom_preset()) {
      JsonArray opt = root[ESPHOME_F("custom_presets")].to<JsonArray>();
      for (auto const &custom_preset : traits.get_supported_custom_presets())
        opt.add(custom_preset);
    }
    this->add_sorting_info_(root, obj);
  }

  bool has_state = false;
  root[ESPHOME_F("mode")] = PSTR_LOCAL(climate_mode_to_string(obj->mode));
  root[ESPHOME_F("max_temp")] =
      (value_accuracy_to_buf(temp_buf, traits.get_visual_max_temperature(), target_accuracy), temp_buf);
  root[ESPHOME_F("min_temp")] =
      (value_accuracy_to_buf(temp_buf, traits.get_visual_min_temperature(), target_accuracy), temp_buf);
  root[ESPHOME_F("step")] = traits.get_visual_target_temperature_step();
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION)) {
    root[ESPHOME_F("action")] = PSTR_LOCAL(climate_action_to_string(obj->action));
    root[ESPHOME_F("state")] = root[ESPHOME_F("action")];
    has_state = true;
  }
  if (traits.get_supports_fan_modes() && obj->fan_mode.has_value()) {
    root[ESPHOME_F("fan_mode")] = PSTR_LOCAL(climate_fan_mode_to_string(obj->fan_mode.value()));
  }
  if (!traits.get_supported_custom_fan_modes().empty() && obj->has_custom_fan_mode()) {
    root[ESPHOME_F("custom_fan_mode")] = obj->get_custom_fan_mode();
  }
  if (traits.get_supports_presets() && obj->preset.has_value()) {
    root[ESPHOME_F("preset")] = PSTR_LOCAL(climate_preset_to_string(obj->preset.value()));
  }
  if (!traits.get_supported_custom_presets().empty() && obj->has_custom_preset()) {
    root[ESPHOME_F("custom_preset")] = obj->get_custom_preset();
  }
  if (traits.get_supports_swing_modes()) {
    root[ESPHOME_F("swing_mode")] = PSTR_LOCAL(climate_swing_mode_to_string(obj->swing_mode));
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE)) {
    root[ESPHOME_F("current_temperature")] =
        std::isnan(obj->current_temperature)
            ? "NA"
            : (value_accuracy_to_buf(temp_buf, obj->current_temperature, current_accuracy), temp_buf);
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    root[ESPHOME_F("target_temperature_low")] =
        (value_accuracy_to_buf(temp_buf, obj->target_temperature_low, target_accuracy), temp_buf);
    root[ESPHOME_F("target_temperature_high")] =
        (value_accuracy_to_buf(temp_buf, obj->target_temperature_high, target_accuracy), temp_buf);
    if (!has_state) {
      root[ESPHOME_F("state")] =
          (value_accuracy_to_buf(temp_buf, (obj->target_temperature_high + obj->target_temperature_low) / 2.0f,
                                 target_accuracy),
           temp_buf);
    }
  } else {
    root[ESPHOME_F("target_temperature")] =
        (value_accuracy_to_buf(temp_buf, obj->target_temperature, target_accuracy), temp_buf);
    if (!has_state)
      root[ESPHOME_F("state")] = root[ESPHOME_F("target_temperature")];
  }

  return builder.serialize();
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
#endif

#ifdef USE_LOCK
void WebServer::on_lock_update(lock::Lock *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", lock_state_json_generator);
}
void WebServer::handle_lock_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (lock::Lock *obj : App.get_locks()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->lock_json_(obj, obj->state, detail);
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
  return web_server->lock_json_((lock::Lock *) (source), ((lock::Lock *) (source))->state, DETAIL_STATE);
}
std::string WebServer::lock_all_json_generator(WebServer *web_server, void *source) {
  return web_server->lock_json_((lock::Lock *) (source), ((lock::Lock *) (source))->state, DETAIL_ALL);
}
std::string WebServer::lock_json_(lock::Lock *obj, lock::LockState value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  char buf[PSTR_LOCAL_SIZE];
  set_json_icon_state_value(root, obj, "lock", PSTR_LOCAL(lock::lock_state_to_string(value)), value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_VALVE
void WebServer::on_valve_update(valve::Valve *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", valve_state_json_generator);
}
void WebServer::handle_valve_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (valve::Valve *obj : App.get_valves()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->valve_json_(obj, detail);
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
  return web_server->valve_json_((valve::Valve *) (source), DETAIL_STATE);
}
std::string WebServer::valve_all_json_generator(WebServer *web_server, void *source) {
  return web_server->valve_json_((valve::Valve *) (source), DETAIL_ALL);
}
std::string WebServer::valve_json_(valve::Valve *obj, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_icon_state_value(root, obj, "valve", obj->is_fully_closed() ? "CLOSED" : "OPEN", obj->position,
                            start_config);
  char buf[PSTR_LOCAL_SIZE];
  root[ESPHOME_F("current_operation")] = PSTR_LOCAL(valve::valve_operation_to_str(obj->current_operation));

  if (obj->get_traits().get_supports_position())
    root[ESPHOME_F("position")] = obj->position;
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
void WebServer::on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", alarm_control_panel_state_json_generator);
}
void WebServer::handle_alarm_control_panel_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (alarm_control_panel::AlarmControlPanel *obj : App.get_alarm_control_panels()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->alarm_control_panel_json_(obj, obj->get_state(), detail);
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
  return web_server->alarm_control_panel_json_((alarm_control_panel::AlarmControlPanel *) (source),
                                               ((alarm_control_panel::AlarmControlPanel *) (source))->get_state(),
                                               DETAIL_STATE);
}
std::string WebServer::alarm_control_panel_all_json_generator(WebServer *web_server, void *source) {
  return web_server->alarm_control_panel_json_((alarm_control_panel::AlarmControlPanel *) (source),
                                               ((alarm_control_panel::AlarmControlPanel *) (source))->get_state(),
                                               DETAIL_ALL);
}
std::string WebServer::alarm_control_panel_json_(alarm_control_panel::AlarmControlPanel *obj,
                                                 alarm_control_panel::AlarmControlPanelState value,
                                                 JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  char buf[PSTR_LOCAL_SIZE];
  set_json_icon_state_value(root, obj, "alarm-control-panel", PSTR_LOCAL(alarm_control_panel_state_to_string(value)),
                            value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
#endif

#ifdef USE_EVENT
void WebServer::on_event(event::Event *obj) {
  if (!this->include_internal_ && obj->is_internal())
    return;
  this->events_.deferrable_send_state(obj, "state", event_state_json_generator);
}

void WebServer::handle_event_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (event::Event *obj : App.get_events()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    // Note: request->method() is always HTTP_GET here (canHandle ensures this)
    if (entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->event_json_(obj, "", detail);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}

static std::string get_event_type(event::Event *event) {
  const char *last_type = event ? event->get_last_event_type() : nullptr;
  return last_type ? last_type : "";
}

std::string WebServer::event_state_json_generator(WebServer *web_server, void *source) {
  auto *event = static_cast<event::Event *>(source);
  return web_server->event_json_(event, get_event_type(event), DETAIL_STATE);
}
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
std::string WebServer::event_all_json_generator(WebServer *web_server, void *source) {
  auto *event = static_cast<event::Event *>(source);
  return web_server->event_json_(event, get_event_type(event), DETAIL_ALL);
}
std::string WebServer::event_json_(event::Event *obj, const std::string &event_type, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  set_json_id(root, obj, "event", start_config);
  if (!event_type.empty()) {
    root[ESPHOME_F("event_type")] = event_type;
  }
  if (start_config == DETAIL_ALL) {
    JsonArray event_types = root[ESPHOME_F("event_types")].to<JsonArray>();
    for (const char *event_type : obj->get_event_types()) {
      event_types.add(event_type);
    }
    root[ESPHOME_F("device_class")] = obj->get_device_class_ref();
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
#endif

#ifdef USE_UPDATE
static const LogString *update_state_to_string(update::UpdateState state) {
  switch (state) {
    case update::UPDATE_STATE_NO_UPDATE:
      return LOG_STR("NO UPDATE");
    case update::UPDATE_STATE_AVAILABLE:
      return LOG_STR("UPDATE AVAILABLE");
    case update::UPDATE_STATE_INSTALLING:
      return LOG_STR("INSTALLING");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void WebServer::on_update(update::UpdateEntity *obj) {
  this->events_.deferrable_send_state(obj, "state", update_state_json_generator);
}
void WebServer::handle_update_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (update::UpdateEntity *obj : App.get_updates()) {
    auto entity_match = match.match_entity(obj);
    if (!entity_match.matched)
      continue;

    if (request->method() == HTTP_GET && entity_match.action_is_empty) {
      auto detail = get_request_detail(request);
      std::string data = this->update_json_(obj, detail);
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
  return web_server->update_json_((update::UpdateEntity *) (source), DETAIL_STATE);
}
std::string WebServer::update_all_json_generator(WebServer *web_server, void *source) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return web_server->update_json_((update::UpdateEntity *) (source), DETAIL_STATE);
}
std::string WebServer::update_json_(update::UpdateEntity *obj, JsonDetail start_config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  json::JsonBuilder builder;
  JsonObject root = builder.root();

  char buf[PSTR_LOCAL_SIZE];
  set_json_icon_state_value(root, obj, "update", PSTR_LOCAL(update_state_to_string(obj->state)),
                            obj->update_info.latest_version, start_config);
  if (start_config == DETAIL_ALL) {
    root[ESPHOME_F("current_version")] = obj->update_info.current_version;
    root[ESPHOME_F("title")] = obj->update_info.title;
    root[ESPHOME_F("summary")] = obj->update_info.summary;
    root[ESPHOME_F("release_url")] = obj->update_info.release_url;
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
  if (method == HTTP_OPTIONS && request->hasHeader(ESPHOME_F("Access-Control-Request-Private-Network")))
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
  if (request->method() == HTTP_OPTIONS && request->hasHeader(ESPHOME_F("Access-Control-Request-Private-Network"))) {
    this->handle_pna_cors_request(request);
    return;
  }
#endif

  // Parse URL for component routing
  // Pass HTTP method to disambiguate 3-segment URLs (GET=sub-device state, POST=main device action)
  UrlMatch match = match_url(url.c_str(), url.length(), false, request->method() == HTTP_POST);

  // Route to appropriate handler based on domain
  // NOLINTNEXTLINE(readability-simplify-boolean-expr)
  if (false) {  // Start chain for else-if macro pattern
  }
#ifdef USE_SENSOR
  else if (match.domain_equals("sensor")) {
    this->handle_sensor_request(request, match);
  }
#endif
#ifdef USE_SWITCH
  else if (match.domain_equals("switch")) {
    this->handle_switch_request(request, match);
  }
#endif
#ifdef USE_BUTTON
  else if (match.domain_equals("button")) {
    this->handle_button_request(request, match);
  }
#endif
#ifdef USE_BINARY_SENSOR
  else if (match.domain_equals("binary_sensor")) {
    this->handle_binary_sensor_request(request, match);
  }
#endif
#ifdef USE_FAN
  else if (match.domain_equals("fan")) {
    this->handle_fan_request(request, match);
  }
#endif
#ifdef USE_LIGHT
  else if (match.domain_equals("light")) {
    this->handle_light_request(request, match);
  }
#endif
#ifdef USE_TEXT_SENSOR
  else if (match.domain_equals("text_sensor")) {
    this->handle_text_sensor_request(request, match);
  }
#endif
#ifdef USE_COVER
  else if (match.domain_equals("cover")) {
    this->handle_cover_request(request, match);
  }
#endif
#ifdef USE_NUMBER
  else if (match.domain_equals("number")) {
    this->handle_number_request(request, match);
  }
#endif
#ifdef USE_DATETIME_DATE
  else if (match.domain_equals("date")) {
    this->handle_date_request(request, match);
  }
#endif
#ifdef USE_DATETIME_TIME
  else if (match.domain_equals("time")) {
    this->handle_time_request(request, match);
  }
#endif
#ifdef USE_DATETIME_DATETIME
  else if (match.domain_equals("datetime")) {
    this->handle_datetime_request(request, match);
  }
#endif
#ifdef USE_TEXT
  else if (match.domain_equals("text")) {
    this->handle_text_request(request, match);
  }
#endif
#ifdef USE_SELECT
  else if (match.domain_equals("select")) {
    this->handle_select_request(request, match);
  }
#endif
#ifdef USE_CLIMATE
  else if (match.domain_equals("climate")) {
    this->handle_climate_request(request, match);
  }
#endif
#ifdef USE_LOCK
  else if (match.domain_equals("lock")) {
    this->handle_lock_request(request, match);
  }
#endif
#ifdef USE_VALVE
  else if (match.domain_equals("valve")) {
    this->handle_valve_request(request, match);
  }
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  else if (match.domain_equals("alarm_control_panel")) {
    this->handle_alarm_control_panel_request(request, match);
  }
#endif
#ifdef USE_UPDATE
  else if (match.domain_equals("update")) {
    this->handle_update_request(request, match);
  }
#endif
  else {
    // No matching handler found - send 404
    ESP_LOGV(TAG, "Request for unknown URL: %s", url.c_str());
    request->send(404, "text/plain", "Not Found");
  }
}

bool WebServer::isRequestHandlerTrivial() const { return false; }

void WebServer::add_sorting_info_(JsonObject &root, EntityBase *entity) {
#ifdef USE_WEBSERVER_SORTING
  if (this->sorting_entitys_.find(entity) != this->sorting_entitys_.end()) {
    root[ESPHOME_F("sorting_weight")] = this->sorting_entitys_[entity].weight;
    if (this->sorting_groups_.find(this->sorting_entitys_[entity].group_id) != this->sorting_groups_.end()) {
      root[ESPHOME_F("sorting_group")] = this->sorting_groups_[this->sorting_entitys_[entity].group_id].name;
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

}  // namespace esphome::web_server
#endif
