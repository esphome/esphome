#include "captive_portal.h"
#ifdef USE_CAPTIVE_PORTAL
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/wifi/wifi_component.h"
#include "captive_index.h"

namespace esphome {
namespace captive_portal {

static const char *const TAG = "captive_portal";

void CaptivePortal::handle_captive_portal(AsyncWebServerRequest *request) {
  auto *response = request->beginResponse_P(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}
void CaptivePortal::handle_config(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream(ESPHOME_F("application/json"));
  stream->addHeader(ESPHOME_F("cache-control"), ESPHOME_F("public, max-age=0, must-revalidate"));
  char mac_s[18];
  const char *mac_str = get_mac_address_pretty_into_buffer(mac_s);
#ifdef USE_ESP8266
  stream->print(ESPHOME_F("{\"mac\":\""));
  stream->print(mac_str);
  stream->print(ESPHOME_F("\",\"name\":\""));
  stream->print(App.get_name().c_str());
  stream->print(ESPHOME_F("\",\"aps\":[{}"));
#else
  stream->printf(R"({"mac":"%s","name":"%s","aps":[{})", mac_str, App.get_name().c_str());
#endif

  // Todo Test this if it is needed!
  bool passive_scan = request->url() == "/" ? false : true;

  wifi::global_wifi_component->start_scanning(passive_scan);

  for (auto &scan : wifi::global_wifi_component->get_scan_result()) {
    if (scan.get_is_hidden())
      continue;

      // Assumes no " in ssid, possible unicode isses?
#ifdef USE_ESP8266
    stream->print(ESPHOME_F(",{\"ssid\":\""));
    stream->print(scan.get_ssid().c_str());
    stream->print(ESPHOME_F("\",\"rssi\":"));
    stream->print(scan.get_rssi());
    stream->print(ESPHOME_F(",\"lock\":"));
    stream->print(scan.get_with_auth());
    stream->print(ESPHOME_F("}"));
#else
    stream->printf(R"(,{"ssid":"%s","rssi":%d,"lock":%d})", scan.get_ssid().c_str(), scan.get_rssi(),
                   scan.get_with_auth());
#endif
  }
  stream->print(ESPHOME_F("]}"));
  request->send(stream);
}
void CaptivePortal::handle_wifisave(AsyncWebServerRequest *request) {
  std::string ssid = request->arg("ssid").c_str();  // NOLINT(readability-redundant-string-cstr)
  std::string psk = request->arg("psk").c_str();    // NOLINT(readability-redundant-string-cstr)
  ESP_LOGI(TAG, "Requested WiFi Settings Change:");
  ESP_LOGI(TAG, "  SSID='%s'", ssid.c_str());
  ESP_LOGI(TAG, "  Password=" LOG_SECRET("'%s'"), psk.c_str());
  wifi::global_wifi_component->save_wifi_sta(ssid, psk);
  request->redirect("/?save");
}

void CaptivePortal::handleRequest(AsyncWebServerRequest *req) {
  if (req->url() == this->portal_path_) {
    this->handle_captive_portal(req);
    return;
  } else if (req->url() == "/config.json") {
    this->handle_config(req);
    return;
  } else if (req->url() == "/wifisave") {
    this->handle_wifisave(req);
    return;
  }
}

void CaptivePortal::setup() {}
void CaptivePortal::start(const String portal_path) {
  ESP_LOGV(TAG, "Starting Captive Portal using path: %s", portal_path.c_str());
  this->portal_path_ = portal_path;
  // Defer save to main loop thread to avoid NVS operations from HTTP thread
  this->defer([ssid, psk]() { wifi::global_wifi_component->save_wifi_sta(ssid, psk); });
  request->redirect(ESPHOME_F("/?save"));
}

void CaptivePortal::setup() {
  // Disable loop by default - will be enabled when captive portal starts
  this->disable_loop();
}
void CaptivePortal::start() {
  this->base_->init();
  if (!this->initialized_) {
    this->base_->add_handler(this);
  }
#ifdef USE_WIFI_AP
#ifdef USE_ARDUINO

  network::IPAddress ip = wifi::global_wifi_component->wifi_soft_ap_ip();

#if defined(USE_ESP32)
  // Create DNS server instance for ESP-IDF
  this->dns_server_ = make_unique<DNSServer>();
  this->dns_server_->start(ip);
#elif defined(USE_ARDUINO)
  this->dns_server_ = make_unique<DNSServer>();
  this->dns_server_->setErrorReplyCode(DNSReplyCode::NoError);
  this->dns_server_->start(53, ESPHOME_F("*"), ip);
#endif

  this->base_->get_server()->onNotFound([this](AsyncWebServerRequest *req) {
    if (!this->active_ || req->host().c_str() == wifi::global_wifi_component->wifi_soft_ap_ip().str()) {
      req->send(404, "text/html", "File not found");
      return;
    }

    auto url = "http://" + wifi::global_wifi_component->wifi_soft_ap_ip().str();
    req->redirect(url.c_str());
  });
#endif  // USE_WIFI_AP
  this->initialized_ = true;
  this->active_ = true;

  // Enable loop() now that captive portal is active
  this->enable_loop();

  ESP_LOGV(TAG, "Captive portal started");
}

void CaptivePortal::end() {
  ESP_LOGV(TAG, "Ending Captive Portal...");

  this->active_ = false;
  this->base_->deinit();
#ifdef USE_ARDUINO
  this->dns_server_->stop();
  this->dns_server_ = nullptr;
#endif
  void CaptivePortal::handleRequest(AsyncWebServerRequest * req) {
    if (req->url() == ESPHOME_F("/config.json")) {
      this->handle_config(req);
      return;
    } else if (req->url() == ESPHOME_F("/wifisave")) {
      this->handle_wifisave(req);
      return;
    }

    // All other requests get the captive portal page
    // This includes OS captive portal detection endpoints which will trigger
    // the captive portal when they don't receive their expected responses
#ifndef USE_ESP8266
    auto *response = req->beginResponse(200, ESPHOME_F("text/html"), INDEX_GZ, sizeof(INDEX_GZ));
#else
    auto *response = req->beginResponse_P(200, ESPHOME_F("text/html"), INDEX_GZ, sizeof(INDEX_GZ));
#endif
    response->addHeader(ESPHOME_F("Content-Encoding"), ESPHOME_F("gzip"));
    req->send(response);
  }

  CaptivePortal::CaptivePortal(web_server_base::WebServerBase * base) : base_(base) { global_captive_portal = this; }
  float CaptivePortal::get_setup_priority() const {
    // Before WiFi
    return setup_priority::WIFI + 1.0f;
  }
  void CaptivePortal::dump_config() { ESP_LOGCONFIG(TAG, "Captive Portal:"); }

  CaptivePortal *global_captive_portal = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace captive_portal
}  // namespace esphome
#endif
