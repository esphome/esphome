#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WEBSERVER_OTA

#include "esphome/components/ota/ota_backend_factory.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome::web_server {

class WebServerOTAComponent final : public AsyncWebHandler, public ota::OTAComponent {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index, uint8_t *data,
                    size_t len, bool final) override;
  bool canHandle(AsyncWebServerRequest *request) const override;

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
  bool ota_success_{false};

 private:
  ota::OTABackendPtr ota_backend_{nullptr};
  /// Request that owns ota_backend_.
  const AsyncWebServerRequest *ota_request_{nullptr};
  const AsyncWebServerRequest *refused_request_{nullptr};
};

}  // namespace esphome::web_server

#endif  // USE_WEBSERVER_OTA
