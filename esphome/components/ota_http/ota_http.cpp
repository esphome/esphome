#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/ota/ota_backend_arduino_esp32.h"
#include "esphome/components/ota/ota_backend_arduino_esp8266.h"
#include "esphome/components/ota/ota_backend_arduino_rp2040.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"
#include "esphome/components/ota/ota_backend.h"
#include "ota_http.h"

namespace esphome {
namespace ota_http {

std::unique_ptr<ota::OTABackend> make_ota_backend() {
#ifdef USE_ESP8266
  ESP_LOGD(TAG, "Using ArduinoESP8266OTABackend");
  return make_unique<ota::ArduinoESP8266OTABackend>();
#endif  // USE_ESP8266

#ifdef USE_ARDUINO
#ifdef USE_ESP32
  ESP_LOGD(TAG, "Using ArduinoESP32OTABackend");
  return make_unique<ota::ArduinoESP32OTABackend>();
#endif  // USE_ESP32
#endif  // USE_ARDUINO

#ifdef USE_ESP_IDF
  ESP_LOGD(TAG, "Using IDFOTABackend");
  return make_unique<ota::IDFOTABackend>();
#endif  // USE_ESP_IDF
#ifdef USE_RP2040
  ESP_LOGD(TAG, "Using ArduinoRP2040OTABackend");
  return make_unique<ota::ArduinoRP2040OTABackend>();
#endif  // USE_RP2040
  ESP_LOGE(TAG, "No OTA backend!");
}

const std::unique_ptr<ota::OTABackend> OtaHttpComponent::BACKEND = make_ota_backend();

void OtaHttpComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OTA_http:");
  ESP_LOGCONFIG(TAG, "  Timeout: %llums", (uint64_t) this->timeout_);
};

void OtaHttpComponent::flash() {
  if (this->pref_.ota_http_state != OTA_HTTP_STATE_SAFE_MODE) {
    ESP_LOGV(TAG, "Setting mode to progress");
    this->pref_.ota_http_state = OTA_HTTP_STATE_PROGRESS;
    this->pref_obj_.save(&this->pref_);
  }

  global_preferences->sync();

#ifdef OTA_HTTP_ONLY_AT_BOOT
  if (this->pref_.ota_http_state != OTA_HTTP_STATE_SAFE_MODE) {
    ESP_LOGI(TAG, "Rebooting before flashing new firmware.");
    App.safe_reboot();
  }
#endif

  uint32_t update_start_time = millis();
  uint8_t buf[this->http_recv_buffer_ + 1];
  int error_code = 0;
  uint32_t last_progress = 0;
  esphome::md5::MD5Digest md5_receive;
  std::unique_ptr<char[]> md5_receive_str(new char[33]);
  if (!this->http_init()) {
    return;
  }

  // we will compute md5 on the fly
  // TODO: better security if fetched from the http server
  md5_receive.init();
  ESP_LOGV(TAG, "md5sum from received data initialized.");

  error_code = esphome::ota_http::OtaHttpComponent::BACKEND->begin(this->body_length_);
  if (error_code != 0) {
    ESP_LOGW(TAG, "BACKEND->begin error: %d", error_code);
    this->cleanup_();
    return;
  }
  ESP_LOGV(TAG, "OTA backend begin");

  while (this->bytes_read_ != this->body_length_) {
    // read a maximum of chunk_size bytes into buf. (real read size returned)
    int bufsize = this->http_read(buf, this->http_recv_buffer_);

    if (bufsize < 0) {
      ESP_LOGE(TAG, "ERROR: stream closed");
      this->cleanup_();
      return;
    }

    // add read bytes to md5
    md5_receive.add(buf, bufsize);

    // write bytes to OTA backend
    this->update_started_ = true;
    error_code = esphome::ota_http::OtaHttpComponent::BACKEND->write(buf, bufsize);
    if (error_code != 0) {
      // error code explaination available at
      // https://github.com/esphome/esphome/blob/dev/esphome/components/ota/ota_component.h
      ESP_LOGE(TAG, "Error code (%d) writing binary data to flash at offset %d and size %d", error_code,
               this->bytes_read_ - bufsize, this->body_length_);
      this->cleanup_();
      return;
    }

    uint32_t now = millis();
    if ((now - last_progress > 1000) or (this->bytes_read_ == this->body_length_)) {
      last_progress = now;
      ESP_LOGI(TAG, "Progress: %0.1f%%", this->bytes_read_ * 100. / this->body_length_);
      // feed watchdog and give other tasks a chance to run
      esphome::App.feed_wdt();
      yield();
    }
  }  // while

  ESP_LOGI(TAG, "Done in %.0f secs", float(millis() - update_start_time) / 1000);

  // send md5 to backend (backend will check that the flashed one has the same)
  md5_receive.calculate();
  md5_receive.get_hex(md5_receive_str.get());
  ESP_LOGD(TAG, "md5sum recieved: %s (size %d)", md5_receive_str.get(), this->bytes_read_);
  esphome::ota_http::OtaHttpComponent::BACKEND->set_update_md5(md5_receive_str.get());

  this->http_end();

  // feed watchdog and give other tasks a chance to run
  esphome::App.feed_wdt();
  yield();
  delay(100);  // NOLINT

  error_code = esphome::ota_http::OtaHttpComponent::BACKEND->end();
  if (error_code != 0) {
    ESP_LOGE(TAG, "Error ending OTA!, error_code: %d", error_code);
    this->cleanup_();
    return;
  }

  this->pref_.ota_http_state = OTA_HTTP_STATE_OK;
  this->pref_obj_.save(&this->pref_);
  delay(10);
  ESP_LOGI(TAG, "OTA update finished! Rebooting...");
  delay(10);
  esphome::App.safe_reboot();
}

void OtaHttpComponent::cleanup_() {
  if (this->update_started_) {
    ESP_LOGE(TAG, "Abort OTA backend");
    esphome::ota_http::OtaHttpComponent::BACKEND->abort();
  }
  ESP_LOGE(TAG, "Aborting http connection");
  this->http_end();
  ESP_LOGE(TAG, "Previous safe mode unsuccessful; skipped ota_http");
  this->pref_.ota_http_state = OTA_HTTP_STATE_ABORT;
  this->pref_obj_.save(&this->pref_);
};

void OtaHttpComponent::check_upgrade() {
  // function called at boot time if CONF_SAFE_MODE is True or "fallback"
  if (this->pref_obj_.load(&this->pref_)) {
    if (this->pref_.ota_http_state == OTA_HTTP_STATE_PROGRESS) {
      // progress at boot time means that there was a problem

      // Delay here to allow power to stabilise before Wi-Fi/Ethernet is initialised.
      delay(300);  // NOLINT
      App.setup();

      ESP_LOGI(TAG, "Previous ota_http unsuccessful. Retrying");
      this->pref_.ota_http_state = OTA_HTTP_STATE_SAFE_MODE;
      this->pref_obj_.save(&this->pref_);
      this->flash();
      return;
    }
    if (this->pref_.ota_http_state == OTA_HTTP_STATE_SAFE_MODE) {
      ESP_LOGE(TAG, "Previous safe mode unsuccessful; skipped ota_http");
      this->pref_.ota_http_state = OTA_HTTP_STATE_ABORT;
      this->pref_obj_.save(&this->pref_);
      global_preferences->sync();
    }
  }
}

}  // namespace ota_http
}  // namespace esphome
