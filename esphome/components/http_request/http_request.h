#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

namespace esphome {
namespace http_request {

struct Header {
  const char *name;
  const char *value;
};

class HttpRequestResponseTrigger : public Trigger<int32_t, uint32_t> {
 public:
  void process(int32_t status_code, uint32_t duration_ms) { this->trigger(status_code, duration_ms); }
};

class HttpRequestComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_url(std::string url);
  void set_method(const char *method) { this->method_ = method; }
  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }
  void set_follow_redirects(bool follow_redirects) { this->follow_redirects_ = follow_redirects; }
  void set_redirect_limit(uint16_t limit) { this->redirect_limit_ = limit; }
  void set_body(const std::string &body) { this->body_ = body; }
  void set_headers(std::list<Header> headers) { this->headers_ = std::move(headers); }
  void send(const std::vector<HttpRequestResponseTrigger *> &response_triggers);
  void close();
  const char *get_string();

 protected:
  HTTPClient client_{};
  std::string url_;
  std::string last_url_;
  const char *method_;
  const char *useragent_{nullptr};
  bool secure_;
  bool follow_redirects_;
  uint16_t redirect_limit_;
  uint16_t timeout_{5000};
  std::string body_;
  std::list<Header> headers_;
#ifdef USE_ESP8266
  std::shared_ptr<WiFiClient> wifi_client_;
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  std::shared_ptr<BearSSL::WiFiClientSecure> wifi_client_secure_;
#endif
  std::shared_ptr<WiFiClient> get_wifi_client_();
#endif
};

template<typename... Ts> class HttpRequestSendAction : public Action<Ts...> {
 public:
  HttpRequestSendAction(HttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(const char *, method)
  TEMPLATABLE_VALUE(std::string, body)
  TEMPLATABLE_VALUE(const char *, useragent)
  TEMPLATABLE_VALUE(uint16_t, timeout)

  void add_header(const char *key, TemplatableValue<const char *, Ts...> value) { this->headers_.insert({key, value}); }

  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.insert({key, value}); }

  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

  void register_response_trigger(HttpRequestResponseTrigger *trigger) { this->response_triggers_.push_back(trigger); }

  void play(Ts... x) override {
    this->parent_->set_url(this->url_.value(x...));
    this->parent_->set_method(this->method_.value(x...));
    if (this->body_.has_value()) {
      this->parent_->set_body(this->body_.value(x...));
    }
    if (!this->json_.empty()) {
      auto f = std::bind(&HttpRequestSendAction<Ts...>::encode_json_, this, x..., std::placeholders::_1);
      this->parent_->set_body(json::build_json(f));
    }
    if (this->json_func_ != nullptr) {
      auto f = std::bind(&HttpRequestSendAction<Ts...>::encode_json_func_, this, x..., std::placeholders::_1);
      this->parent_->set_body(json::build_json(f));
    }
    if (this->useragent_.has_value()) {
      this->parent_->set_useragent(this->useragent_.value(x...));
    }
    if (this->timeout_.has_value()) {
      this->parent_->set_timeout(this->timeout_.value(x...));
    }
    if (!this->headers_.empty()) {
      std::list<Header> headers;
      for (const auto &item : this->headers_) {
        auto val = item.second;
        Header header;
        header.name = item.first;
        header.value = val.value(x...);
        headers.push_back(header);
      }
      this->parent_->set_headers(headers);
    }
    this->parent_->send(this->response_triggers_);
    this->parent_->close();
  }

 protected:
  void encode_json_(Ts... x, JsonObject root) {
    for (const auto &item : this->json_) {
      auto val = item.second;
      root[item.first] = val.value(x...);
    }
  }
  void encode_json_func_(Ts... x, JsonObject root) { this->json_func_(x..., root); }
  HttpRequestComponent *parent_;
  std::map<const char *, TemplatableValue<const char *, Ts...>> headers_{};
  std::map<const char *, TemplatableValue<std::string, Ts...>> json_{};
  std::function<void(Ts..., JsonObject)> json_func_{nullptr};
  std::vector<HttpRequestResponseTrigger *> response_triggers_;
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
