#ifdef USE_ARDUINO

#include "web_server.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/util.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"

#include "StreamString.h"

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

void WebServer::set_css_url(const char *css_url) { this->css_url_ = css_url; }
void WebServer::set_css_include(const char *css_include) { this->css_include_ = css_include; }
void WebServer::set_js_url(const char *js_url) { this->js_url_ = js_url; }
void WebServer::set_js_include(const char *js_include) { this->js_include_ = js_include; }

void WebServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up web server...");
  this->setup_controller(this->include_internal_);
  this->base_->init();

  this->events_.onConnect([this](AsyncEventSourceClient *client) {
    // Configure reconnect timeout and send config

    client->send(json::build_json([this](JsonObject root) {
                   root["title"] = App.get_name();
                   root["ota"] = this->allow_ota_;
                   root["lang"] = "en";
                 }).c_str(),
                 "ping", millis(), 30000);

    this->entities_iterator_.begin(this->include_internal_);
  });

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr) {
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
void WebServer::loop() { this->entities_iterator_.advance(); }
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
#else
void WebServer::handle_index_request(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/html");
  // All content is controlled and created by user - so allowing all origins is fine here.
  stream->addHeader("Access-Control-Allow-Origin", "*");
#if USE_WEBSERVER_VERSION == 1
  const std::string &title = App.get_name();
  stream->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                  "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\"><title>"));
  stream->print(title.c_str());
  stream->print(F("</title>"));
#else
  stream->print(F("<!DOCTYPE html><html><head><meta charset=UTF-8><link rel=icon href=data:>"));
#endif
#ifdef USE_WEBSERVER_CSS_INCLUDE
  stream->print(F("<link rel=\"stylesheet\" href=\"/0.css\">"));
#endif
  if (strlen(this->css_url_) > 0) {
    stream->print(F("<link rel=\"stylesheet\" href=\""));
    stream->print(this->css_url_);
    stream->print(F("\">"));
  }
  stream->print(F("</head><body>"));
#if USE_WEBSERVER_VERSION == 1
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
#endif
#ifdef USE_WEBSERVER_JS_INCLUDE
  if (this->js_include_ != nullptr) {
    stream->print(F("<script type=\"module\" src=\"/0.js\"></script>"));
  }
#endif
#if USE_WEBSERVER_VERSION == 2
  stream->print(F("<esp-app></esp-app>"));
#endif
  if (strlen(this->js_url_) > 0) {
    stream->print(F("<script src=\""));
    stream->print(this->js_url_);
    stream->print(F("\"></script>"));
  }
#if USE_WEBSERVER_VERSION == 1
  stream->print(F("</article></body></html>"));
#else
  stream->print(F("</body></html>"));
#endif

  request->send(stream);
}
#endif
#ifdef USE_WEBSERVER_CSS_INCLUDE
void WebServer::handle_css_request(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/css");
  if (this->css_include_ != nullptr) {
    stream->print(this->css_include_);
  }

  request->send(stream);
}
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::handle_js_request(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/javascript");
  if (this->js_include_ != nullptr) {
    stream->addHeader("Access-Control-Allow-Origin", "*");
    stream->print(this->js_include_);
  }

  request->send(stream);
}
#endif

#define set_json_id(root, obj, sensor, start_config) \
  (root)["id"] = sensor; \
  if (((start_config) == DETAIL_ALL)) \
    (root)["name"] = (obj)->get_name();

#define set_json_value(root, obj, sensor, value, start_config) \
  set_json_id((root), (obj), sensor, start_config)(root)["value"] = value;

#define set_json_state_value(root, obj, sensor, state, value, start_config) \
  set_json_value(root, obj, sensor, value, start_config)(root)["state"] = state;

#define set_json_icon_state_value(root, obj, sensor, state, value, start_config) \
  set_json_value(root, obj, sensor, value, start_config)(root)["state"] = state; \
  if (((start_config) == DETAIL_ALL)) \
    (root)["icon"] = (obj)->get_icon();

#ifdef USE_SENSOR
void WebServer::on_sensor_update(sensor::Sensor *obj, float state) {
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
    if (isnan(value)) {
      state = "NA";
    } else {
      state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
      if (!obj->get_unit_of_measurement().empty())
        state += " " + obj->get_unit_of_measurement();
    }
    set_json_icon_state_value(root, obj, "sensor-" + obj->get_object_id(), state, value, start_config);
  });
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
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
  this->events_.send(this->switch_json(obj, state, DETAIL_STATE).c_str(), "state");
}
std::string WebServer::switch_json(switch_::Switch *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "switch-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
  });
}
void WebServer::handle_switch_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (switch_::Switch *obj : App.get_switches()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->switch_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      this->defer([obj]() { obj->turn_on(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      this->defer([obj]() { obj->turn_off(); });
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
    if (request->method() == HTTP_POST && match.method == "press") {
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
#endif

#ifdef USE_BINARY_SENSOR
void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  this->events_.send(this->binary_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
}
std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_state_value(root, obj, "binary_sensor-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
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
void WebServer::on_fan_update(fan::Fan *obj) { this->events_.send(this->fan_json(obj, DETAIL_STATE).c_str(), "state"); }
std::string WebServer::fan_json(fan::Fan *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_state_value(root, obj, "fan-" + obj->get_object_id(), obj->state ? "ON" : "OFF", obj->state, start_config);
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

    if (request->method() == HTTP_GET) {
      std::string data = this->fan_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("speed")) {
        String speed = request->getParam("speed")->value();
      }
      if (request->hasParam("speed_level")) {
        String speed_level = request->getParam("speed_level")->value();
        auto val = parse_number<int>(speed_level.c_str());
        if (!val.has_value()) {
          ESP_LOGW(TAG, "Can't convert '%s' to number!", speed_level.c_str());
          return;
        }
        call.set_speed(*val);
      }
      if (request->hasParam("oscillation")) {
        String speed = request->getParam("oscillation")->value();
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
    } else if (match.method == "turn_off") {
      this->defer([obj]() { obj->turn_off().perform(); });
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
  this->events_.send(this->light_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_light_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (light::LightState *obj : App.get_lights()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->light_json(obj, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("brightness"))
        call.set_brightness(request->getParam("brightness")->value().toFloat() / 255.0f);
      if (request->hasParam("r"))
        call.set_red(request->getParam("r")->value().toFloat() / 255.0f);
      if (request->hasParam("g"))
        call.set_green(request->getParam("g")->value().toFloat() / 255.0f);
      if (request->hasParam("b"))
        call.set_blue(request->getParam("b")->value().toFloat() / 255.0f);
      if (request->hasParam("white_value"))
        call.set_white(request->getParam("white_value")->value().toFloat() / 255.0f);
      if (request->hasParam("color_temp"))
        call.set_color_temperature(request->getParam("color_temp")->value().toFloat());

      if (request->hasParam("flash")) {
        float length_s = request->getParam("flash")->value().toFloat();
        call.set_flash_length(static_cast<uint32_t>(length_s * 1000));
      }

      if (request->hasParam("transition")) {
        float length_s = request->getParam("transition")->value().toFloat();
        call.set_transition_length(static_cast<uint32_t>(length_s * 1000));
      }

      if (request->hasParam("effect")) {
        const char *effect = request->getParam("effect")->value().c_str();
        call.set_effect(effect);
      }

      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      auto call = obj->turn_off();
      if (request->hasParam("transition")) {
        auto length = (uint32_t) request->getParam("transition")->value().toFloat() * 1000;
        call.set_transition_length(length);
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
  this->events_.send(this->cover_json(obj, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_cover_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (cover::Cover *obj : App.get_covers()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
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

    if (request->hasParam("position"))
      call.set_position(request->getParam("position")->value().toFloat());
    if (request->hasParam("tilt"))
      call.set_tilt(request->getParam("tilt")->value().toFloat());

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::cover_json(cover::Cover *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_state_value(root, obj, "cover-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
                         obj->position, start_config);
    root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_tilt())
      root["tilt"] = obj->tilt;
  });
}
#endif

#ifdef USE_NUMBER
void WebServer::on_number_update(number::Number *obj, float state) {
  this->events_.send(this->number_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_number_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_numbers()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
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
      String value = request->getParam("value")->value();
      optional<float> value_f = parse_number<float>(value.c_str());
      if (value_f.has_value())
        call.set_value(*value_f);
    }

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

std::string WebServer::number_json(number::Number *obj, float value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_id(root, obj, "number-" + obj->get_object_id(), start_config);
    if (start_config == DETAIL_ALL) {
      root["min_value"] = obj->traits.get_min_value();
      root["max_value"] = obj->traits.get_max_value();
      root["step"] = obj->traits.get_step();
      root["mode"] = (int) obj->traits.get_mode();
    }
    if (isnan(value)) {
      root["value"] = "\"NaN\"";
      root["state"] = "NA";
    } else {
      root["value"] = value;
      std::string state = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      if (!obj->traits.get_unit_of_measurement().empty())
        state += " " + obj->traits.get_unit_of_measurement();
      root["state"] = state;
    }
  });
}
#endif

#ifdef USE_SELECT
void WebServer::on_select_update(select::Select *obj, const std::string &state, size_t index) {
  this->events_.send(this->select_json(obj, state, DETAIL_STATE).c_str(), "state");
}
void WebServer::handle_select_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_selects()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->select_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
      return;
    }

    if (match.method != "set") {
      request->send(404);
      return;
    }

    auto call = obj->make_call();

    if (request->hasParam("option")) {
      String option = request->getParam("option")->value();
      call.set_option(option.c_str());  // NOLINT(clang-diagnostic-deprecated-declarations)
    }

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServer::select_json(select::Select *obj, const std::string &value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_state_value(root, obj, "select-" + obj->get_object_id(), value, value, start_config);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("option");
      for (auto &option : obj->traits.get_options()) {
        opt.add(option);
      }
    }
  });
}
#endif

#ifdef USE_CLIMATE
void WebServer::on_climate_update(climate::Climate *obj) {
  this->events_.send(this->climate_json(obj, DETAIL_STATE).c_str(), "state");
}

void WebServer::handle_climate_request(AsyncWebServerRequest *request, const UrlMatch &match) {
  for (auto *obj : App.get_climates()) {
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
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
      String mode = request->getParam("mode")->value();
      call.set_mode(mode.c_str());
    }

    if (request->hasParam("target_temperature_high")) {
      String value = request->getParam("target_temperature_high")->value();
      optional<float> value_f = parse_number<float>(value.c_str());
      if (value_f.has_value())
        call.set_target_temperature_high(*value_f);
    }

    if (request->hasParam("target_temperature_low")) {
      String value = request->getParam("target_temperature_low")->value();
      optional<float> value_f = parse_number<float>(value.c_str());
      if (value_f.has_value())
        call.set_target_temperature_low(*value_f);
    }

    if (request->hasParam("target_temperature")) {
      String value = request->getParam("target_temperature")->value();
      optional<float> value_f = parse_number<float>(value.c_str());
      if (value_f.has_value())
        call.set_target_temperature(*value_f);
    }

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}

// Longest: HORIZONTAL
#define PSTR_LOCAL(mode_s) strncpy_P(__buf, (PGM_P)((mode_s)), 15)

std::string WebServer::climate_json(climate::Climate *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "climate-" + obj->get_object_id(), start_config);
    const auto traits = obj->get_traits();
    int8_t accuracy = traits.get_temperature_accuracy_decimals();
    char __buf[16];

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
    root["max_temp"] = value_accuracy_to_string(traits.get_visual_max_temperature(), accuracy);
    root["min_temp"] = value_accuracy_to_string(traits.get_visual_min_temperature(), accuracy);
    root["step"] = traits.get_visual_temperature_step();
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
        root["current_temperature"] = value_accuracy_to_string(obj->current_temperature, accuracy);
      } else {
        root["current_temperature"] = "NA";
      }
    }
    if (traits.get_supports_two_point_target_temperature()) {
      root["target_temperature_low"] = value_accuracy_to_string(obj->target_temperature_low, accuracy);
      root["target_temperature_high"] = value_accuracy_to_string(obj->target_temperature_high, accuracy);
      if (!has_state) {
        root["state"] =
            value_accuracy_to_string((obj->target_temperature_high + obj->target_temperature_low) / 2.0f, accuracy);
      }
    } else {
      root["target_temperature"] = value_accuracy_to_string(obj->target_temperature, accuracy);
      if (!has_state)
        root["state"] = root["target_temperature"];
    }
  });
}
#endif

#ifdef USE_LOCK
void WebServer::on_lock_update(lock::Lock *obj) {
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

    if (request->method() == HTTP_GET) {
      std::string data = this->lock_json(obj, obj->state, DETAIL_STATE);
      request->send(200, "application/json", data.c_str());
    } else if (match.method == "lock") {
      this->defer([obj]() { obj->lock(); });
      request->send(200);
    } else if (match.method == "unlock") {
      this->defer([obj]() { obj->unlock(); });
      request->send(200);
    } else if (match.method == "open") {
      this->defer([obj]() { obj->open(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
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
  if (request->method() == HTTP_POST && match.domain == "button")
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
}

bool WebServer::isRequestHandlerTrivial() { return false; }

}  // namespace web_server
}  // namespace esphome

#endif  // USE_ARDUINO
