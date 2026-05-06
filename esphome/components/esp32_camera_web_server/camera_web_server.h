#pragma once

#ifdef USE_ESP32

#include <cinttypes>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esphome/components/camera/camera.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

struct httpd_req;  // NOLINT(readability-identifier-naming)

namespace esphome {
namespace esp32_camera_web_server {

enum Mode { STREAM, SNAPSHOT };

class CameraWebServer : public Component, public camera::CameraListener {
 public:
  CameraWebServer();
  ~CameraWebServer();

  void setup() override;
  void on_shutdown() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_port(uint16_t port) { this->port_ = port; }
  void set_mode(Mode mode) { this->mode_ = mode; }
  void loop() override;

  /// CameraListener interface
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;

 protected:
  std::shared_ptr<camera::CameraImage> wait_for_image_();
  esp_err_t handler_(struct httpd_req *req);
  esp_err_t streaming_handler_(struct httpd_req *req);
  esp_err_t snapshot_handler_(struct httpd_req *req);

  uint16_t port_{0};
  void *httpd_{nullptr};
  SemaphoreHandle_t semaphore_;
  std::shared_ptr<camera::CameraImage> image_;
  bool running_{false};
  Mode mode_{STREAM};
};

}  // namespace esp32_camera_web_server
}  // namespace esphome

#endif  // USE_ESP32
