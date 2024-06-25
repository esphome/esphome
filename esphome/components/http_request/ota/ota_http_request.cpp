#include "ota_http_request.h"
#include "../watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include "esphome/components/md5/md5.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_arduino_esp32.h"
#include "esphome/components/ota/ota_backend_arduino_esp8266.h"
#include "esphome/components/ota/ota_backend_arduino_rp2040.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.ota";

void OtaHttpRequestComponent::setup() {
#ifdef USE_OTA_STATE_CALLBACK
  ota::register_ota_platform(this);
#endif
}

void OtaHttpRequestComponent::dump_config() { ESP_LOGCONFIG(TAG, "Over-The-Air updates via HTTP request"); };

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

void OtaHttpRequestComponent::cleanup_(std::unique_ptr<ota::OTABackend> backend,
                                       const std::shared_ptr<HttpContainer> &container) {
  if (this->update_started_) {
    ESP_LOGV(TAG, "Aborting OTA backend");
    backend->abort();
  }
  ESP_LOGV(TAG, "Aborting HTTP connection");
  container->end();
};

uint8_t OtaHttpRequestComponent::do_ota_() {
  uint8_t buf[OtaHttpRequestComponent::HTTP_RECV_BUFFER + 1];
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

  auto container = this->parent_->get(url_with_auth);

  if (container == nullptr) {
    return OTA_CONNECTION_ERROR;
  }

  // we will compute MD5 on the fly for verification -- Arduino OTA seems to ignore it
  md5_receive.init();
  ESP_LOGV(TAG, "MD5Digest initialized");

  ESP_LOGV(TAG, "OTA backend begin");
  auto backend = ota::make_ota_backend();
  auto error_code = backend->begin(container->content_length);
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "backend->begin error: %d", error_code);
    this->cleanup_(std::move(backend), container);
    return error_code;
  }

  while (container->get_bytes_read() < container->content_length) {
    // read a maximum of chunk_size bytes into buf. (real read size returned)
    int bufsize = container->read(buf, OtaHttpRequestComponent::HTTP_RECV_BUFFER);
    ESP_LOGVV(TAG, "bytes_read_ = %u, body_length_ = %u, bufsize = %i", container->get_bytes_read(),
              container->content_length, bufsize);

    // feed watchdog and give other tasks a chance to run
    App.feed_wdt();
    yield();

    if (bufsize < 0) {
      ESP_LOGE(TAG, "Stream closed");
      this->cleanup_(std::move(backend), container);
      return OTA_CONNECTION_ERROR;
    } else if (bufsize > 0 && bufsize <= OtaHttpRequestComponent::HTTP_RECV_BUFFER) {
      // add read bytes to MD5
      md5_receive.add(buf, bufsize);

      // write bytes to OTA backend
      this->update_started_ = true;
      error_code = backend->write(buf, bufsize);
      if (error_code != ota::OTA_RESPONSE_OK) {
        // error code explanation available at
        // https://github.com/esphome/esphome/blob/dev/esphome/components/ota/ota_backend.h
        ESP_LOGE(TAG, "Error code (%02X) writing binary data to flash at offset %d and size %d", error_code,
                 container->get_bytes_read() - bufsize, container->content_length);
        this->cleanup_(std::move(backend), container);
        return error_code;
      }
    }

    uint32_t now = millis();
    if ((now - last_progress > 1000) or (container->get_bytes_read() == container->content_length)) {
      last_progress = now;
      float percentage = container->get_bytes_read() * 100.0f / container->content_length;
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
    this->cleanup_(std::move(backend), container);
    return ota::OTA_RESPONSE_ERROR_MD5_MISMATCH;
  } else {
    backend->set_update_md5(md5_receive_str.get());
  }

  container->end();

  // feed watchdog and give other tasks a chance to run
  App.feed_wdt();
  yield();
  delay(100);  // NOLINT

  error_code = backend->end();
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending update! error_code: %d", error_code);
    this->cleanup_(std::move(backend), container);
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
  auto container = this->parent_->get(url_with_auth);
  if (container == nullptr) {
    ESP_LOGE(TAG, "Failed to connect to MD5 URL");
    return false;
  }
  size_t length = container->content_length;
  if (length == 0) {
    container->end();
    return false;
  }
  if (length < MD5_SIZE) {
    ESP_LOGE(TAG, "MD5 file must be %u bytes; %u bytes reported by HTTP server. Aborting", MD5_SIZE, length);
    container->end();
    return false;
  }

  this->md5_expected_.resize(MD5_SIZE);
  int read_len = 0;
  while (container->get_bytes_read() < MD5_SIZE) {
    read_len = container->read((uint8_t *) this->md5_expected_.data(), MD5_SIZE);
    App.feed_wdt();
    yield();
  }
  container->end();

  ESP_LOGV(TAG, "Read len: %u, MD5 expected: %u", read_len, MD5_SIZE);
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
