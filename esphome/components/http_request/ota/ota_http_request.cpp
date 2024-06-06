#include "ota_http_request.h"
#include "watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include "esphome/components/md5/md5.h"
#include "esphome/components/ota/ota_backend_arduino_esp32.h"
#include "esphome/components/ota/ota_backend_arduino_esp8266.h"
#include "esphome/components/ota/ota_backend_arduino_rp2040.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"
#include "esphome/components/ota/ota_backend.h"

namespace esphome {
namespace http_request {

void OtaHttpRequestComponent::setup() {
#ifdef USE_OTA_STATE_CALLBACK
  ota::register_ota_platform(this);
#endif
}

void OtaHttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Over-The-Air updates via HTTP request:");
  ESP_LOGCONFIG(TAG, "  Timeout: %llus", this->timeout_ / 1000);
#ifdef USE_ESP8266
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  ESP_LOGCONFIG(TAG, "  ESP8266 SSL support: No");
#else
  ESP_LOGCONFIG(TAG, "  ESP8266 SSL support: Yes");
#endif
#endif
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  ESP_LOGCONFIG(TAG, "  TLS server verification: Yes");
#else
  ESP_LOGCONFIG(TAG, "  TLS server verification: No");
#endif
#ifdef USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT
  ESP_LOGCONFIG(TAG, "  Watchdog timeout: %ds", USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT / 1000);
#endif
};

void OtaHttpRequestComponent::set_md5_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->md5_url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->md5_url_ = url;
  this->md5_expected_.clear();  // to be retrieved later
}

void OtaHttpRequestComponent::set_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->url_ = url;
}

bool OtaHttpRequestComponent::check_status() {
  // status can be -1, or HTTP status code
  if (this->status_ < 100) {
    ESP_LOGE(TAG, "HTTP server did not respond (error %d)", this->status_);
    return false;
  }
  if (this->status_ >= 310) {
    ESP_LOGE(TAG, "HTTP error %d", this->status_);
    return false;
  }
  ESP_LOGV(TAG, "HTTP status %d", this->status_);
  return true;
}

void OtaHttpRequestComponent::flash() {
  if (this->url_.empty()) {
    ESP_LOGE(TAG, "URL not set; cannot start update");
    return;
  }

  ESP_LOGI(TAG, "Starting update...");
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_STARTED, 0.0f, 0);
#endif

  auto ota_status = this->do_ota_();

  switch (ota_status) {
    case ota::OTA_RESPONSE_OK:
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_COMPLETED, 100.0f, ota_status);
#endif
      delay(10);
      App.safe_reboot();
      break;

    default:
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_ERROR, 0.0f, ota_status);
#endif
      this->md5_computed_.clear();  // will be reset at next attempt
      this->md5_expected_.clear();  // will be reset at next attempt
      break;
  }
}

void OtaHttpRequestComponent::cleanup_(std::unique_ptr<ota::OTABackend> backend) {
  if (this->update_started_) {
    ESP_LOGV(TAG, "Aborting OTA backend");
    backend->abort();
  }
  ESP_LOGV(TAG, "Aborting HTTP connection");
  this->http_end();
};

uint8_t OtaHttpRequestComponent::do_ota_() {
  uint8_t buf[this->http_recv_buffer_ + 1];
  uint32_t last_progress = 0;
  uint32_t update_start_time = millis();
  md5::MD5Digest md5_receive;
  std::unique_ptr<char[]> md5_receive_str(new char[33]);

  if (this->md5_expected_.empty() && !this->http_get_md5_()) {
    return OTA_MD5_INVALID;
  }

  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());

  auto url_with_auth = this->get_url_with_auth_(this->url_);
  if (url_with_auth.empty()) {
    return OTA_BAD_URL;
  }
  ESP_LOGVV(TAG, "url_with_auth: %s", url_with_auth.c_str());
  ESP_LOGI(TAG, "Connecting to: %s", this->url_.c_str());
  this->http_init(url_with_auth);
  if (!this->check_status()) {
    this->http_end();
    return OTA_CONNECTION_ERROR;
  }

  // we will compute MD5 on the fly for verification -- Arduino OTA seems to ignore it
  md5_receive.init();
  ESP_LOGV(TAG, "MD5Digest initialized");

  ESP_LOGV(TAG, "OTA backend begin");
  auto backend = ota::make_ota_backend();
  auto error_code = backend->begin(this->body_length_);
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "backend->begin error: %d", error_code);
    this->cleanup_(std::move(backend));
    return error_code;
  }

  this->bytes_read_ = 0;
  while (this->bytes_read_ < this->body_length_) {
    // read a maximum of chunk_size bytes into buf. (real read size returned)
    int bufsize = this->http_read(buf, this->http_recv_buffer_);
    ESP_LOGVV(TAG, "bytes_read_ = %u, body_length_ = %u, bufsize = %i", this->bytes_read_, this->body_length_, bufsize);

    // feed watchdog and give other tasks a chance to run
    App.feed_wdt();
    yield();

    if (bufsize < 0) {
      ESP_LOGE(TAG, "Stream closed");
      this->cleanup_(std::move(backend));
      return OTA_CONNECTION_ERROR;
    } else if (bufsize > 0 && bufsize <= this->http_recv_buffer_) {
      // add read bytes to MD5
      md5_receive.add(buf, bufsize);

      // write bytes to OTA backend
      this->update_started_ = true;
      error_code = backend->write(buf, bufsize);
      if (error_code != ota::OTA_RESPONSE_OK) {
        // error code explanation available at
        // https://github.com/esphome/esphome/blob/dev/esphome/components/ota/ota_backend.h
        ESP_LOGE(TAG, "Error code (%02X) writing binary data to flash at offset %d and size %d", error_code,
                 this->bytes_read_ - bufsize, this->body_length_);
        this->cleanup_(std::move(backend));
        return error_code;
      }
    }

    uint32_t now = millis();
    if ((now - last_progress > 1000) or (this->bytes_read_ == this->body_length_)) {
      last_progress = now;
      float percentage = this->bytes_read_ * 100.0f / this->body_length_;
      ESP_LOGD(TAG, "Progress: %0.1f%%", percentage);
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_IN_PROGRESS, percentage, 0);
#endif
    }
  }  // while

  ESP_LOGI(TAG, "Done in %.0f seconds", float(millis() - update_start_time) / 1000);

  // verify MD5 is as expected and act accordingly
  md5_receive.calculate();
  md5_receive.get_hex(md5_receive_str.get());
  this->md5_computed_ = md5_receive_str.get();
  if (strncmp(this->md5_computed_.c_str(), this->md5_expected_.c_str(), MD5_SIZE) != 0) {
    ESP_LOGE(TAG, "MD5 computed: %s - Aborting due to MD5 mismatch", this->md5_computed_.c_str());
    this->cleanup_(std::move(backend));
    return ota::OTA_RESPONSE_ERROR_MD5_MISMATCH;
  } else {
    backend->set_update_md5(md5_receive_str.get());
  }

  this->http_end();

  // feed watchdog and give other tasks a chance to run
  App.feed_wdt();
  yield();
  delay(100);  // NOLINT

  error_code = backend->end();
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending update! error_code: %d", error_code);
    this->cleanup_(std::move(backend));
    return error_code;
  }

  ESP_LOGI(TAG, "Update complete");
  return ota::OTA_RESPONSE_OK;
}

std::string OtaHttpRequestComponent::get_url_with_auth_(const std::string &url) {
  if (this->username_.empty() || this->password_.empty()) {
    return url;
  }

  auto start_char = url.find("://");
  if ((start_char == std::string::npos) || (start_char < 4)) {
    ESP_LOGE(TAG, "Incorrect URL prefix");
    return {};
  }

  ESP_LOGD(TAG, "Using basic HTTP authentication");

  start_char += 3;  // skip '://' characters
  auto url_with_auth =
      url.substr(0, start_char) + this->username_ + ":" + this->password_ + "@" + url.substr(start_char);
  return url_with_auth;
}

bool OtaHttpRequestComponent::http_get_md5_() {
  if (this->md5_url_.empty()) {
    return false;
  }

  auto url_with_auth = this->get_url_with_auth_(this->md5_url_);
  if (url_with_auth.empty()) {
    return false;
  }

  ESP_LOGVV(TAG, "url_with_auth: %s", url_with_auth.c_str());
  ESP_LOGI(TAG, "Connecting to: %s", this->md5_url_.c_str());
  this->http_init(url_with_auth);
  if (!this->check_status()) {
    this->http_end();
    return false;
  }
  int length = this->body_length_;
  if (length < 0) {
    this->http_end();
    return false;
  }
  if (length < MD5_SIZE) {
    ESP_LOGE(TAG, "MD5 file must be %u bytes; %u bytes reported by HTTP server. Aborting", MD5_SIZE,
             this->body_length_);
    this->http_end();
    return false;
  }

  this->bytes_read_ = 0;
  this->md5_expected_.resize(MD5_SIZE);
  auto read_len = this->http_read((uint8_t *) this->md5_expected_.data(), MD5_SIZE);
  this->http_end();

  return read_len == MD5_SIZE;
}

bool OtaHttpRequestComponent::validate_url_(const std::string &url) {
  if ((url.length() < 8) || (url.find("http") != 0) || (url.find("://") == std::string::npos)) {
    ESP_LOGE(TAG, "URL is invalid and/or must be prefixed with 'http://' or 'https://'");
    return false;
  }
  return true;
}

}  // namespace http_request
}  // namespace esphome
