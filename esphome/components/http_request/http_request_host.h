#pragma once

#ifdef USE_HOST
#include "http_request.h"
namespace esphome {
namespace http_request {

class HttpRequestHost;
class HttpContainerHost : public HttpContainer {
 public:
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

 protected:
  friend class HttpRequestHost;
  std::vector<uint8_t> response_body_{};
};

class HttpRequestHost : public HttpRequestComponent {
 public:
  std::shared_ptr<HttpContainer> perform(const std::string &url, const std::string &method, const std::string &body,
                                         const std::list<Header> &request_headers,
                                         const std::set<std::string> &response_headers) override;
  void set_ca_path(const char *ca_path) { this->ca_path_ = ca_path; }

 protected:
  const char *ca_path_{};
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_HOST
