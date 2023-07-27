#include "captive_portal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/wifi/wifi_component.h"
#include "captive_index.h"

namespace esphome {
namespace captive_portal {

static const char *const TAG = "captive_portal";

void CaptivePortal::handle_config(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("application/json");
  stream->addHeader("cache-control", "public, max-age=0, must-revalidate");
  stream->printf(R"({"name":"%s","aps":[{})", App.get_name().c_str());

  for (auto &scan : wifi::global_wifi_component->get_scan_result()) {
    if (scan.get_is_hidden())
      continue;

    // Assumes no " in ssid, possible unicode isses?
    stream->printf(R"(,{"ssid":"%s","rssi":%d,"lock":%d})", scan.get_ssid().c_str(), scan.get_rssi(),
                   scan.get_with_auth());
  }
  stream->print(F("]}"));
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
  request->redirect("/?save");
}

void CaptivePortal::setup() {}
void CaptivePortal::start() {
  this->base_->init();
  if (!this->initialized_) {
    this->base_->add_handler(this);
    this->base_->add_ota_handler();
  }

#ifdef USE_ARDUINO
  this->dns_server_ = make_unique<DNSServer>();
  this->dns_server_->setErrorReplyCode(DNSReplyCode::NoError);
  network::IPAddress ip = wifi::global_wifi_component->wifi_soft_ap_ip();
  this->dns_server_->start(53, "*", (uint32_t) ip);
#endif

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

void CaptivePortal::handleRequest(AsyncWebServerRequest *req) {
  if (req->url() == "/") {
    auto *response = req->beginResponse_P(200, "text/html", INDEX_GZ, sizeof(INDEX_GZ));
    response->addHeader("Content-Encoding", "gzip");
    req->send(response);
    return;
  } else if (req->url() == "/config.json") {
    this->handle_config(req);
    return;
  } else if (req->url() == "/wifisave") {
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
