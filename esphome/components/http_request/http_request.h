#pragma once

#include <list>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#ifdef ARDUINO_ARCH_ESP32
#include <HTTPClient.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266HTTPClient.h>
#endif

namespace esphome {
namespace http_request {

class HttpRequestComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_uri(const char *uri) { this->uri_ = uri; }
  void set_method(const char *method) { this->method_ = method; }
  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }
  void set_payload(std::string payload) { this->payload_ = payload; }
  void set_ssl_fingerprint(const std::array<uint8_t, 20> &fingerprint) {
    static uint8_t FP[20];
    memcpy(FP, fingerprint.data(), 20);
    this->fingerprint_ = FP;
  }
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
  const char *uri_;
  const char *method_;
  const uint8_t *fingerprint_{nullptr};
  const char *useragent_{nullptr};
  uint16_t timeout_{5000};
  std::string payload_;
  std::list<Header> headers_;
};

template<typename... Ts> class HttpRequestSendAction : public Action<Ts...> {
 public:
  HttpRequestSendAction(HttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, payload)

  void play(Ts... x) override {
    if (this->payload_.has_value()) {
      auto payload = this->payload_.value(x...);
      this->parent_->set_payload(payload);
    }
    this->parent_->send();
  }

 protected:
  HttpRequestComponent *parent_;
};

}  // namespace http_request
}  // namespace esphome
