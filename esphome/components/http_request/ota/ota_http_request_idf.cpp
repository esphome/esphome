#include "ota_http_request_idf.h"
#include "watchdog.h"

#ifdef USE_ESP_IDF
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/network/util.h"

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_tls.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <cctype>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <sys/param.h>
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

namespace esphome {
namespace http_request {

void OtaHttpRequestComponentIDF::http_init(const std::string &url) {
  App.feed_wdt();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  esp_http_client_config_t config = {nullptr};
  config.url = url.c_str();
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = (int) this->timeout_;
  config.buffer_size = this->max_http_recv_buffer_;
  config.auth_type = HTTP_AUTH_TYPE_BASIC;
  config.max_authorization_retries = -1;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  if (this->secure_()) {
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }
#endif
#pragma GCC diagnostic pop

  watchdog::WatchdogManager wdts;
  this->client_ = esp_http_client_init(&config);
  if ((this->status_ = esp_http_client_open(this->client_, 0)) == ESP_OK) {
    this->body_length_ = esp_http_client_fetch_headers(this->client_);
    this->status_ = esp_http_client_get_status_code(this->client_);
  }
}

int OtaHttpRequestComponentIDF::http_read(uint8_t *buf, const size_t max_len) {
  watchdog::WatchdogManager wdts;
  int bufsize = std::min(max_len, this->body_length_ - this->bytes_read_);

  App.feed_wdt();
  int read_len = esp_http_client_read(this->client_, (char *) buf, bufsize);
  if (read_len > 0) {
    this->bytes_read_ += bufsize;
    buf[bufsize] = '\0';  // not fed to ota
  }

  return read_len;
}

void OtaHttpRequestComponentIDF::http_end() {
  watchdog::WatchdogManager wdts;

  esp_http_client_close(this->client_);
  esp_http_client_cleanup(this->client_);
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
