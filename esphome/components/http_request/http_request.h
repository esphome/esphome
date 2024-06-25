#pragma once

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

struct Header {
  const char *name;
  const char *value;
};

class HttpRequestComponent;

class HttpContainer : public Parented<HttpRequestComponent> {
 public:
  virtual ~HttpContainer() = default;
  size_t content_length;
  int status_code;
  uint32_t duration_ms;

  virtual int read(uint8_t *buf, size_t max_len) = 0;
  virtual void end() = 0;

  void set_secure(bool secure) { this->secure_ = secure; }

  size_t get_bytes_read() const { return this->bytes_read_; }

 protected:
  size_t bytes_read_{0};
  bool secure_{false};
};

class HttpRequestResponseTrigger : public Trigger<std::shared_ptr<HttpContainer>, std::string> {
 public:
  void process(std::shared_ptr<HttpContainer> container, std::string response_body) {
    this->trigger(std::move(container), std::move(response_body));
  }
};

class HttpRequestComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }
  void set_watchdog_timeout(uint32_t watchdog_timeout) { this->watchdog_timeout_ = watchdog_timeout; }
  uint32_t get_watchdog_timeout() const { return this->watchdog_timeout_; }
  void set_follow_redirects(bool follow_redirects) { this->follow_redirects_ = follow_redirects; }
  void set_redirect_limit(uint16_t limit) { this->redirect_limit_ = limit; }

  std::shared_ptr<HttpContainer> get(std::string url) { return this->start(std::move(url), "GET", "", {}); }
  std::shared_ptr<HttpContainer> get(std::string url, std::list<Header> headers) {
    return this->start(std::move(url), "GET", "", std::move(headers));
  }
  std::shared_ptr<HttpContainer> post(std::string url, std::string body) {
    return this->start(std::move(url), "POST", std::move(body), {});
  }
  std::shared_ptr<HttpContainer> post(std::string url, std::string body, std::list<Header> headers) {
    return this->start(std::move(url), "POST", std::move(body), std::move(headers));
  }

  virtual std::shared_ptr<HttpContainer> start(std::string url, std::string method, std::string body,
                                               std::list<Header> headers) = 0;

 protected:
  const char *useragent_{nullptr};
  bool follow_redirects_;
  uint16_t redirect_limit_;
  uint16_t timeout_{5000};
  uint32_t watchdog_timeout_{0};
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

  void set_max_response_buffer_size(size_t max_response_buffer_size) {
    this->max_response_buffer_size_ = max_response_buffer_size;
  }

  void play(Ts... x) override {
    std::string body;
    if (this->body_.has_value()) {
      body = this->body_.value(x...);
    }
    if (!this->json_.empty()) {
      auto f = std::bind(&HttpRequestSendAction<Ts...>::encode_json_, this, x..., std::placeholders::_1);
      body = json::build_json(f);
    }
    if (this->json_func_ != nullptr) {
      auto f = std::bind(&HttpRequestSendAction<Ts...>::encode_json_func_, this, x..., std::placeholders::_1);
      body = json::build_json(f);
    }
    std::list<Header> headers;
    for (const auto &item : this->headers_) {
      auto val = item.second;
      Header header;
      header.name = item.first;
      header.value = val.value(x...);
      headers.push_back(header);
    }

    auto container = this->parent_->start(this->url_.value(x...), this->method_.value(x...), body, headers);

    if (container == nullptr) {
      return;
    }

    size_t content_length = container->content_length;
    size_t max_length = std::min(content_length, this->max_response_buffer_size_);

    std::string response_body;
    if (this->capture_response_.value(x...)) {
      ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
      uint8_t *buf = allocator.allocate(max_length);
      if (buf != nullptr) {
        size_t read_index = 0;
        while (container->get_bytes_read() < max_length) {
          int read = container->read(buf + read_index, std::min<size_t>(max_length - read_index, 512));
          App.feed_wdt();
          yield();
          read_index += read;
        }
        response_body.reserve(read_index);
        response_body.assign((char *) buf, read_index);
        allocator.deallocate(buf, max_length);
      }
    }

    for (auto *trigger : this->response_triggers_) {
      trigger->process(container, response_body);
    }
    container->end();
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

  size_t max_response_buffer_size_{SIZE_MAX};
};

}  // namespace http_request
}  // namespace esphome
