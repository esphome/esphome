#pragma once

#include "simplesevse.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace simpleevse {

#ifdef USE_SIMPLEEVSE_WEB_CONFIG

/** Http handler for the setup/config register of the SimpleEVSE.
 * 
 * This handler provides a web page with the config register of the SimpleEVSE and
 * allows also the value to change.
 */
class SimpleEvseHttpHandler : public AsyncWebHandler, public Component {
 public:
  SimpleEvseHttpHandler(web_server_base::WebServerBase *base, SimpleEvseComponent *parent) : base_(base), parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    String url = request->url();
    return url == this->index_path || url == this->set_value_path;
  }

  void handleRequest(AsyncWebServerRequest *req) override;

  bool isRequestHandlerTrivial() override { return false; } // POST data

  void setup() override {
    this->base_->init();
    this->base_->add_handler(this);
  }

  float get_setup_priority() const override {
    // After WiFi
    return setup_priority::WIFI - 1.0f;
  }
protected:
  void handleIndex(AsyncWebServerRequest *req);
  void handleSetConfig(AsyncWebServerRequest *req);

  const String index_path = "/simpleevse";
  const String set_value_path = "/simpleevse/set_value";

  web_server_base::WebServerBase *const base_;
  SimpleEvseComponent *const parent_;
};

#endif // USE_SIMPLEEVSE_WEB_CONFIG

}  // namespace simpleevse
}  // namespace esphome
