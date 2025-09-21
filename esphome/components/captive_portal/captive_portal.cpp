#include "captive_portal.h"
#ifdef USE_CAPTIVE_PORTAL
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/wifi/wifi_component.h"
#include "captive_index.h"

namespace esphome {
namespace captive_portal {

static const char *const TAG = "captive_portal";

void CaptivePortal::handle_config(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream(F("application/json"));
  stream->addHeader(F("cache-control"), F("public, max-age=0, must-revalidate"));
#ifdef USE_ESP8266
  stream->print(F("{\"mac\":\""));
  stream->print(get_mac_address_pretty().c_str());
  stream->print(F("\",\"name\":\""));
  stream->print(App.get_name().c_str());
  stream->print(F("\",\"aps\":[{}"));
#else
  stream->printf(R"({"mac":"%s","name":"%s","aps":[{})", get_mac_address_pretty().c_str(), App.get_name().c_str());
#endif

  for (auto &scan : wifi::global_wifi_component->get_scan_result()) {
    if (scan.get_is_hidden())
      continue;

      // Assumes no " in ssid, possible unicode isses?
#ifdef USE_ESP8266
    stream->print(F(",{\"ssid\":\""));
    stream->print(scan.get_ssid().c_str());
    stream->print(F("\",\"rssi\":"));
    stream->print(scan.get_rssi());
    stream->print(F(",\"lock\":"));
    stream->print(scan.get_with_auth());
    stream->print(F("}"));
#else
    stream->printf(R"(,{"ssid":"%s","rssi":%d,"lock":%d})", scan.get_ssid().c_str(), scan.get_rssi(),
                   scan.get_with_auth());
#endif
  }
  stream->print(F("]}"));
  request->send(stream);
}
void CaptivePortal::handle_wifisave(AsyncWebServerRequest *request) {
  std::string ssid = request->arg("ssid").c_str();
  std::string psk = request->arg("psk").c_str();
  ESP_LOGI(TAG, "Requested WiFi Settings Change:");
  ESP_LOGI(TAG, "  SSID='%s'", ssid.c_str());
  ESP_LOGI(TAG, "  Password=" LOG_SECRET("'%s'"), psk.c_str());
  wifi::global_wifi_component->save_wifi_sta(ssid, psk);
  wifi::global_wifi_component->start_scanning();
  request->redirect(F("/?save"));
}

void CaptivePortal::setup() {
#ifndef USE_ARDUINO
  // No DNS server needed for non-Arduino frameworks
  this->disable_loop();
#endif
}
void CaptivePortal::start() {
  this->base_->init();
  if (!this->initialized_) {
    this->base_->add_handler(this);
  }

#ifdef USE_ARDUINO
  this->dns_server_ = make_unique<DNSServer>();
  this->dns_server_->setErrorReplyCode(DNSReplyCode::NoError);
  network::IPAddress ip = wifi::global_wifi_component->wifi_soft_ap_ip();
  this->dns_server_->start(53, F("*"), ip);
  // Re-enable loop() when DNS server is started
  this->enable_loop();
#endif

  this->base_->get_server()->onNotFound([this](AsyncWebServerRequest *req) {
    if (!this->active_ || req->host().c_str() == wifi::global_wifi_component->wifi_soft_ap_ip().str()) {
      req->send(404, F("text/html"), F("File not found"));
      return;
    }

#ifdef USE_ESP8266
    String url = F("http://");
    url += wifi::global_wifi_component->wifi_soft_ap_ip().str().c_str();
#else
    auto url = "http://" + wifi::global_wifi_component->wifi_soft_ap_ip().str();
#endif
    req->redirect(url.c_str());
  });

  this->initialized_ = true;
  this->active_ = true;
}

void CaptivePortal::handleRequest(AsyncWebServerRequest *req) {
  if (req->url() == F("/")) {
#ifndef USE_ESP8266
    auto *response = req->beginResponse(200, F("text/html"), INDEX_GZ, sizeof(INDEX_GZ));
#else
    auto *response = req->beginResponse_P(200, F("text/html"), INDEX_GZ, sizeof(INDEX_GZ));
#endif
    response->addHeader(F("Content-Encoding"), F("gzip"));
    req->send(response);
    return;
  } else if (req->url() == F("/config.json")) {
    this->handle_config(req);
    return;
  } else if (req->url() == F("/wifisave")) {
    this->handle_wifisave(req);
    return;
  }
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
#endif
