#include "ota_coap_client_component.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include "esphome/components/watchdog/watchdog.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"

namespace esphome::coap {

static const char *const TAG = "coap_client.ota";

void OtaCoapClientComponent::setup() {
#ifdef USE_OTA_STATE_CALLBACK
  ota::register_ota_platform(this);
#endif
}

void OtaCoapClientComponent::dump_config() { ESP_LOGCONFIG(TAG, "Over-The-Air updates via Coap Client"); };

void OtaCoapClientComponent::set_md5_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->md5_url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->md5_url_ = url;
  this->md5_expected_.clear();  // to be retrieved later
}

void OtaCoapClientComponent::set_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->url_ = url;
}

void OtaCoapClientComponent::flash() {
  if (this->url_.empty()) {
    ESP_LOGE(TAG, "URL not set; cannot start update");
    return;
  }

  ESP_LOGI(TAG, "Starting update");
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

void OtaCoapClientComponent::abort() {
  if (this->image_download_started_) {
    ESP_LOGV(TAG, "Aborting OTA backend");
    this->backend_->abort();
    this->image_download_abort_ = true;
    this->image_download_started_ = false;
    this->image_download_ready_ = true;
  }
};

uint8_t OtaCoapClientComponent::do_ota_() {
  uint32_t update_start_time = millis();
  std::unique_ptr<char[]> md5_receive_str(new char[MD5_SIZE + 1]);
  md5_receive_str[MD5_SIZE] = '\0';
  this->md5_expected_ = this->md5_;
  if (this->md5_expected_.empty() && !this->get_md5_expected_()) {
    ESP_LOGE(TAG, "MD5 Invalid %s", this->md5_expected_.c_str());
    return OTA_MD5_INVALID;
  }
  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());
  // feed watchdog and give other tasks a chance to run
  App.feed_wdt();
  yield();
  delay(100);  // NOLINT

  ESP_LOGI(TAG, "Get: %s", this->url_.c_str());

  this->image_download_started_ = true;
  this->image_download_abort_ = false;
  this->image_download_ready_ = false;

  // we will compute MD5 on the fly for verification -- Arduino OTA seems to ignore it
  this->md5_receive_.init();
  ESP_LOGV(TAG, "MD5Digest initialized");

  ESP_LOGV(TAG, "OTA backend begin");
  this->backend_ = ota::make_ota_backend();
  auto error_code = this->backend_->begin(OTA_SIZE_UNKNOWN);
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "backend->begin error: %d", error_code);
    this->abort();
    return error_code;
  }

  this->parent_->get(this->url_, OtaCoapClientComponent::image_callback, this);
  while (!this->image_download_ready_) {
    App.feed_wdt();
    yield();
    delay(100);  // NOLINT
  }

  ESP_LOGI(TAG, "Done in %.0f seconds", float(millis() - update_start_time) / 1000);

  if (this->is_image_download_abort()) {
    return OTA_ABORT;
  }

  // verify MD5 is as expected and act accordingly
  this->md5_receive_.calculate();
  this->md5_receive_.get_hex(md5_receive_str.get());
  this->md5_computed_ = md5_receive_str.get();
  if (strncmp(this->md5_computed_.c_str(), this->md5_expected_.c_str(), MD5_SIZE) != 0) {
    ESP_LOGE(TAG, "MD5 expected: %s computed: %s - Aborting due to MD5 mismatch", this->md5_expected_.c_str(),
             this->md5_computed_.c_str());
    this->abort();
    return ota::OTA_RESPONSE_ERROR_MD5_MISMATCH;
  } else {
    this->backend_->set_update_md5(md5_receive_str.get());
  }

  // feed watchdog and give other tasks a chance to run
  App.feed_wdt();
  yield();
  delay(100);  // NOLINT

  error_code = this->backend_->end();
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending update! error_code: %d", error_code);
    this->abort();
    return error_code;
  }

  ESP_LOGI(TAG, "Update complete");
  return ota::OTA_RESPONSE_OK;
}

bool OtaCoapClientComponent::get_md5_expected_() {
  set_md5_url_ready(false);
  if (this->md5_url_.empty()) {
    return false;
  }
  ESP_LOGI(TAG, "Get: %s", this->md5_url_.c_str());
  this->parent_->get(this->md5_url_, OtaCoapClientComponent::md5_callback, this);
  while (!this->md5_url_ready_) {
    App.feed_wdt();
    yield();
    delay(100);  // NOLINT
  }
  return this->md5_expected().size() == MD5_SIZE;
}

void OtaCoapClientComponent::append_image(const uint8_t *block, uint16_t length, uint32_t offset) {
  if (length < 0) {
    ESP_LOGE(TAG, "CoAP closed");
    this->abort();
  } else {
    // add read bytes to MD5
    this->md5_receive_.add(block, length);
    // write bytes to OTA backend
    uint8_t *non_const_ptr = const_cast<uint8_t *>(block);
    auto error_code = this->backend_->write(non_const_ptr, length, offset);
    if (error_code != ota::OTA_RESPONSE_OK) {
      // error code explanation available at
      // https://github.com/esphome/esphome/blob/dev/esphome/components/ota/ota_backend.h
      ESP_LOGE(TAG, "Error code (%02X) writing binary data to flash");
      this->abort();
    }
#ifdef USE_OTA_STATE_CALLBACK
    // don't know percentag because we did not get size
    // this->state_callback_.call(ota::OTA_IN_PROGRESS, percentage, 0);
#endif
  }
}

bool OtaCoapClientComponent::validate_url_(const std::string &url) {
  /*
  if ((url.length() < 8) || !url.starts_with("coap") || (url.find("://") == std::string::npos)) {
    ESP_LOGE(TAG, "URL is invalid allowed schemes: coap[s][+tcp|+ws]");
    return false;
  }
  */
  return true;
}

void OtaCoapClientComponent::md5_callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset,
                                          size_t total, void *context) {
  ESP_LOGD(TAG, "md5_callback %d, %d, %d", len, offset, total);
  OtaCoapClientComponent *obj = (OtaCoapClientComponent *) context;
  if (!obj->is_md5_url_ready()) {
    // assume you get the 32 bytes back on first callback, just some len checking.
    if (len >= 32) {
      obj->append_md5_expected(data, 32);
    }
    obj->set_md5_url_ready(true);
  }
}

void OtaCoapClientComponent::image_callback(uint16_t response_code, const unsigned char *data, size_t len,
                                            size_t offset, size_t total, void *context) {
  OtaCoapClientComponent *obj = (OtaCoapClientComponent *) context;
  if (offset % 102400 == 0) {
    ESP_LOGD(TAG, "image_callback %d, %d", len, offset);
  }
  if (!obj->is_image_download_ready()) {
    if (len <= 0) {
      obj->abort();
    } else {
      obj->append_image(data, len, offset);
    }
    if (len + offset == total) {
      ESP_LOGD(TAG, "image_callback %d, %d %d", len, offset, len + offset);
      obj->set_image_download_ready(true);
    }
  }
}

}  // namespace esphome::coap
