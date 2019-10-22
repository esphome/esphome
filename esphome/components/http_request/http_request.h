#pragma once

#include <list>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#ifdef ARDUINO_ARCH_ESP32
#include <HTTPClient.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

namespace esphome {
namespace http_request {

class HttpRequestComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_url(const char *url) {
    this->url_ = url;
    this->wifi_client_ = new BearSSL::WiFiClientSecure();
    this->wifi_client_->setInsecure();
  }
  void set_method(const char *method) { this->method_ = method; }
  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }
  void set_body(std::string body) { this->body_ = body; }
  void add_header(const char *name, const char *value) {
    Header header;
    header.name = name;
    header.value = value;
    this->headers_.push_back(header);
  }
  void send();

 protected:
  struct Header {
    const char *name;
    const char *value;
  };
  HTTPClient client_{};
  const char *url_;
  const char *method_;
  const char *useragent_{nullptr};
  uint16_t timeout_{5000};
  std::string body_;
  std::list<Header> headers_;
  BearSSL::WiFiClientSecure *wifi_client_;
};

template<typename... Ts> class HttpRequestSendAction : public Action<Ts...> {
 public:
  HttpRequestSendAction(HttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, body)

  void play(Ts... x) override {
    if (this->body_.has_value()) {
      auto body = this->body_.value(x...);
      this->parent_->set_body(body);
    }
    this->parent_->send();
  }

 protected:
  HttpRequestComponent *parent_;
};

}  // namespace http_request
}  // namespace esphome
