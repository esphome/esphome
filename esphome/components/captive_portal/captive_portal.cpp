#ifdef USE_ARDUINO

#include "captive_portal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace captive_portal {

static const char *const TAG = "captive_portal";

void CaptivePortal::handle_index(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/html");
  stream->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" "
                  "content=\"width=device-width,initial-scale=1,user-scalable=no\"/><title>"));
  stream->print(App.get_name().c_str());
  stream->print(F("</title><link rel=\"stylesheet\" href=\"/stylesheet.css\">"));
  stream->print(F("<script>function c(l){document.getElementById('ssid').value=l.innerText||l.textContent; "
                  "document.getElementById('psk').focus();}</script>"));
  stream->print(F("</head>"));
  stream->print(F("<body><div class=\"main\"><h1>WiFi Networks</h1>"));

  if (request->hasArg("save")) {
    stream->print(F("<div class=\"info\">The ESP will now try to connect to the network...<br/>Please give it some "
                    "time to connect.<br/>Note: Copy the changed network to your YAML file - the next OTA update will "
                    "overwrite these settings.</div>"));
  }

  for (auto &scan : wifi::global_wifi_component->get_scan_result()) {
    if (scan.get_is_hidden())
      continue;

    stream->print(F("<div class=\"network\" onclick=\"c(this)\"><a href=\"#\" class=\"network-left\">"));

    if (scan.get_rssi() >= -50) {
      stream->print(F("<img src=\"/wifi-strength-4.svg\">"));
    } else if (scan.get_rssi() >= -65) {
      stream->print(F("<img src=\"/wifi-strength-3.svg\">"));
    } else if (scan.get_rssi() >= -85) {
      stream->print(F("<img src=\"/wifi-strength-2.svg\">"));
    } else {
      stream->print(F("<img src=\"/wifi-strength-1.svg\">"));
    }

    stream->print(F("<span class=\"network-ssid\">"));
    stream->print(scan.get_ssid().c_str());
    stream->print(F("</span></a>"));
    if (scan.get_with_auth()) {
      stream->print(F("<img src=\"/lock.svg\">"));
    }
    stream->print(F("</div>"));
  }

  stream->print(F("<h3>WiFi Settings</h3><form method=\"GET\" action=\"/wifisave\"><input id=\"ssid\" name=\"ssid\" "
                  "length=32 placeholder=\"SSID\"><br/><input id=\"psk\" name=\"psk\" length=64 type=\"password\" "
                  "placeholder=\"Password\"><br/><br/><button type=\"submit\">Save</button></form><br><hr><br>"));
  stream->print(F("<h1>OTA Update</h1><form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\"><input "
                  "type=\"file\" name=\"update\"><button type=\"submit\">Update</button></form>"));
  stream->print(F("</div></body></html>"));
  request->send(stream);
}
void CaptivePortal::handle_wifisave(AsyncWebServerRequest *request) {
  std::string ssid = request->arg("ssid").c_str();
  std::string psk = request->arg("psk").c_str();
  ESP_LOGI(TAG, "Captive Portal Requested WiFi Settings Change:");
  ESP_LOGI(TAG, "  SSID='%s'", ssid.c_str());
  ESP_LOGI(TAG, "  Password=" LOG_SECRET("'%s'"), psk.c_str());
  wifi::global_wifi_component->save_wifi_sta(ssid, psk);
  wifi::global_wifi_component->start_scanning();
  request->redirect("/?save=true");
}

void CaptivePortal::setup() {}
void CaptivePortal::start() {
  this->base_->init();
  if (!this->initialized_) {
    this->base_->add_handler(this);
    this->base_->add_ota_handler();
  }

  this->dns_server_ = make_unique<DNSServer>();
  this->dns_server_->setErrorReplyCode(DNSReplyCode::NoError);
  network::IPAddress ip = wifi::global_wifi_component->wifi_soft_ap_ip();
  this->dns_server_->start(53, "*", (uint32_t) ip);

  this->base_->get_server()->onNotFound([this](AsyncWebServerRequest *req) {
    if (!this->active_ || req->host().c_str() == wifi::global_wifi_component->wifi_soft_ap_ip().str()) {
      req->send(404, "text/html", "File not found");
      return;
    }

    auto url = "http://" + wifi::global_wifi_component->wifi_soft_ap_ip().str();
    req->redirect(url.c_str());
  });

  this->initialized_ = true;
  this->active_ = true;
}

const char STYLESHEET_CSS[] PROGMEM =
    R"(*{box-sizing:inherit}div,input{padding:5px;font-size:1em}input{width:95%}body{text-align:center;font-family:sans-serif}button{border:0;border-radius:.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;padding:0}.main{text-align:left;display:inline-block;min-width:260px}.network{display:flex;justify-content:space-between;align-items:center}.network-left{display:flex;align-items:center}.network-ssid{margin-bottom:-7px;margin-left:10px}.info{border:1px solid;margin:10px 0;padding:15px 10px;color:#4f8a10;background-color:#dff2bf})";
const char LOCK_SVG[] PROGMEM =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24"><path d="M12 17a2 2 0 0 0 2-2 2 2 0 0 0-2-2 2 2 0 0 0-2 2 2 2 0 0 0 2 2m6-9a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V10a2 2 0 0 1 2-2h1V6a5 5 0 0 1 5-5 5 5 0 0 1 5 5v2h1m-6-5a3 3 0 0 0-3 3v2h6V6a3 3 0 0 0-3-3z"/></svg>)";

void CaptivePortal::handleRequest(AsyncWebServerRequest *req) {
  if (req->url() == "/") {
    this->handle_index(req);
    return;
  } else if (req->url() == "/wifisave") {
    this->handle_wifisave(req);
    return;
  } else if (req->url() == "/stylesheet.css") {
    req->send_P(200, "text/css", STYLESHEET_CSS);
    return;
  } else if (req->url() == "/lock.svg") {
    req->send_P(200, "image/svg+xml", LOCK_SVG);
    return;
  }

  AsyncResponseStream *stream = req->beginResponseStream("image/svg+xml");
  stream->print(F("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\"><path d=\"M12 3A18.9 18.9 0 0 "
                  "0 .38 7C4.41 12.06 7.89 16.37 12 21.5L23.65 7C20.32 4.41 16.22 3 12 "));
  if (req->url() == "/wifi-strength-4.svg") {
    stream->print(F("3z"));
  } else {
    if (req->url() == "/wifi-strength-1.svg") {
      stream->print(F("3m0 2c3.07 0 6.09.86 8.71 2.45l-5.1 6.36a8.43 8.43 0 0 0-7.22-.01L3.27 7.4"));
    } else if (req->url() == "/wifi-strength-2.svg") {
      stream->print(F("3m0 2c3.07 0 6.09.86 8.71 2.45l-3.21 3.98a11.32 11.32 0 0 0-11 0L3.27 7.4"));
    } else if (req->url() == "/wifi-strength-3.svg") {
      stream->print(F("3m0 2c3.07 0 6.09.86 8.71 2.45l-1.94 2.43A13.6 13.6 0 0 0 12 8C9 8 6.68 9 5.21 9.84l-1.94-2."));
    }
    stream->print(F("4A16.94 16.94 0 0 1 12 5z"));
  }
  stream->print(F("\"/></svg>"));
  req->send(stream);
}
CaptivePortal::CaptivePortal(web_server_base::WebServerBase *base) : base_(base) { global_captive_portal = this; }
float CaptivePortal::get_setup_priority() const {
  // Before WiFi
  return setup_priority::WIFI + 1.0f;
}
void CaptivePortal::dump_config() { ESP_LOGCONFIG(TAG, "Captive Portal:"); }

CaptivePortal *global_captive_portal = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace captive_portal
}  // namespace esphome

#endif  // USE_ARDUINO
