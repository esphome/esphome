#include "web_server.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#ifdef USE_ARDUINO
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
#include "server_index.h"
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

#if USE_WEBSERVER_VERSION == 1
void write_row(AsyncResponseStream *stream, EntityBase *obj, const std::string &klass, const std::string &action,
               const std::function<void(AsyncResponseStream &stream, EntityBase *obj)> &action_func = nullptr) {
  stream->print("<tr class=\"");
  stream->print(klass.c_str());
  if (obj->is_internal())
    stream->print(" internal");
  stream->print("\" id=\"");
  stream->print(klass.c_str());
  stream->print("-");
  stream->print(obj->get_object_id().c_str());
  stream->print("\"><td>");
  stream->print(obj->get_name().c_str());
  stream->print("</td><td></td><td>");
  stream->print(action.c_str());
  if (action_func) {
    action_func(*stream, obj);
  }
  stream->print("</td>");
  stream->print("</tr>");
}
#endif

UrlMatch match_url(const std::string &url, bool only_domain = false) {
  UrlMatch match;
  match.valid = false;
  size_t domain_end = url.find('/', 1);
  if (domain_end == std::string::npos)
    return match;
  match.domain = url.substr(1, domain_end - 1);
  if (only_domain) {
    match.valid = true;
    return match;
  }
  if (url.length() == domain_end - 1)
    return match;
  size_t id_begin = domain_end + 1;
  size_t id_end = url.find('/', id_begin);
  match.valid = true;
  if (id_end == std::string::npos) {
    match.id = url.substr(id_begin, url.length() - id_begin);
    return match;
  }
  match.id = url.substr(id_begin, id_end - id_begin);
  size_t method_begin = id_end + 1;
  match.method = url.substr(method_begin, url.length() - method_begin);
  return match;
}

WebServer::WebServer(web_server_base::WebServerBase *base)
    : base_(base), entities_iterator_(ListEntitiesIterator(this)) {
#ifdef USE_ESP32
  to_schedule_lock_ = xSemaphoreCreateMutex();
#endif
}

#if USE_WEBSERVER_VERSION == 1
void WebServer::set_css_url(const char *css_url) { this->css_url_ = css_url; }
void WebServer::set_js_url(const char *js_url) { this->js_url_ = js_url; }
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
void WebServer::set_css_include(const char *css_include) { this->css_include_ = css_include; }
#endif
#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::set_js_include(const char *js_include) { this->js_include_ = js_include; }
#endif

std::string WebServer::get_config_json() {
  return json::build_json([this](JsonObject root) {
    root["title"] = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
    root["comment"] = App.get_comment();
    root["ota"] = this->allow_ota_;
    root["log"] = this->expose_log_;
    root["lang"] = "en";
  });
}

void WebServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up web server...");
  this->setup_controller(this->include_internal_);
  this->base_->init();

  this->events_.onConnect([this](AsyncEventSourceClient *client) {
    // Configure reconnect timeout and send config
    client->send(this->get_config_json().c_str(), "ping", millis(), 30000);

    this->entities_iterator_.begin(this->include_internal_);
  });

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->expose_log_) {
    logger::global_logger->add_on_log_callback(
        [this](int level, const char *tag, const char *message) { this->events_.send(message, "log", millis()); });
  }
#endif
  this->base_->add_handler(&this->events_);
  this->base_->add_handler(this);

  if (this->allow_ota_)
    this->base_->add_ota_handler();

  this->set_interval(10000, [this]() { this->events_.send("", "ping", millis(), 30000); });
}
void WebServer::loop() {
#ifdef USE_ESP32
  if (xSemaphoreTake(this->to_schedule_lock_, 0L)) {
    std::function<void()> fn;
    if (!to_schedule_.empty()) {
      // scheduler execute things out of order which may lead to incorrect state
      // this->defer(std::move(to_schedule_.front()));
      // let's execute it directly from the loop
      fn = std::move(to_schedule_.front());
      to_schedule_.pop_front();
    }
    xSemaphoreGive(this->to_schedule_lock_);
    if (fn) {
      fn();
    }
  }
#endif
  this->entities_iterator_.advance();
}
void WebServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Web Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
}
float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

#ifdef USE_WEBSERVER_LOCAL
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
#elif USE_WEBSERVER_VERSION == 1
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/html");
  const std::string &title = App.get_name();
  stream->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                  "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\"><title>"));
  stream->print(title.c_str());
  stream->print(F("</title>"));
#ifdef USE_WEBSERVER_CSS_INCLUDE
  stream->print(F("<link rel=\"stylesheet\" href=\"/0.css\">"));
#endif
  if (strlen(this->css_url_) > 0) {
    stream->print(F(R"(<link rel="stylesheet" href=")"));
    stream->print(this->css_url_);
    stream->print(F("\">"));
  }
  stream->print(F("</head><body>"));
  stream->print(F("<article class=\"markdown-body\"><h1>"));
  stream->print(title.c_str());
  stream->print(F("</h1>"));
  stream->print(F("<h2>States</h2><table id=\"states\"><thead><tr><th>Name<th>State<th>Actions<tbody>"));

#ifdef USE_SENSOR
  for (auto *obj : App.get_sensors()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "sensor", "");
  }
#endif

#ifdef USE_SWITCH
  for (auto *obj : App.get_switches()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "switch", "<button>Toggle</button>");
  }
#endif

#ifdef USE_BUTTON
  for (auto *obj : App.get_buttons())
    write_row(stream, obj, "button", "<button>Press</button>");
#endif

#ifdef USE_BINARY_SENSOR
  for (auto *obj : App.get_binary_sensors()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "binary_sensor", "");
  }
#endif

#ifdef USE_FAN
  for (auto *obj : App.get_fans()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "fan", "<button>Toggle</button>");
  }
#endif

#ifdef USE_LIGHT
  for (auto *obj : App.get_lights()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "light", "<button>Toggle</button>");
  }
#endif

#ifdef USE_TEXT_SENSOR
  for (auto *obj : App.get_text_sensors()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "text_sensor", "");
  }
#endif

#ifdef USE_COVER
  for (auto *obj : App.get_covers()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "cover", "<button>Open</button><button>Close</button>");
  }
#endif

#ifdef USE_NUMBER
  for (auto *obj : App.get_numbers()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "number", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        number::Number *number = (number::Number *) obj;
        stream.print(R"(<input type="number" min=")");
        stream.print(number->traits.get_min_value());
        stream.print(R"(" max=")");
        stream.print(number->traits.get_max_value());
        stream.print(R"(" step=")");
        stream.print(number->traits.get_step());
        stream.print(R"(" value=")");
        stream.print(number->state);
        stream.print(R"("/>)");
      });
    }
  }
#endif

#ifdef USE_TEXT
  for (auto *obj : App.get_texts()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "text", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        text::Text *text = (text::Text *) obj;
        auto mode = (int) text->traits.get_mode();
        stream.print(R"(<input type=")");
        if (mode == 2) {
          stream.print(R"(password)");
        } else {  // default
          stream.print(R"(text)");
        }
        stream.print(R"(" minlength=")");
        stream.print(text->traits.get_min_length());
        stream.print(R"(" maxlength=")");
        stream.print(text->traits.get_max_length());
        stream.print(R"(" pattern=")");
        stream.print(text->traits.get_pattern().c_str());
        stream.print(R"(" value=")");
        stream.print(text->state.c_str());
        stream.print(R"("/>)");
      });
    }
  }
#endif

#ifdef USE_SELECT
  for (auto *obj : App.get_selects()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "select", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        select::Select *select = (select::Select *) obj;
        stream.print("<select>");
        stream.print("<option></option>");
        for (auto const &option : select->traits.get_options()) {
          stream.print("<option>");
          stream.print(option.c_str());
          stream.print("</option>");
        }
        stream.print("</select>");
      });
    }
  }
#endif

#ifdef USE_LOCK
  for (auto *obj : App.get_locks()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "lock", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        lock::Lock *lock = (lock::Lock *) obj;
        stream.print("<button>Lock</button><button>Unlock</button>");
        if (lock->traits.get_supports_open()) {
          stream.print("<button>Open</button>");
        }
      });
    }
  }
#endif

#ifdef USE_CLIMATE
  for (auto *obj : App.get_climates()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "climate", "");
  }
#endif

  stream->print(F("</tbody></table><p>See <a href=\"https://esphome.io/web-api/index.html\">ESPHome Web API</a> for "
                  "REST API documentation.</p>"));
  if (this->allow_ota_) {
    stream->print(
        F("<h2>OTA Update</h2><form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\"><input "
          "type=\"file\" name=\"update\"><input type=\"submit\" value=\"Update\"></form>"));
  }
  stream->print(F("<h2>Debug Log</h2><pre id=\"log\"></pre>"));
#ifdef USE_WEBSERVER_JS_INCLUDE
  if (this->js_include_ != nullptr) {
    stream->print(F("<script type=\"module\" src=\"/0.js\"></script>"));
  }
#endif
  if (strlen(this->js_url_) > 0) {
    stream->print(F("<script src=\""));
    stream->print(this->js_url_);
    stream->print(F("\"></script>"));
  }
  stream->print(F("</article></body></html>"));
  request->send(stream);
}
#elif USE_WEBSERVER_VERSION >= 2
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/html", ESPHOME_WEBSERVER_INDEX_HTML, ESPHOME_WEBSERVER_INDEX_HTML_SIZE);
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
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/css", ESPHOME_WEBSERVER_CSS_INCLUDE, ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE);
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::handle_js_request(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/javascript", ESPHOME_WEBSERVER_JS_INCLUDE, ESPHOME_WEBSERVER_JS_INCLUDE_SIZE);
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
#endif

#define set_json_id(root, obj, sensor, start_config) \
  (root)["id"] = sensor; \
  if (((start_config) == DETAIL_ALL)) { \
    (root)["name"] = (obj)->get_name(); \
    (root)["icon"] = (obj)->get_icon(); \
    (root)["entity_category"] = (obj)->get_entity_category(); \
    if ((obj)->is_disabled_by_default()) \
      (root)["is_disabled_by_default"] = (obj)->is_disabled_by_default(); \
  }

#define set_json_value(root, obj, sensor, value, start_config) \
  set_json_id((root), (obj), sensor, start_config); \
  (root)["value"] = value;

#define set_json_icon_state_value(root, obj, sensor, state, value, start_config) \
  set_json_value(root, obj, sensor, value, start_config); \
  (root)["state"] = state;

#ifdef USE_SENSOR
void WebServer::on_sensor_update(sensor::Sensor *obj, float state) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (sensor::Sensor *obj : App.get_sensors()) {
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->sensor_json(obj, obj->state, DETAIL_STATE);
    request->send(200, "application/json", data.c_str());
    return;
  }
  request->send(404);
}
std::string WebServer::sensor_json(sensor::Sensor *obj, float value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    std::string state;
    if (std::isnan(value)) {
      state = "NA";
    } else {
      state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
      if (!obj->get_unit_of_measurement().empty())
        state += " " + obj->get_unit_of_measurement();
    }
    set_json_icon_state_value(root, obj, "sensor-" + obj->get_object_id(), state, value, start_config);
    if (start_config == DETAIL_ALL) {
      if (!obj->get_unit_of_measurement().empty())
        root["uom"] = obj->get_unit_of_measurement();
    }
  });
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->text_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_text_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (text_sensor::TextSensor *obj : App.get_text_sensors()) {
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->text_sensor_json(obj, obj->state, DETAIL_STATE);
    request->send(200, "application/json", data.c_str());
    return;
  }
  request->send(404);
}
std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value,
                                        JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "text_sensor-" + obj->get_object_id(), value, value, start_config);
  });
}
#endif

#ifdef USE_SWITCH
void WebServer::on_switch_update(switch_::Switch *obj, bool state) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->switch_json(obj, state, DETAIL_STATE).c_str(), "state");
}
std::string WebServer::switch_json(switch_::Switch *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "switch-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
    if (start_config == DETAIL_ALL) {
      root["assumed_state"] = obj->assumed_state();
    }
  });
}
void WebServer::handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (switch_::Switch *obj : App.get_switches()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->switch_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "toggle") {
      this->schedule_([obj]() { obj->toggle(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      this->schedule_([obj]() { obj->turn_on(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      this->schedule_([obj]() { obj->turn_off(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_BUTTON
std::string WebServer::button_json(button::Button *obj, JsonDetail start_config) {
  return json::build_json(
      [obj, start_config](JsonObject root) { set_json_id(root, obj, "button-" + obj->get_object_id(), start_config); });
}

void WebServer::handle_button_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (button::Button *obj : App.get_buttons()) {
    if (obj->get_object_id() != match.id)
      continue;
    if (match.method == "press") {
      this->schedule_([obj]() { obj->press(); });
      request->send(200);
      return;
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_BINARY_SENSOR
void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->binary_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
}
std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "binary_sensor-" + obj->get_object_id(), value ? "ON" : "OFF", value,
                              start_config);
  });
}
void WebServer::handle_binary_sensor_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (binary_sensor::BinarySensor *obj : App.get_binary_sensors()) {
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->binary_sensor_json(obj, obj->state, DETAIL_STATE);
    request->send(200, "application/json", data.c_str());
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_FAN
void WebServer::on_fan_update(fan::Fan *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->fan_json(obj, DETAIL_STATE).c_str(), "state");
}
std::string WebServer::fan_json(fan::Fan *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "fan-" + obj->get_object_id(), obj->state ? "ON" : "OFF", obj->state,
                              start_config);
    const auto traits = obj->get_traits();
    if (traits.supports_speed()) {
      root["speed_level"] = obj->speed;
      root["speed_count"] = traits.supported_speed_count();
    }
    if (obj->get_traits().supports_oscillation())
      root["oscillation"] = obj->oscillating;
  });
}
void WebServer::handle_fan_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (fan::Fan *obj : App.get_fans()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->fan_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "toggle") {
      this->schedule_([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("speed_level")) {
        auto speed_level = request->getParam("speed_level")->value();
        auto val = parse_number<int>(speed_level.c_str());
        if (!val.has_value()) {
          ESP_LOGW(TAG, "Can't convert '%s' to number!", speed_level.c_str());
          return;
        }
        call.set_speed(*val);
      }
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
      this->schedule_([call]() mutable { call.perform(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      this->schedule_([obj]() { obj->turn_off().perform(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_LIGHT
void WebServer::on_light_update(light::LightState *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->light_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (light::LightState *obj : App.get_lights()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->light_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "toggle") {
      this->schedule_([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("brightness")) {
        auto brightness = parse_number<float>(request->getParam("brightness")->value().c_str());
        if (brightness.has_value()) {
          call.set_brightness(*brightness / 255.0f);
        }
      }
      if (request->hasParam("r")) {
        auto r = parse_number<float>(request->getParam("r")->value().c_str());
        if (r.has_value()) {
          call.set_red(*r / 255.0f);
        }
      }
      if (request->hasParam("g")) {
        auto g = parse_number<float>(request->getParam("g")->value().c_str());
        if (g.has_value()) {
          call.set_green(*g / 255.0f);
        }
      }
      if (request->hasParam("b")) {
        auto b = parse_number<float>(request->getParam("b")->value().c_str());
        if (b.has_value()) {
          call.set_blue(*b / 255.0f);
        }
      }
      if (request->hasParam("white_value")) {
        auto white_value = parse_number<float>(request->getParam("white_value")->value().c_str());
        if (white_value.has_value()) {
          call.set_white(*white_value / 255.0f);
        }
      }
      if (request->hasParam("color_temp")) {
        auto color_temp = parse_number<float>(request->getParam("color_temp")->value().c_str());
        if (color_temp.has_value()) {
          call.set_color_temperature(*color_temp);
        }
      }
      if (request->hasParam("flash")) {
        auto flash = parse_number<uint32_t>(request->getParam("flash")->value().c_str());
        if (flash.has_value()) {
          call.set_flash_length(*flash * 1000);
        }
      }
      if (request->hasParam("transition")) {
        auto transition = parse_number<uint32_t>(request->getParam("transition")->value().c_str());
        if (transition.has_value()) {
          call.set_transition_length(*transition * 1000);
        }
      }
      if (request->hasParam("effect")) {
        const char *effect = request->getParam("effect")->value().c_str();
        call.set_effect(effect);
      }

      this->schedule_([call]() mutable { call.perform(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      auto call = obj->turn_off();
      if (request->hasParam("transition")) {
        auto transition = parse_number<uint32_t>(request->getParam("transition")->value().c_str());
        if (transition.has_value()) {
          call.set_transition_length(*transition * 1000);
        }
      }
      this->schedule_([call]() mutable { call.perform(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServer::light_json(light::LightState *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "light-" + obj->get_object_id(), start_config);
    root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

    light::LightJSONSchema::dump_json(*obj, root);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("effects");
      opt.add("None");
      for (auto const &option : obj->get_effects()) {
        opt.add(option->get_name());
      }
    }
  });
}
#endif

#ifdef USE_COVER
void WebServer::on_cover_update(cover::Cover *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->cover_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (cover::Cover *obj : App.get_covers()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->cover_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      continue;
    }

    auto call = obj->make_call();
    if (match.method == "open") {
      call.set_command_open();
    } else if (match.method == "close") {
      call.set_command_close();
    } else if (match.method == "stop") {
      call.set_command_stop();
    } else if (match.method == "toggle") {
      call.set_command_toggle();
    } else if (match.method != "set") {
      request->send(404);
      return;
    }

    auto traits = obj->get_traits();
    if ((request->hasParam("position") && !traits.get_supports_position()) ||
        (request->hasParam("tilt") && !traits.get_supports_tilt())) {
      request->send(409);
      return;
    }

    if (request->hasParam("position")) {
      auto position = parse_number<float>(request->getParam("position")->value().c_str());
      if (position.has_value()) {
        call.set_position(*position);
      }
    }
    if (request->hasParam("tilt")) {
      auto tilt = parse_number<float>(request->getParam("tilt")->value().c_str());
      if (tilt.has_value()) {
        call.set_tilt(*tilt);
      }
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::cover_json(cover::Cover *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "cover-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
                              obj->position, start_config);
    root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_position())
      root["position"] = obj->position;
    if (obj->get_traits().get_supports_tilt())
      root["tilt"] = obj->tilt;
  });
}
#endif

#ifdef USE_NUMBER
void WebServer::on_number_update(number::Number *obj, float state) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->number_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_numbers()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->number_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();
    if (request->hasParam("value")) {
      auto value = parse_number<float>(request->getParam("value")->value().c_str());
      if (value.has_value())
        call.set_value(*value);
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::number_json(number::Number *obj, float value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_id(root, obj, "number-" + obj->get_object_id(), start_config);
    if (start_config == DETAIL_ALL) {
      root["min_value"] =
          value_accuracy_to_string(obj->traits.get_min_value(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["max_value"] =
          value_accuracy_to_string(obj->traits.get_max_value(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["step"] =
          value_accuracy_to_string(obj->traits.get_step(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["mode"] = (int) obj->traits.get_mode();
      if (!obj->traits.get_unit_of_measurement().empty())
        root["uom"] = obj->traits.get_unit_of_measurement();
    }
    if (std::isnan(value)) {
      root["value"] = "\"NaN\"";
      root["state"] = "NA";
    } else {
      root["value"] = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      std::string state = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      if (!obj->traits.get_unit_of_measurement().empty())
        state += " " + obj->traits.get_unit_of_measurement();
      root["state"] = state;
    }
  });
}
#endif

#ifdef USE_DATETIME_DATE
void WebServer::on_date_update(datetime::DateEntity *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->date_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_date_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_dates()) {
    if (obj->get_object_id() != match.id)
      continue;
    if (request->method() == HTTP_GET) {
      std::string data = this->date_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (!request->hasParam("value")) {
      request->send(409);
      return;
    }

    if (request->hasParam("value")) {
      std::string value = request->getParam("value")->value().c_str();
      call.set_date(value);
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::date_json(datetime::DateEntity *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "date-" + obj->get_object_id(), start_config);
    std::string value = str_sprintf("%d-%02d-%02d", obj->year, obj->month, obj->day);
    root["value"] = value;
    root["state"] = value;
  });
}
#endif  // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
void WebServer::on_time_update(datetime::TimeEntity *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->time_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_time_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_times()) {
    if (obj->get_object_id() != match.id)
      continue;
    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->time_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (!request->hasParam("value")) {
      request->send(409);
      return;
    }

    if (request->hasParam("value")) {
      std::string value = request->getParam("value")->value().c_str();
      call.set_time(value);
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::time_json(datetime::TimeEntity *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "time-" + obj->get_object_id(), start_config);
    std::string value = str_sprintf("%02d:%02d:%02d", obj->hour, obj->minute, obj->second);
    root["value"] = value;
    root["state"] = value;
  });
}
#endif  // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
void WebServer::on_datetime_update(datetime::DateTimeEntity *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->datetime_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_datetime_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_datetimes()) {
    if (obj->get_object_id() != match.id)
      continue;
    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->datetime_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }
    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (!request->hasParam("value")) {
      request->send(409);
      return;
    }

    if (request->hasParam("value")) {
      std::string value = request->getParam("value")->value().c_str();
      call.set_datetime(value);
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::datetime_json(datetime::DateTimeEntity *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "datetime-" + obj->get_object_id(), start_config);
    std::string value = str_sprintf("%d-%02d-%02d %02d:%02d:%02d", obj->year, obj->month, obj->day, obj->hour,
                                    obj->minute, obj->second);
    root["value"] = value;
    root["state"] = value;
  });
}
#endif  // USE_DATETIME_DATETIME

#ifdef USE_TEXT
void WebServer::on_text_update(text::Text *obj, const std::string &state) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->text_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_text_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_texts()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->text_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "text/json", data.c_str());
      return;
    }
    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();
    if (request->hasParam("value")) {
      String value = request->getParam("value")->value();
      call.set_value(value.c_str());
    }

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::text_json(text::Text *obj, const std::string &value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_id(root, obj, "text-" + obj->get_object_id(), start_config);
    if (start_config == DETAIL_ALL) {
      root["mode"] = (int) obj->traits.get_mode();
    }
    root["min_length"] = obj->traits.get_min_length();
    root["max_length"] = obj->traits.get_max_length();
    root["pattern"] = obj->traits.get_pattern();
    if (obj->traits.get_mode() == text::TextMode::TEXT_MODE_PASSWORD) {
      root["state"] = "********";
    } else {
      root["state"] = value;
    }
    root["value"] = value;
  });
}
#endif

#ifdef USE_SELECT
void WebServer::on_select_update(select::Select *obj, const std::string &state, size_t index) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->select_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_selects()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      auto detail = DETAIL_STATE;
      auto *param = request->getParam("detail");
      if (param && param->value() == "all") {
        detail = DETAIL_ALL;
      }
      std::string data = this->select_json(obj, obj->state, detail);
      request->send(200, "application/json", data.c_str());
      return;
    }

    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (request->hasParam("option")) {
      auto option = request->getParam("option")->value();
      call.set_option(option.c_str());  // NOLINT(clang-diagnostic-deprecated-declarations)
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::select_json(select::Select *obj, const std::string &value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "select-" + obj->get_object_id(), value, value, start_config);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("option");
      for (auto &option : obj->traits.get_options()) {
        opt.add(option);
      }
    }
  });
}
#endif

// Longest: HORIZONTAL
#define PSTR_LOCAL(mode_s) strncpy_P(buf, (PGM_P) ((mode_s)), 15)

#ifdef USE_CLIMATE
void WebServer::on_climate_update(climate::Climate *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->climate_json(obj, DETAIL_STATE).c_str(), "state");
}

void WebServer::handle_climate_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_climates()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->climate_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }

    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (request->hasParam("mode")) {
      auto mode = request->getParam("mode")->value();
      call.set_mode(mode.c_str());
    }

    if (request->hasParam("target_temperature_high")) {
      auto target_temperature_high = parse_number<float>(request->getParam("target_temperature_high")->value().c_str());
      if (target_temperature_high.has_value())
        call.set_target_temperature_high(*target_temperature_high);
    }

    if (request->hasParam("target_temperature_low")) {
      auto target_temperature_low = parse_number<float>(request->getParam("target_temperature_low")->value().c_str());
      if (target_temperature_low.has_value())
        call.set_target_temperature_low(*target_temperature_low);
    }

    if (request->hasParam("target_temperature")) {
      auto target_temperature = parse_number<float>(request->getParam("target_temperature")->value().c_str());
      if (target_temperature.has_value())
        call.set_target_temperature(*target_temperature);
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::climate_json(climate::Climate *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "climate-" + obj->get_object_id(), start_config);
    const auto traits = obj->get_traits();
    int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
    int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
    char buf[16];

    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("modes");
      for (climate::ClimateMode m : traits.get_supported_modes())
        opt.add(PSTR_LOCAL(climate::climate_mode_to_string(m)));
      if (!traits.get_supported_custom_fan_modes().empty()) {
        JsonArray opt = root.createNestedArray("fan_modes");
        for (climate::ClimateFanMode m : traits.get_supported_fan_modes())
          opt.add(PSTR_LOCAL(climate::climate_fan_mode_to_string(m)));
      }

      if (!traits.get_supported_custom_fan_modes().empty()) {
        JsonArray opt = root.createNestedArray("custom_fan_modes");
        for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
          opt.add(custom_fan_mode);
      }
      if (traits.get_supports_swing_modes()) {
        JsonArray opt = root.createNestedArray("swing_modes");
        for (auto swing_mode : traits.get_supported_swing_modes())
          opt.add(PSTR_LOCAL(climate::climate_swing_mode_to_string(swing_mode)));
      }
      if (traits.get_supports_presets() && obj->preset.has_value()) {
        JsonArray opt = root.createNestedArray("presets");
        for (climate::ClimatePreset m : traits.get_supported_presets())
          opt.add(PSTR_LOCAL(climate::climate_preset_to_string(m)));
      }
      if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
        JsonArray opt = root.createNestedArray("custom_presets");
        for (auto const &custom_preset : traits.get_supported_custom_presets())
          opt.add(custom_preset);
      }
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
  });
}
#endif

#ifdef USE_LOCK
void WebServer::on_lock_update(lock::Lock *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->lock_json(obj, obj->state, DETAIL_STATE).c_str(), "state");
}
std::string WebServer::lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "lock-" + obj->get_object_id(), lock::lock_state_to_string(value), value,
                              start_config);
  });
}
void WebServer::handle_lock_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (lock::Lock *obj : App.get_locks()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->lock_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "lock") {
      this->schedule_([obj]() { obj->lock(); });
      request->send(200);
    } else if (match.method == "unlock") {
      this->schedule_([obj]() { obj->unlock(); });
      request->send(200);
    } else if (match.method == "open") {
      this->schedule_([obj]() { obj->open(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_VALVE
void WebServer::on_valve_update(valve::Valve *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->valve_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_valve_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (valve::Valve *obj : App.get_valves()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->valve_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      continue;
    }

    auto call = obj->make_call();
    if (match.method == "open") {
      call.set_command_open();
    } else if (match.method == "close") {
      call.set_command_close();
    } else if (match.method == "stop") {
      call.set_command_stop();
    } else if (match.method == "toggle") {
      call.set_command_toggle();
    } else if (match.method != "set") {
      request->send(404);
      return;
    }

    auto traits = obj->get_traits();
    if (request->hasParam("position") && !traits.get_supports_position()) {
      request->send(409);
      return;
    }

    if (request->hasParam("position")) {
      auto position = parse_number<float>(request->getParam("position")->value().c_str());
      if (position.has_value()) {
        call.set_position(*position);
      }
    }

    this->schedule_([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::valve_json(valve::Valve *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "valve-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
                              obj->position, start_config);
    root["current_operation"] = valve::valve_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_position())
      root["position"] = obj->position;
  });
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
void WebServer::on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) {
  if (this->events_.count() == 0)
    return;
  this->events_.send(this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str(), "state");
}
std::string WebServer::alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                                alarm_control_panel::AlarmControlPanelState value,
                                                JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    char buf[16];
    set_json_icon_state_value(root, obj, "alarm-control-panel-" + obj->get_object_id(),
                              PSTR_LOCAL(alarm_control_panel_state_to_string(value)), value, start_config);
  });
}
void WebServer::handle_alarm_control_panel_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (alarm_control_panel::AlarmControlPanel *obj : App.get_alarm_control_panels()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET && match.method.empty()) {
      std::string data = this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  request->send(404);
}
#endif

#ifdef USE_EVENT
void WebServer::on_event(event::Event *obj, const std::string &event_type) {
  this->events_.send(this->event_json(obj, event_type, DETAIL_STATE).c_str(), "state");
}

std::string WebServer::event_json(event::Event *obj, const std::string &event_type, JsonDetail start_config) {
  return json::build_json([obj, event_type, start_config](JsonObject root) {
    set_json_id(root, obj, "event-" + obj->get_object_id(), start_config);
    if (!event_type.empty()) {
      root["event_type"] = event_type;
    }
    if (start_config == DETAIL_ALL) {
      JsonArray event_types = root.createNestedArray("event_types");
      for (auto const &event_type : obj->get_event_types()) {
        event_types.add(event_type);
      }
      root["device_class"] = obj->get_device_class();
    }
  });
}
#endif

bool WebServer::canHandle(AsyncWebServerRequest *request) {
  if (request->url() == "/")
    return true;

#ifdef USE_WEBSERVER_CSS_INCLUDE
  if (request->url() == "/0.css")
    return true;
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
  if (request->url() == "/0.js")
    return true;
#endif

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
  if (request->method() == HTTP_OPTIONS && request->hasHeader(HEADER_CORS_REQ_PNA)) {
#ifdef USE_ARDUINO
    // Header needs to be added to interesting header list for it to not be
    // nuked by the time we handle the request later.
    // Only required in Arduino framework.
    request->addInterestingHeader(HEADER_CORS_REQ_PNA);
#endif
    return true;
  }
#endif

  UrlMatch match = match_url(request->url().c_str(), true);
  if (!match.valid)
    return false;
#ifdef USE_SENSOR
  if (request->method() == HTTP_GET && match.domain == "sensor")
    return true;
#endif

#ifdef USE_SWITCH
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "switch")
    return true;
#endif

#ifdef USE_BUTTON
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "button")
    return true;
#endif

#ifdef USE_BINARY_SENSOR
  if (request->method() == HTTP_GET && match.domain == "binary_sensor")
    return true;
#endif

#ifdef USE_FAN
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "fan")
    return true;
#endif

#ifdef USE_LIGHT
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "light")
    return true;
#endif

#ifdef USE_TEXT_SENSOR
  if (request->method() == HTTP_GET && match.domain == "text_sensor")
    return true;
#endif

#ifdef USE_COVER
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "cover")
    return true;
#endif

#ifdef USE_NUMBER
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "number")
    return true;
#endif

#ifdef USE_DATETIME_DATE
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "date")
    return true;
#endif

#ifdef USE_DATETIME_TIME
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "time")
    return true;
#endif

#ifdef USE_DATETIME_DATETIME
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "datetime")
    return true;
#endif

#ifdef USE_TEXT
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "text")
    return true;
#endif

#ifdef USE_SELECT
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "select")
    return true;
#endif

#ifdef USE_CLIMATE
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "climate")
    return true;
#endif

#ifdef USE_LOCK
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "lock")
    return true;
#endif

#ifdef USE_VALVE
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "valve")
    return true;
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  if (request->method() == HTTP_GET && match.domain == "alarm_control_panel")
    return true;
#endif

  return false;
}
void WebServer::handleRequest(AsyncWebServerRequest *request) {
  if (request->url() == "/") {
    this->handle_index_request(request);
    return;
  }

#ifdef USE_WEBSERVER_CSS_INCLUDE
  if (request->url() == "/0.css") {
    this->handle_css_request(request);
    return;
  }
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
  if (request->url() == "/0.js") {
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

  UrlMatch match = match_url(request->url().c_str());
#ifdef USE_SENSOR
  if (match.domain == "sensor") {
    this->handle_sensor_request(request, match);
    return;
  }
#endif

#ifdef USE_SWITCH
  if (match.domain == "switch") {
    this->handle_switch_request(request, match);
    return;
  }
#endif

#ifdef USE_BUTTON
  if (match.domain == "button") {
    this->handle_button_request(request, match);
    return;
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (match.domain == "binary_sensor") {
    this->handle_binary_sensor_request(request, match);
    return;
  }
#endif

#ifdef USE_FAN
  if (match.domain == "fan") {
    this->handle_fan_request(request, match);
    return;
  }
#endif

#ifdef USE_LIGHT
  if (match.domain == "light") {
    this->handle_light_request(request, match);
    return;
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (match.domain == "text_sensor") {
    this->handle_text_sensor_request(request, match);
    return;
  }
#endif

#ifdef USE_COVER
  if (match.domain == "cover") {
    this->handle_cover_request(request, match);
    return;
  }
#endif

#ifdef USE_NUMBER
  if (match.domain == "number") {
    this->handle_number_request(request, match);
    return;
  }
#endif

#ifdef USE_DATETIME_DATE
  if (match.domain == "date") {
    this->handle_date_request(request, match);
    return;
  }
#endif

#ifdef USE_DATETIME_TIME
  if (match.domain == "time") {
    this->handle_time_request(request, match);
    return;
  }
#endif

#ifdef USE_DATETIME_DATETIME
  if (match.domain == "datetime") {
    this->handle_datetime_request(request, match);
    return;
  }
#endif

#ifdef USE_TEXT
  if (match.domain == "text") {
    this->handle_text_request(request, match);
    return;
  }
#endif

#ifdef USE_SELECT
  if (match.domain == "select") {
    this->handle_select_request(request, match);
    return;
  }
#endif

#ifdef USE_CLIMATE
  if (match.domain == "climate") {
    this->handle_climate_request(request, match);
    return;
  }
#endif

#ifdef USE_LOCK
  if (match.domain == "lock") {
    this->handle_lock_request(request, match);

    return;
  }
#endif

#ifdef USE_VALVE
  if (match.domain == "valve") {
    this->handle_valve_request(request, match);
    return;
  }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  if (match.domain == "alarm_control_panel") {
    this->handle_alarm_control_panel_request(request, match);

    return;
  }
#endif
}

bool WebServer::isRequestHandlerTrivial() { return false; }

void WebServer::schedule_(std::function<void()> &&f) {
#ifdef USE_ESP32
  xSemaphoreTake(this->to_schedule_lock_, portMAX_DELAY);
  to_schedule_.push_back(std::move(f));
  xSemaphoreGive(this->to_schedule_lock_);
#else
  this->defer(std::move(f));
#endif
}

}  // namespace web_server
}  // namespace esphome
