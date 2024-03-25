#ifdef USE_ESP_IDF

#include "ota_http_idf.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "esphome/components/md5/md5.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "esp_idf_version.h"
#include <cinttypes>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sys/param.h>
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"

namespace esphome {
namespace ota_http {

int OtaHttpIDF::http_init(char *url) {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0x03,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&wdt_config);
#else
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  App.feed_wdt();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  esp_http_client_config_t config = {nullptr};
  config.url = url;
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

  this->client_ = esp_http_client_init(&config);
  esp_err_t err;
  if ((err = esp_http_client_open(this->client_, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    return 0;
  }
  this->body_length_ = esp_http_client_fetch_headers(this->client_);

  ESP_LOGV(TAG, "body_length: %d , esp_http_client_get_content_length: %" PRId64, this->body_length_,
           (int64_t) esp_http_client_get_content_length(this->client_));

  if (this->body_length_ <= 0) {
    ESP_LOGE(TAG, "Incorrect file size (%d) reported by HTTP server (status: %d). Aborting", this->body_length_, err);
    return -1;
  }
  return this->body_length_;
}

int OtaHttpIDF::http_read(uint8_t *buf, const size_t max_len) {
  int bufsize = std::min(max_len, this->body_length_ - this->bytes_read_);
  int read_len = esp_http_client_read(this->client_, (char *) buf, bufsize);
  if (read_len > 0) {
    this->bytes_read_ += bufsize;
    buf[bufsize] = '\0';  // not fed to ota
  }
  // ESP_LOGVV(TAG, "Read %d bytes, %d remaining", read_len, this->body_length_ - this->bytes_read);

  return read_len;
}

void OtaHttpIDF::http_end() {
  esp_http_client_close(this->client_);
  esp_http_client_cleanup(this->client_);
}

}  // namespace ota_http
}  // namespace esphome

#endif  // USE_ESP_IDF
