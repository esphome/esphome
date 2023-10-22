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

std::unique_ptr<ota::OTABackend> OtaHttpComponent::backend_ = make_ota_backend();

void OtaHttpComponent::flash() {
  unsigned long update_start_time = millis();
  const size_t chunk_size = 1024;  // must be =< HTTP_TCP_BUFFER_SIZE;
  uint8_t buf[chunk_size + 1];
  int error_code = 0;
  unsigned long last_progress = 0;
  esphome::md5::MD5Digest md5_receive;
  char *md5_receive_str = new char[33];

  if (!this->http_init()) {
    return;
  }

  // we will compute md5 on the fly
  // TODO: better security if fetched from the http server
  md5_receive.init();
  ESP_LOGV(TAG, "md5sum from received data initialized.");

  error_code = this->backend_->begin(this->body_length);
  if (error_code != 0) {
    ESP_LOGW(TAG, "this->backend_->begin error: %d", error_code);
    this->cleanup();
    return;
  }
  ESP_LOGV(TAG, "OTA backend begin");

  while (this->bytes_read != this->body_length) {
    // read a maximum of chunk_size bytes into buf. (real read size returned)
    size_t bufsize = this->http_read(buf, chunk_size);

    // add read bytes to md5
    md5_receive.add(buf, bufsize);

    // write bytes to OTA backend
    this->update_started_ = true;
    error_code = this->backend_->write(buf, bufsize);
    if (error_code != 0) {
      // error code explaination available at
      // https://github.com/esphome/esphome/blob/dev/esphome/components/ota/ota_component.h
      ESP_LOGE(TAG, "Error code (%d) writing binary data to flash at offset %d and size %d", error_code,
               this->bytes_read - bufsize, this->body_length);
      this->cleanup();
      return;
    }

    unsigned long now = millis();
    if ((now - last_progress > 1000) or (this->bytes_read == this->body_length)) {
      last_progress = now;
      ESP_LOGI(TAG, "Progress: %0.1f%%", this->bytes_read * 100. / this->body_length);
      // feed watchdog and give other tasks a chance to run
      esphome::App.feed_wdt();
      yield();
    }
  }  // while

  ESP_LOGI(TAG, "Done in %.0f secs", float(millis() - update_start_time) / 1000);

  // send md5 to backend (backend will check that the flashed one has the same)
  md5_receive.calculate();
  md5_receive.get_hex(md5_receive_str);
  ESP_LOGD(TAG, "md5sum recieved: %s (size %d)", md5_receive_str, bytes_read);
  this->backend_->set_update_md5(md5_receive_str);

  this->http_end();

  delete[] md5_receive_str;

  // feed watchdog and give other tasks a chance to run
  esphome::App.feed_wdt();
  yield();
  delay(100);

  error_code = this->backend_->end();
  if (error_code != 0) {
    ESP_LOGE(TAG, "Error ending OTA!, error_code: %d", error_code);
    this->cleanup();
    return;
  }

  delay(10);
  ESP_LOGI(TAG, "OTA update finished! Rebooting...");
  delay(100);  // NOLINT
  esphome::App.safe_reboot();
  // new firmware flashed!
  return;
}

}  // namespace ota_http
}  // namespace esphome
