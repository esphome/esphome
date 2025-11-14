#pragma once

#ifdef USE_HOST
#include <curl/curl.h>
#include "http_request.h"

namespace esphome {
namespace http_request {

class HttpRequestHost;
class HttpContainerHost : public HttpContainer {
 public:
  HttpContainerHost(CURLM *multi, CURL *handle, curl_slist *headers_slist)
      : multi_(multi), handle_(handle), headers_slist_(headers_slist) {}

  ~HttpContainerHost() {
    // ensure the curl easy handle is disposed. otherwise, it may hold a pointer to
    // `this->response_body_`, which will become invalid after `~HttpContainerHost()`
    this->end();
  }

  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

 protected:
  friend class HttpRequestHost;
  std::vector<uint8_t> response_body_{};
  CURLM *multi_;
  CURL *handle_;
  curl_slist *headers_slist_;
};

class HttpRequestHost : public HttpRequestComponent {
 public:
  std::shared_ptr<HttpContainer> perform(const std::string &url, const std::string &method, const std::string &body,
                                         const std::list<Header> &request_headers,
                                         const std::set<std::string> &response_headers) override;
  void set_ca_path(const char *ca_path) { this->ca_path_ = ca_path; }

  void set_verbose(bool verbose) { this->verbose_ = verbose; }

  void setup() override { curl_global_init(CURL_GLOBAL_DEFAULT); }

  virtual bool teardown() override {
    curl_global_cleanup();
    return true;
  }

 protected:
  const char *ca_path_{};
  bool verbose_{};
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_HOST
