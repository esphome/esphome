#include "ota_web_server.h"
#ifdef USE_WEBSERVER_OTA

#include "esphome/components/ota/ota_backend_factory.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

#if USE_ESP32
using PlatformString = std::string;
#elif USE_ARDUINO
using PlatformString = String;
#endif

namespace esphome::web_server {

ESPHOME_LOG_TAG(TAG, "web_server.ota");

class OTARequestHandler : public AsyncWebHandler {
 public:
  OTARequestHandler(WebServerOTAComponent *parent) : parent_(parent) {}
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index, uint8_t *data,
                    size_t len, bool final) override;
  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST)
      return false;
    // Check if this is an OTA update request
#ifdef USE_ESP32
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    bool is_ota_request = request->url_to(url_buf) == "/update";
#else
    bool is_ota_request = request->url() == ESPHOME_F("/update");
#endif

#if defined(USE_WEBSERVER_OTA_DISABLED) && defined(USE_CAPTIVE_PORTAL)
    // IMPORTANT: USE_WEBSERVER_OTA_DISABLED only disables OTA for the web_server component
    // Captive portal can still perform OTA updates - check if request is from active captive portal
    // Note: global_captive_portal is the standard way components communicate in ESPHome
    return is_ota_request && captive_portal::global_captive_portal != nullptr &&
           captive_portal::global_captive_portal->is_active();
#elif defined(USE_WEBSERVER_OTA_DISABLED)
    // OTA disabled for web_server and no captive portal compiled in
    return false;
#else
    // OTA enabled for web_server
    return is_ota_request;
#endif
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  void report_ota_progress_(AsyncWebServerRequest *request);
  void schedule_ota_reboot_();
  void ota_init_(const char *filename);
  /// Abort and drop the backend, releasing the lock.
  void abort_session_();
  /// Reply "busy" without touching the running upload.
  void refuse_(AsyncWebServerRequest *request);

  uint32_t last_ota_progress_{0};
  uint32_t ota_read_length_{0};
  WebServerOTAComponent *parent_;
  bool ota_success_{false};

 private:
  ota::OTABackendPtr ota_backend_{nullptr};
  /// Request that owns ota_backend_.
  const AsyncWebServerRequest *ota_request_{nullptr};
  const AsyncWebServerRequest *refused_request_{nullptr};
};

void OTARequestHandler::report_ota_progress_(AsyncWebServerRequest *request) {
  const uint32_t now = millis();
  if (now - this->last_ota_progress_ > 1000) {
    float percentage = 0.0f;
    if (request->contentLength() != 0) {
      // Note: Using contentLength() for progress calculation is technically wrong as it includes
      // multipart headers/boundaries, but it's only off by a small amount and we don't have
      // access to the actual firmware size until the upload is complete. This is intentional
      // as it still gives the user a reasonable progress indication.
      percentage = (this->ota_read_length_ * 100.0f) / request->contentLength();
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
    } else {
      ESP_LOGD(TAG, "OTA in progress: %" PRIu32 " bytes read", this->ota_read_length_);
    }
#ifdef USE_OTA_STATE_LISTENER
    // Report progress - use notify_state_deferred_ since we're in web server task
    this->parent_->notify_state_deferred_(ota::OTA_IN_PROGRESS, percentage, 0);
#endif
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
  this->ota_success_ = false;
}

void OTARequestHandler::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                     uint8_t *data, size_t len, bool final) {
  ota::OTAResponseTypes error_code = ota::OTA_RESPONSE_OK;

  // First byte of a new upload: index==0 with actual data. (web_server_idf
  // fires a separate start-marker call with data==nullptr/len==0 before the
  // first real chunk; gate on len>0 so we only trigger once per upload.)
  if (index == 0 && len > 0) {
    // A stalled upload times out and onDisconnect() frees it, so refusing can't block for long.
    if (this->ota_backend_) {
      ESP_LOGW(TAG, "OTA upload refused: another upload is in progress");
      this->refuse_(request);
      return;
    }

    this->ota_backend_ = ota::make_ota_backend();
    if (!this->ota_backend_) {
      ESP_LOGE(TAG, "Failed to create OTA backend");
#ifdef USE_OTA_STATE_LISTENER
      this->parent_->notify_state_deferred_(ota::OTA_ERROR, 0.0f,
                                            static_cast<uint8_t>(ota::OTA_RESPONSE_ERROR_UNKNOWN));
#endif
      return;
    }

    // Web server OTA uses multipart uploads where the actual firmware size
    // is unknown (contentLength includes multipart overhead)
    // Pass 0 to indicate unknown size
    error_code = this->ota_backend_->begin(0);
    if (error_code == ota::OTA_RESPONSE_ERROR_UPDATE_BUSY) {
      // Nothing started: no state notifications.
      ESP_LOGW(TAG, "OTA upload refused: another update is in progress");
      this->ota_backend_.reset();
      this->refuse_(request);
      return;
    }

    // Initialize OTA on first call
    this->ota_init_(filename.c_str());

#ifdef USE_OTA_STATE_LISTENER
    // Notify OTA started - use notify_state_deferred_ since we're in web server task
    this->parent_->notify_state_deferred_(ota::OTA_STARTED, 0.0f, 0);
#endif

    if (error_code != ota::OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "OTA begin failed: %d", error_code);
      this->ota_backend_.reset();
#ifdef USE_OTA_STATE_LISTENER
      this->parent_->notify_state_deferred_(ota::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
      return;
    }

    // An interrupted upload gets no final chunk.
    this->ota_request_ = request;
    request->onDisconnect([this, request]() {
      if (this->ota_request_ == request && this->ota_backend_) {
        ESP_LOGW(TAG, "OTA upload interrupted; aborting");
        this->abort_session_();
      }
    });
  }

  if (!this->ota_backend_ || request != this->ota_request_) {
    return;
  }

  // Process data
  if (len > 0) {
    error_code = this->ota_backend_->write(data, len);
    if (error_code != ota::OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "OTA write failed: %d", error_code);
      this->ota_backend_->abort();
      this->ota_backend_.reset();
      this->ota_request_ = nullptr;
#ifdef USE_OTA_STATE_LISTENER
      this->parent_->notify_state_deferred_(ota::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
      return;
    }
    this->ota_read_length_ += len;
    this->report_ota_progress_(request);
  }

  // Finalize
  if (final) {
    ESP_LOGD(TAG, "OTA final chunk: index=%zu, len=%zu, total_read=%" PRIu32 ", contentLength=%zu", index, len,
             this->ota_read_length_, request->contentLength());

    // For Arduino framework, the Update library tracks expected size from firmware header
    // If we haven't received enough data, calling end() will fail
    // This can happen if the upload is interrupted or the client disconnects
    error_code = this->ota_backend_->end();
    if (error_code == ota::OTA_RESPONSE_OK) {
      this->ota_success_ = true;
#ifdef USE_OTA_STATE_LISTENER
      // Report completion before reboot - use notify_state_deferred_ since we're in web server task
      this->parent_->notify_state_deferred_(ota::OTA_COMPLETED, 100.0f, 0);
#endif
      this->schedule_ota_reboot_();
    } else {
      ESP_LOGE(TAG, "OTA end failed: %d", error_code);
      // Reset the platform updater before the lock is released.
      this->ota_backend_->abort();
#ifdef USE_OTA_STATE_LISTENER
      this->parent_->notify_state_deferred_(ota::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
    }
    this->ota_backend_.reset();
    this->ota_request_ = nullptr;
  }
}

void OTARequestHandler::abort_session_() {
  this->ota_backend_->abort();
#ifdef USE_OTA_STATE_LISTENER
  this->parent_->notify_state_deferred_(ota::OTA_ABORT, 0.0f, 0);
#endif
  this->ota_backend_.reset();
  this->ota_request_ = nullptr;
}

void OTARequestHandler::refuse_(AsyncWebServerRequest *request) {
  this->refused_request_ = request;
  request->onDisconnect([this, request]() {
    if (this->refused_request_ == request)
      this->refused_request_ = nullptr;
  });
}

void OTARequestHandler::handleRequest(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response;
  // Use the ota_success_ flag to determine the actual result
  const bool refused = request == this->refused_request_;
  if (refused)
    this->refused_request_ = nullptr;
#ifdef USE_ESP8266
  static const char UPDATE_SUCCESS[] PROGMEM = "Update Successful!";
  static const char UPDATE_FAILED[] PROGMEM = "Update Failed!";
  static const char UPDATE_BUSY[] PROGMEM = "Update Failed! Another update is in progress, retry later.";
  static const char TEXT_PLAIN[] PROGMEM = "text/plain";
  static const char CONNECTION_STR[] PROGMEM = "Connection";
  static const char CLOSE_STR[] PROGMEM = "close";
  const char *msg = refused ? UPDATE_BUSY : (this->ota_success_ ? UPDATE_SUCCESS : UPDATE_FAILED);
  response = request->beginResponse_P(200, TEXT_PLAIN, msg);
  response->addHeader(CONNECTION_STR, CLOSE_STR);
#else
  const char *msg = refused              ? "Update Failed! Another update is in progress, retry later."
                    : this->ota_success_ ? "Update Successful!"
                                         : "Update Failed!";
  response = request->beginResponse(200, "text/plain", msg);
  response->addHeader("Connection", "close");
#endif
  request->send(response);
}

void WebServerOTAComponent::setup() {
  // Get the global web server base instance and register our handler
  auto *base = web_server_base::global_web_server_base;
  if (base == nullptr) {
    ESP_LOGE(TAG, "WebServerBase not found");
    this->mark_failed();
    return;
  }

  // The handler lives for the life of the process; WebServerBase never destroys its server
  base->add_handler(new OTARequestHandler(this));  // NOLINT
}

void WebServerOTAComponent::dump_config() { ESP_LOGCONFIG(TAG, "Web Server OTA"); }

}  // namespace esphome::web_server

#endif  // USE_WEBSERVER_OTA
