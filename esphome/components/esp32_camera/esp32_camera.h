#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace esp32_camera {

class ESP32Camera;

class CameraImage {
 public:
  CameraImage(camera_fb_t *buffer);
  camera_fb_t *get_raw_buffer();
  uint8_t *get_data_buffer();
  size_t get_data_length();

 protected:
  camera_fb_t *buffer_;
};

class CameraImageReader {
 public:
  void set_image(std::shared_ptr<CameraImage> image);
  size_t available() const;
  uint8_t *peek_data_buffer();
  void consume_data(size_t consumed);
  void return_image();

 protected:
  std::shared_ptr<CameraImage> image_;
  size_t offset_{0};
};

enum ESP32CameraFrameSize {
  ESP32_CAMERA_SIZE_160X120,    // QQVGA
  ESP32_CAMERA_SIZE_176X144,    // QCIF
  ESP32_CAMERA_SIZE_240X176,    // HQVGA
  ESP32_CAMERA_SIZE_320X240,    // QVGA
  ESP32_CAMERA_SIZE_400X296,    // CIF
  ESP32_CAMERA_SIZE_640X480,    // VGA
  ESP32_CAMERA_SIZE_800X600,    // SVGA
  ESP32_CAMERA_SIZE_1024X768,   // XGA
  ESP32_CAMERA_SIZE_1280X1024,  // SXGA
  ESP32_CAMERA_SIZE_1600X1200,  // UXGA
};

class ESP32Camera : public Component, public EntityBase {
 public:
  ESP32Camera(const std::string &name);
  ESP32Camera();
  void set_data_pins(std::array<uint8_t, 8> pins);
  void set_vsync_pin(uint8_t pin);
  void set_href_pin(uint8_t pin);
  void set_pixel_clock_pin(uint8_t pin);
  void set_external_clock(uint8_t pin, uint32_t frequency);
  void set_i2c_pins(uint8_t sda, uint8_t scl);
  void set_frame_size(ESP32CameraFrameSize size);
  void set_jpeg_quality(uint8_t quality);
  void set_reset_pin(uint8_t pin);
  void set_power_down_pin(uint8_t pin);
  void set_vertical_flip(bool vertical_flip);
  void set_horizontal_mirror(bool horizontal_mirror);
  void set_contrast(int contrast);
  void set_brightness(int brightness);
  void set_saturation(int saturation);
  void set_max_update_interval(uint32_t max_update_interval);
  void set_idle_update_interval(uint32_t idle_update_interval);
  void set_test_pattern(bool test_pattern);
  void setup() override;
  void loop() override;
  void dump_config() override;
  void add_image_callback(std::function<void(std::shared_ptr<CameraImage>)> &&f);
  float get_setup_priority() const override;
  void request_stream();
  void request_image();

 protected:
  uint32_t hash_base() override;
  bool has_requested_image_() const;
  bool can_return_image_() const;

  static void framebuffer_task(void *pv);

  camera_config_t config_{};
  bool vertical_flip_{true};
  bool horizontal_mirror_{true};
  int contrast_{0};
  int brightness_{0};
  int saturation_{0};
  bool test_pattern_{false};

  esp_err_t init_error_{ESP_OK};
  std::shared_ptr<CameraImage> current_image_;
  uint32_t last_stream_request_{0};
  bool single_requester_{false};
  QueueHandle_t framebuffer_get_queue_;
  QueueHandle_t framebuffer_return_queue_;
  CallbackManager<void(std::shared_ptr<CameraImage>)> new_image_callback_;
  uint32_t max_update_interval_{1000};
  uint32_t idle_update_interval_{15000};
  uint32_t last_update_{0};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32Camera *global_esp32_camera;

}  // namespace esp32_camera
}  // namespace esphome

#endif
