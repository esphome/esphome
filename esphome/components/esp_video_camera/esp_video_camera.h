#pragma once

#include "esphome/core/defines.h"

// This component is ESP32-P4 silicon (MIPI-CSI, ISP, hardware JPEG) and builds
// only against esp_video's V4L2 headers, so it compiles on that variant alone.
#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/core/component.h"
#include "esphome/components/camera/camera.h"
#include "esphome/components/i2c/i2c.h"

#include "driver/gpio.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace esphome::esp_video_camera {

/// An owned JPEG frame, copied out of the mapped V4L2 buffer so that buffer can
/// be re-queued immediately while the API streams this copy out.
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

/// Home Assistant camera backed by Espressif's esp_video (V4L2) pipeline. It
/// captures JPEG frames from either the hardware JPEG encoder fed by an
/// auto-detected MIPI-CSI sensor ("jpeg"), a USB-UVC camera ("uvc"), or an
/// explicit /dev/videoN path.
class ESPVideoCamera : public camera::Camera {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Pipeline configuration -----------------------------------------------------
  void set_i2c_bus(i2c::InternalI2CBus *bus) { this->i2c_bus_ = bus; }
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

  // Copy a finished JPEG frame into PSRAM and hand it to the listeners.
  void deliver_frame_(const uint8_t *data, size_t length);
  // Tear the capture down and arm a retry when `err` means the device is gone
  // (a USB-UVC camera unplugged mid-stream). Returns true when it handled `err`.
  bool handle_device_gone_(int err);
  bool configure_capture_format_(uint32_t pixelformat);
  bool setup_capture_buffers_();
  // Hardware-JPEG path: capture RGB565 (sensor/ISP) -> JPEG M2M encoder.
  bool start_jpeg_pipeline_();
  void loop_jpeg_pipeline_();
  // STREAMOFF/STREAMON both encoder queues to release buffers stuck in the
  // driver after a failed QBUF/DQBUF. Returns false if the encoder is dead.
  bool reset_jpeg_encoder_();
  // Hand the encoder the single MMAP buffer it writes encoded frames into.
  bool queue_jpeg_capture_buffer_();
  // Direct path: a source that already delivers JPEG/MJPEG (USB-UVC / device).
  bool start_direct_capture_();
  void loop_direct_capture_();

  // Pipeline
  i2c::InternalI2CBus *i2c_bus_{nullptr};
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
  int jpeg_quality_{80};  // V4L2 semantics: 1..100, higher is better
  float max_framerate_{10.0f};
  uint32_t min_interval_ms_{100};
  uint32_t last_frame_ms_{0};

  // Re-open delay after the capture device disappeared mid-stream.
  static constexpr uint32_t CAPTURE_RETRY_INTERVAL_MS = 2000;
  bool capture_retry_pending_{false};
  uint32_t capture_retry_at_ms_{0};
  // When the last consumer went away, or 0 while at least one is present. The
  // pipeline is only torn down once this is CAPTURE_IDLE_TIMEOUT_MS old.
  uint32_t idle_since_ms_{0};
  // Throughput accumulated between two STATS_INTERVAL_MS reports.
  uint32_t stats_since_ms_{0};
  uint32_t stats_frames_{0};
  uint32_t stats_bytes_{0};
  // One-shot: dump the rejected buffer's placement on the first encoder QBUF
  // failure of a capture, not on every frame.
  bool logged_qbuf_failure_{false};

  // Consumers (bit masks indexed by camera::CameraRequester). Written from the
  // requesting task, read by loop() in the main task, hence atomic. Everything
  // below belongs to loop().
  std::vector<camera::CameraListener *> listeners_;
  std::shared_ptr<ESPVideoCameraImage> current_image_;
  std::atomic<uint8_t> stream_requesters_{0};
  std::atomic<uint8_t> single_requesters_{0};

  // V4L2 state. A direct source (USB-UVC, or a /dev/videoN already producing
  // JPEG) uses capture_fd_ + capture_buffers_ only. The hardware-JPEG source
  // spans two devices: capture_fd_ is the MIPI-CSI/ISP device producing RGB565,
  // jpeg_fd_ the M2M encoder fed from it and read back as JPEG.
  int capture_fd_{-1};
  int jpeg_fd_{-1};
  bool streaming_{false};
  uint32_t capture_width_{0};
  uint32_t capture_height_{0};
  static constexpr int MAX_BUFFERS = 3;
  struct MappedBuffer {
    void *start{nullptr};
    size_t length{0};
  };
  MappedBuffer capture_buffers_[MAX_BUFFERS];
  int num_capture_buffers_{0};
  MappedBuffer jpeg_out_buffer_;
};

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4
