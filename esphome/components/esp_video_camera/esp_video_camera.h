#pragma once

#include "esphome/core/component.h"
#include "esphome/components/camera/camera.h"
#include "esphome/components/i2c/i2c.h"

#include "driver/gpio.h"

#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace esp_video_camera {

/// An owned JPEG/MJPEG frame (copied into PSRAM) shared with the API.
///
/// The data is JPEG-encoded (required by the Home Assistant camera API). It is
/// copied out of the mapped V4L2 buffer so that buffer can be re-queued
/// immediately, while the API streams this copy out over the network.
class ESPVideoCameraImage : public camera::CameraImage {
 public:
  ESPVideoCameraImage(uint8_t *data, size_t length, uint8_t requesters);
  ~ESPVideoCameraImage() override;

  uint8_t *get_data_buffer() override { return this->data_; }
  size_t get_data_length() override { return this->length_; }
  bool was_requested_by(camera::CameraRequester requester) const override;

 protected:
  uint8_t *data_{nullptr};
  size_t length_{0};
  uint8_t requesters_{0};
};

/// Reader used by the API to stream the JPEG bytes out in chunks.
class ESPVideoCameraImageReader : public camera::CameraImageReader {
 public:
  void set_image(std::shared_ptr<camera::CameraImage> image) override;
  size_t available() const override;
  uint8_t *peek_data_buffer() override;
  void consume_data(size_t consumed) override;
  void return_image() override;

 protected:
  std::shared_ptr<camera::CameraImage> image_;
  size_t offset_{0};
};

/// Home Assistant camera backed by Espressif's esp_video (V4L2) pipeline.
///
/// This single component both initialises the camera pipeline (MIPI-CSI, with an
/// optional USB-UVC host) and publishes the stream as a native `camera` entity.
/// It captures JPEG/MJPEG frames from a V4L2 device:
///   - "jpeg": the hardware JPEG encoder (/dev/video10) — works with every
///     auto-detected MIPI-CSI sensor (SC202CS, OV5647, OV02C10, SC2336, ...).
///   - "uvc":  a USB-UVC camera (/dev/video40+) that streams MJPEG.
///   - "/dev/videoN": an explicit V4L2 path.
class ESPVideoCamera : public camera::Camera {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Pipeline configuration -----------------------------------------------------
  void set_i2c_bus(i2c::I2CBus *bus) { this->i2c_bus_ = bus; }
  void set_xclk_pin(gpio_num_t pin) { this->xclk_pin_ = pin; }
  void set_xclk_freq(uint32_t freq) { this->xclk_freq_ = freq; }
  void set_enable_xclk_init(bool enable) { this->enable_xclk_init_ = enable; }
  void set_enable_uvc(bool enable) { this->enable_uvc_ = enable; }

  // Camera platform configuration ----------------------------------------------
  void set_device(const std::string &device) { this->device_ = device; }
  void set_resolution(const std::string &resolution) { this->resolution_ = resolution; }
  void set_jpeg_quality(int quality) { this->jpeg_quality_ = quality; }
  void set_max_framerate(float fps) {
    this->max_framerate_ = fps;
    this->min_interval_ms_ = (fps > 0.0f) ? (uint32_t) (1000.0f / fps) : 0;
  }

  // camera::Camera -------------------------------------------------------------
  void add_listener(camera::CameraListener *listener) override { this->listeners_.push_back(listener); }
  camera::CameraImageReader *create_image_reader() override;
  void request_image(camera::CameraRequester requester) override;
  void start_stream(camera::CameraRequester requester) override;
  void stop_stream(camera::CameraRequester requester) override;

 protected:
  bool init_pipeline_();
  bool start_capture_();
  void stop_capture_();
  void update_capture_state_();
  void configure_format_();
  static bool parse_resolution_(const std::string &res, uint32_t &width, uint32_t &height);

  // Pipeline
  i2c::I2CBus *i2c_bus_{nullptr};
  gpio_num_t xclk_pin_{GPIO_NUM_36};
  uint32_t xclk_freq_{24000000};
  bool enable_xclk_init_{false};
  bool enable_uvc_{false};
  bool pipeline_ready_{false};

  // Camera platform
  std::string device_{"jpeg"};
  std::string resolved_device_;
  bool is_hw_jpeg_{false};
  std::string resolution_{"auto"};
  int jpeg_quality_{10};
  float max_framerate_{10.0f};
  uint32_t min_interval_ms_{100};
  uint32_t last_frame_ms_{0};

  // Consumers (bit masks indexed by camera::CameraRequester)
  std::vector<camera::CameraListener *> listeners_;
  std::shared_ptr<ESPVideoCameraImage> current_image_;
  uint8_t stream_requesters_{0};
  uint8_t single_requesters_{0};

  // V4L2 state
  int fd_{-1};
  bool streaming_{false};
  static constexpr int MAX_BUFFERS = 3;
  struct MappedBuffer {
    void *start{nullptr};
    size_t length{0};
  };
  MappedBuffer buffers_[MAX_BUFFERS];
  int num_buffers_{0};
};

}  // namespace esp_video_camera
}  // namespace esphome
