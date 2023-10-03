#pragma once

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace http_request {

struct Header {
  const char *name;
  const char *value;
};

struct HttpResponse {
  int32_t status_code;
  int content_length;
  std::vector<char> data;
  uint32_t duration_ms;
};

class HttpRequestResponseTrigger : public Trigger<int32_t, uint32_t, HttpResponse &> {
 public:
  void process(HttpResponse &response) { this->trigger(response.status_code, response.duration_ms, response); }
};

class HttpRequestComponent : public Component {
 public:
  HttpRequestComponent();
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_method(std::string method) { this->method_ = std::move(method); }
  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }
  void set_follow_redirects(bool follow_redirects) { this->follow_redirects_ = follow_redirects; }
  void set_redirect_limit(uint16_t limit) { this->redirect_limit_ = limit; }
  void set_body(const std::string &body) { this->body_ = body; }
  void set_headers(std::list<Header> headers) { this->headers_ = std::move(headers); }
  void set_capture_response(bool capture_response) { this->capture_response_ = capture_response; }

  bool get_capture_response() { return this->capture_response_; }

  virtual void set_url(std::string url) = 0;
  virtual HttpResponse send() = 0;

 protected:
  std::string url_;
  std::string method_;
  const char *useragent_{nullptr};
  bool secure_;
  bool follow_redirects_;
  bool capture_response_;
  uint16_t redirect_limit_;
  uint16_t timeout_{5000};
  std::string body_;
  std::list<Header> headers_;
};

template<typename... Ts> class HttpRequestSendAction : public Action<Ts...> {
 public:
  HttpRequestSendAction(HttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(const char *, method)
  TEMPLATABLE_VALUE(std::string, body)
  TEMPLATABLE_VALUE(bool, capture_response)

  void add_header(const char *key, TemplatableValue<const char *, Ts...> value) { this->headers_.insert({key, value}); }

  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.insert({key, value}); }

  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

  void register_response_trigger(HttpRequestResponseTrigger *trigger) { this->response_triggers_.push_back(trigger); }

  void play(Ts... x) override {
    this->parent_->set_url(this->url_.value(x...));
    this->parent_->set_method(this->method_.value(x...));
    this->parent_->set_capture_response(this->capture_response_.value(x...));
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
    std::list<Header> headers;
    for (const auto &item : this->headers_) {
      auto val = item.second;
      Header header;
      header.name = item.first;
      header.value = val.value(x...);
      headers.push_back(header);
    }
    this->parent_->set_headers(headers);
    HttpResponse response = this->parent_->send();

    for (auto *trigger : this->response_triggers_)
      trigger->process(response);
    this->parent_->set_body("");
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

extern HttpRequestComponent *global_http_request;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace http_request
}  // namespace esphome
