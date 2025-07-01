#include "web_server_base.h"
#ifdef USE_NETWORK
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ARDUINO
#include <StreamString.h>
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
#include <Update.h>
#endif
#ifdef USE_ESP8266
#include <Updater.h>
#endif
#endif

#if defined(USE_ESP_IDF) && defined(USE_WEBSERVER_OTA)
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#endif

namespace esphome {
namespace web_server_base {

static const char *const TAG = "web_server_base";

#if defined(USE_ESP_IDF) && defined(USE_WEBSERVER_OTA)
// Minimal OTA backend implementation for web server
// This allows OTA updates via web server without requiring the OTA component
// TODO: In the future, this should be refactored into a common ota_base component
// that both web_server and ota components can depend on, avoiding code duplication
// while keeping the components independent. This would allow both ESP-IDF and Arduino
// implementations to share the base OTA functionality without requiring the full OTA component.
// The IDFWebServerOTABackend class is intentionally designed with the same interface
// as OTABackend to make it easy to swap to using OTABackend when the ota component
// is split into ota and ota_base in the future.
class IDFWebServerOTABackend {
 public:
  bool begin() {
    this->partition_ = esp_ota_get_next_update_partition(nullptr);
    if (this->partition_ == nullptr) {
      ESP_LOGE(TAG, "No OTA partition available");
      return false;
    }

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    // The following function takes longer than the default timeout of WDT due to flash erase
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t wdtc;
    wdtc.idle_core_mask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
    wdtc.idle_core_mask |= (1 << 0);
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
    wdtc.idle_core_mask |= (1 << 1);
#endif
    wdtc.timeout_ms = 15000;
    wdtc.trigger_panic = false;
    esp_task_wdt_reconfigure(&wdtc);
#else
    esp_task_wdt_init(15, false);
#endif
#endif

    esp_err_t err = esp_ota_begin(this->partition_, 0, &this->update_handle_);

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    // Set the WDT back to the configured timeout
#if ESP_IDF_VERSION_MAJOR >= 5
    wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
    esp_task_wdt_reconfigure(&wdtc);
#else
    esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, false);
#endif
#endif

    if (err != ESP_OK) {
      esp_ota_abort(this->update_handle_);
      this->update_handle_ = 0;
      ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
      return false;
    }
    return true;
  }

  bool write(uint8_t *data, size_t len) {
    esp_err_t err = esp_ota_write(this->update_handle_, data, len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
      return false;
    }
    return true;
  }

  bool end() {
    esp_err_t err = esp_ota_end(this->update_handle_);
    this->update_handle_ = 0;
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
      return false;
    }

    err = esp_ota_set_boot_partition(this->partition_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
      return false;
    }

    return true;
  }

  void abort() {
    if (this->update_handle_ != 0) {
      esp_ota_abort(this->update_handle_);
      this->update_handle_ = 0;
    }
  }

 private:
  esp_ota_handle_t update_handle_{0};
  const esp_partition_t *partition_{nullptr};
};
#endif

void WebServerBase::add_handler(AsyncWebHandler *handler) {
  // remove all handlers

  if (!credentials_.username.empty()) {
    handler = new internal::AuthMiddlewareHandler(handler, &credentials_);
  }
  this->handlers_.push_back(handler);
  if (this->server_ != nullptr) {
    this->server_->addHandler(handler);
  }
}

#ifdef USE_WEBSERVER_OTA
void OTARequestHandler::report_ota_progress_(AsyncWebServerRequest *request) {
  const uint32_t now = millis();
  if (now - this->last_ota_progress_ > 1000) {
    if (request->contentLength() != 0) {
      float percentage = (this->ota_read_length_ * 100.0f) / request->contentLength();
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
    } else {
      ESP_LOGD(TAG, "OTA in progress: %u bytes read", this->ota_read_length_);
    }
    this->last_ota_progress_ = now;
  }
}

void OTARequestHandler::schedule_ota_reboot_() {
  ESP_LOGI(TAG, "OTA update successful!");
  this->parent_->set_timeout(100, []() {
    ESP_LOGI(TAG, "Performing OTA reboot now");
    App.safe_reboot();
  });
}

void OTARequestHandler::ota_init_(const char *filename) {
  ESP_LOGI(TAG, "OTA Update Start: %s", filename);
  this->ota_read_length_ = 0;
}

void report_ota_error() {
#ifdef USE_ARDUINO
  StreamString ss;
  Update.printError(ss);
  ESP_LOGW(TAG, "OTA Update failed! Error: %s", ss.c_str());
#endif
}

void OTARequestHandler::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index,
                                     uint8_t *data, size_t len, bool final) {
#ifdef USE_ARDUINO
  bool success;
  if (index == 0) {
    this->ota_init_(filename.c_str());
#ifdef USE_ESP8266
    Update.runAsync(true);
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    success = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
#endif
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_LIBRETINY)
    if (Update.isRunning()) {
      Update.abort();
    }
    success = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
#endif
    if (!success) {
      report_ota_error();
      return;
    }
  } else if (Update.hasError()) {
    // don't spam logs with errors if something failed at start
    return;
  }

  success = Update.write(data, len) == len;
  if (!success) {
    report_ota_error();
    return;
  }
  this->ota_read_length_ += len;
  this->report_ota_progress_(request);

  if (final) {
    if (Update.end(true)) {
      this->schedule_ota_reboot_();
    } else {
      report_ota_error();
    }
  }
#endif  // USE_ARDUINO

#ifdef USE_ESP_IDF
  // ESP-IDF implementation
  if (index == 0 && !this->ota_backend_) {
    // Initialize OTA on first call
    this->ota_init_(filename.c_str());
    this->ota_success_ = false;

    auto *backend = new IDFWebServerOTABackend();
    if (!backend->begin()) {
      ESP_LOGE(TAG, "OTA begin failed");
      delete backend;
      return;
    }
    this->ota_backend_ = backend;
  }

  auto *backend = static_cast<IDFWebServerOTABackend *>(this->ota_backend_);
  if (!backend) {
    return;
  }

  // Process data
  if (len > 0) {
    if (!backend->write(data, len)) {
      ESP_LOGE(TAG, "OTA write failed");
      backend->abort();
      delete backend;
      this->ota_backend_ = nullptr;
      return;
    }
    this->ota_read_length_ += len;
    this->report_ota_progress_(request);
  }

  // Finalize
  if (final) {
    this->ota_success_ = backend->end();
    if (this->ota_success_) {
      this->schedule_ota_reboot_();
    } else {
      ESP_LOGE(TAG, "OTA end failed");
    }
    delete backend;
    this->ota_backend_ = nullptr;
  }
#endif  // USE_ESP_IDF
}

void OTARequestHandler::handleRequest(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response;
#ifdef USE_ARDUINO
  if (!Update.hasError()) {
    response = request->beginResponse(200, "text/plain", "Update Successful!");
  } else {
    StreamString ss;
    ss.print("Update Failed: ");
    Update.printError(ss);
    response = request->beginResponse(200, "text/plain", ss);
  }
#endif  // USE_ARDUINO
#ifdef USE_ESP_IDF
  // Send response based on the OTA result
  response = request->beginResponse(200, "text/plain", this->ota_success_ ? "Update Successful!" : "Update Failed!");
#endif  // USE_ESP_IDF
  response->addHeader("Connection", "close");
  request->send(response);
}

void WebServerBase::add_ota_handler() {
  this->add_handler(new OTARequestHandler(this));  // NOLINT
}
#endif

float WebServerBase::get_setup_priority() const {
  // Before WiFi (captive portal)
  return setup_priority::WIFI + 2.0f;
}

}  // namespace web_server_base
}  // namespace esphome
#endif
