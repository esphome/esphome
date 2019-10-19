#pragma once

#include "esphome/core/component.h"

#include <ESPAsyncWebServer.h>

namespace esphome {
namespace web_server_base {

class WebServerBase : public Component {
 public:
  void init() {
    if (this->initialized_) {
      this->initialized_++;
      return;
    }
    this->server_ = new AsyncWebServer(this->port_);
    this->server_->begin();

    for (auto *handler : this->handlers_)
      this->server_->addHandler(handler);

    this->initialized_++;
  }
  void deinit() {
    this->initialized_--;
    if (this->initialized_ == 0) {
      delete this->server_;
      this->server_ = nullptr;
    }
  }
  AsyncWebServer *get_server() const { return server_; }
  float get_setup_priority() const override;

  void add_handler(AsyncWebHandler *handler) {
    // remove all handlers

    this->handlers_.push_back(handler);
    if (this->server_ != nullptr)
      this->server_->addHandler(handler);
  }

  void add_ota_handler();

  void set_port(uint16_t port) { port_ = port; }
  uint16_t get_port() const { return port_; }

 protected:
  friend class OTARequestHandler;

  int initialized_{0};
  uint16_t port_{80};
  AsyncWebServer *server_{nullptr};
  std::vector<AsyncWebHandler *> handlers_;
};

class OTARequestHandler : public AsyncWebHandler {
 public:
  OTARequestHandler(WebServerBase *parent) : parent_(parent) {}
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool canHandle(AsyncWebServerRequest *request) override {
    return request->url() == "/update" && request->method() == HTTP_POST;
  }

  bool isRequestHandlerTrivial() override { return false; }

 protected:
  uint32_t last_ota_progress_{0};
  uint32_t ota_read_length_{0};
  WebServerBase *parent_;
};

}  // namespace web_server_base
}  // namespace esphome
