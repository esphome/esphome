#pragma once

#include "http_request.h"

#ifdef USE_ESP_IDF

#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_tls.h>

namespace esphome {
namespace http_request {

class HttpContainerIDF : public HttpContainer {
 public:
  HttpContainerIDF(esp_http_client_handle_t client) : client_(client) {}
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

 protected:
  esp_http_client_handle_t client_;
};

class HttpRequestIDF : public HttpRequestComponent {
 public:
  std::shared_ptr<HttpContainer> start(std::string url, std::string method, std::string body,
                                       std::list<Header> headers) override;
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
