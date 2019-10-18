#include "web_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/components/json/json_util.h"

#include "StreamString.h"

#include <cstdlib>

#ifdef USE_LOGGER
#include <esphome/components/logger/logger.h>
#endif

namespace esphome {
namespace web_server {

static const char *TAG = "web_server";

void write_row(AsyncResponseStream *stream, Nameable *obj, const std::string &klass, const std::string &action) {
  stream->print("<tr class=\"");
  stream->print(klass.c_str());
  stream->print("\" id=\"");
  stream->print(klass.c_str());
  stream->print("-");
  stream->print(obj->get_object_id().c_str());
  stream->print("\"><td>");
  stream->print(obj->get_name().c_str());
  stream->print("</td><td></td><td>");
  stream->print(action.c_str());
  stream->print("</td>");
}

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
void WebServer::set_js_url(const char *js_url) { this->js_url_ = js_url; }

void WebServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up web server...");
  this->base_->init();

  this->events_.onConnect([this](AsyncEventSourceClient *client) {
    // Configure reconnect timeout
    client->send("", "ping", millis(), 30000);

#ifdef USE_SENSOR
    for (auto *obj : App.get_sensors())
      if (!obj->is_internal())
        client->send(this->sensor_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_SWITCH
    for (auto *obj : App.get_switches())
      if (!obj->is_internal())
        client->send(this->switch_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_BINARY_SENSOR
    for (auto *obj : App.get_binary_sensors())
      if (!obj->is_internal())
        client->send(this->binary_sensor_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_FAN
    for (auto *obj : App.get_fans())
      if (!obj->is_internal())
        client->send(this->fan_json(obj).c_str(), "state");
#endif

#ifdef USE_LIGHT
    for (auto *obj : App.get_lights())
      if (!obj->is_internal())
        client->send(this->light_json(obj).c_str(), "state");
#endif

#ifdef USE_TEXT_SENSOR
    for (auto *obj : App.get_text_sensors())
      if (!obj->is_internal())
        client->send(this->text_sensor_json(obj, obj->state).c_str(), "state");
#endif
  });

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr)
    logger::global_logger->add_on_log_callback(
        [this](int level, const char *tag, const char *message) { this->events_.send(message, "log", millis()); });
#endif
  this->base_->add_handler(&this->events_);
  this->base_->add_handler(this);
  this->base_->add_ota_handler();

  this->set_interval(10000, [this]() { this->events_.send("", "ping", millis(), 30000); });
}
void WebServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Web Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network_get_address().c_str(), this->base_->get_port());
  if (this->using_auth()) {
    ESP_LOGCONFIG(TAG, "  Basic authentication enabled");
  }
}
float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

void WebServer::handle_index_request(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/html");
  std::string title = App.get_name() + " Web Server";
  stream->print(F("<!DOCTYPE html><html><head><meta charset=UTF-8><title>"));
  stream->print(title.c_str());
  stream->print(F("</title><link rel=\"stylesheet\" href=\""));
  stream->print(this->css_url_);
  stream->print(F("\"></head><body><article class=\"markdown-body\"><h1>"));
  stream->print(title.c_str());
  stream->print(F("</h1><h2>States</h2><table id=\"states\"><thead><tr><th>Name<th>State<th>Actions<tbody>"));

#ifdef USE_SENSOR
  for (auto *obj : App.get_sensors())
    if (!obj->is_internal())
      write_row(stream, obj, "sensor", "");
#endif

#ifdef USE_SWITCH
  for (auto *obj : App.get_switches())
    if (!obj->is_internal())
      write_row(stream, obj, "switch", "<button>Toggle</button>");
#endif

#ifdef USE_BINARY_SENSOR
  for (auto *obj : App.get_binary_sensors())
    if (!obj->is_internal())
      write_row(stream, obj, "binary_sensor", "");
#endif

#ifdef USE_FAN
  for (auto *obj : App.get_fans())
    if (!obj->is_internal())
      write_row(stream, obj, "fan", "<button>Toggle</button>");
#endif

#ifdef USE_LIGHT
  for (auto *obj : App.get_lights())
    if (!obj->is_internal())
      write_row(stream, obj, "light", "<button>Toggle</button>");
#endif

#ifdef USE_TEXT_SENSOR
  for (auto *obj : App.get_text_sensors())
    if (!obj->is_internal())
      write_row(stream, obj, "text_sensor", "");
#endif

  stream->print(F("</tbody></table><p>See <a href=\"https://esphome.io/web-api/index.html\">ESPHome Web API</a> for "
                  "REST API documentation.</p>"
                  "<h2>OTA Update</h2><form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\"><input "
                  "type=\"file\" name=\"update\"><input type=\"submit\" value=\"Update\"></form>"
                  "<h2>Debug Log</h2><pre id=\"log\"></pre>"
                  "<script src=\""));
  stream->print(this->js_url_);
  stream->print(F("\"></script></article></body></html>"));

  request->send(stream);
}

#ifdef USE_SENSOR
void WebServer::on_sensor_update(sensor::Sensor *obj, float state) {
  this->events_.send(this->sensor_json(obj, state).c_str(), "state");
}
void WebServer::handle_sensor_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (sensor::Sensor *obj : App.get_sensors()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->sensor_json(obj, obj->state);
    request->send(200, "text/json", data.c_str());
    return;
  }
  request->send(404);
}
std::string WebServer::sensor_json(sensor::Sensor *obj, float value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "sensor-" + obj->get_object_id();
    std::string state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
    if (!obj->get_unit_of_measurement().empty())
      state += " " + obj->get_unit_of_measurement();
    root["state"] = state;
    root["value"] = value;
  });
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj, std::string state) {
  this->events_.send(this->text_sensor_json(obj, state).c_str(), "state");
}
void WebServer::handle_text_sensor_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (text_sensor::TextSensor *obj : App.get_text_sensors()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->text_sensor_json(obj, obj->state);
    request->send(200, "text/json", data.c_str());
    return;
  }
  request->send(404);
}
std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "text_sensor-" + obj->get_object_id();
    root["state"] = value;
    root["value"] = value;
  });
}
#endif

#ifdef USE_SWITCH
void WebServer::on_switch_update(switch_::Switch *obj, bool state) {
  this->events_.send(this->switch_json(obj, state).c_str(), "state");
}
std::string WebServer::switch_json(switch_::Switch *obj, bool value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "switch-" + obj->get_object_id();
    root["state"] = value ? "ON" : "OFF";
    root["value"] = value;
  });
}
void WebServer::handle_switch_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (switch_::Switch *obj : App.get_switches()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->switch_json(obj, obj->state);
      request->send(200, "text/json", data.c_str());
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

#ifdef USE_BINARY_SENSOR
void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  if (obj->is_internal())
    return;
  this->events_.send(this->binary_sensor_json(obj, state).c_str(), "state");
}
std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "binary_sensor-" + obj->get_object_id();
    root["state"] = value ? "ON" : "OFF";
    root["value"] = value;
  });
}
void WebServer::handle_binary_sensor_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (binary_sensor::BinarySensor *obj : App.get_binary_sensors()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->binary_sensor_json(obj, obj->state);
    request->send(200, "text/json", data.c_str());
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_FAN
void WebServer::on_fan_update(fan::FanState *obj) {
  if (obj->is_internal())
    return;
  this->events_.send(this->fan_json(obj).c_str(), "state");
}
std::string WebServer::fan_json(fan::FanState *obj) {
  return json::build_json([obj](JsonObject &root) {
    root["id"] = "fan-" + obj->get_object_id();
    root["state"] = obj->state ? "ON" : "OFF";
    root["value"] = obj->state;
    if (obj->get_traits().supports_speed()) {
      switch (obj->speed) {
        case fan::FAN_SPEED_LOW:
          root["speed"] = "low";
          break;
        case fan::FAN_SPEED_MEDIUM:
          root["speed"] = "medium";
          break;
        case fan::FAN_SPEED_HIGH:
          root["speed"] = "high";
          break;
      }
    }
    if (obj->get_traits().supports_oscillation())
      root["oscillation"] = obj->oscillating;
  });
}
void WebServer::handle_fan_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (fan::FanState *obj : App.get_fans()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->fan_json(obj);
      request->send(200, "text/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("speed")) {
        String speed = request->getParam("speed")->value();
        call.set_speed(speed.c_str());
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
      this->defer([call]() { call.perform(); });
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
  if (obj->is_internal())
    return;
  this->events_.send(this->light_json(obj).c_str(), "state");
}
void WebServer::handle_light_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (light::LightState *obj : App.get_lights()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->light_json(obj);
      request->send(200, "text/json", data.c_str());
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

      if (request->hasParam("flash"))
        call.set_flash_length((uint32_t) request->getParam("flash")->value().toFloat() * 1000);

      if (request->hasParam("transition"))
        call.set_transition_length((uint32_t) request->getParam("transition")->value().toFloat() * 1000);

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
std::string WebServer::light_json(light::LightState *obj) {
  return json::build_json([obj](JsonObject &root) {
    root["id"] = "light-" + obj->get_object_id();
    root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";
    obj->dump_json(root);
  });
}
#endif

bool WebServer::canHandle(AsyncWebServerRequest *request) {
  if (request->url() == "/")
    return true;

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

  return false;
}
void WebServer::handleRequest(AsyncWebServerRequest *request) {
  if (this->using_auth() && !request->authenticate(this->username_, this->password_)) {
    return request->requestAuthentication();
  }

  if (request->url() == "/") {
    this->handle_index_request(request);
    return;
  }

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
}

bool WebServer::isRequestHandlerTrivial() { return false; }

}  // namespace web_server
}  // namespace esphome
