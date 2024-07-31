#include "web_server.h"
#include "esphome/core/application.h"

#if USE_WEBSERVER_VERSION == 1

namespace esphome {
namespace web_server {

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

void WebServer::set_css_url(const char *css_url) { this->css_url_ = css_url; }

void WebServer::set_js_url(const char *js_url) { this->js_url_ = js_url; }

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

}  // namespace web_server
}  // namespace esphome
#endif
