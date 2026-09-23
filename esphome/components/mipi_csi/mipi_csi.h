#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <atomic>
#include <memory>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_cam_sensor_types.h>

#include "esphome/components/camera/camera.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "jpeg_encoder.h"

namespace esphome::mipi_csi {

/// Raw capture format requested from the video device.
///
/// Only formats the hardware JPEG encoder can consume are offered, because every captured frame is
/// encoded to JPEG before it leaves this component.
enum class PixelFormat : uint8_t {
  PIXEL_FORMAT_RGB565,
  PIXEL_FORMAT_RGB888,
  PIXEL_FORMAT_YUV422,
  PIXEL_FORMAT_GRAYSCALE,
};

/// Payload handed to the `on_image` automation.
struct CameraImageData {
  uint8_t *data;
  size_t length;
};

/// A single JPEG encoded frame.
///
/// The data is owned by the camera's encoder, so an image stays valid only until the camera reuses
/// the encoder's output buffer. The camera waits for every shared_ptr to be released before it does.
class MipiCsiImage : public camera::CameraImage {
 public:
  MipiCsiImage(uint8_t *data, size_t length, uint8_t requesters)
      : data_(data), length_(length), requesters_(requesters) {}

  uint8_t *get_data_buffer() override { return this->data_; }
  size_t get_data_length() override { return this->length_; }
  bool was_requested_by(camera::CameraRequester requester) const override {
    return (this->requesters_ & (1U << requester)) != 0;
  }

 protected:
  uint8_t *data_;
  size_t length_;
  uint8_t requesters_;
};

/// Tracks how far a consumer has read into an image.
class MipiCsiImageReader : public camera::CameraImageReader {
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

/// Camera for MIPI-CSI sensors on the ESP32-P4.
///
/// Frames are captured through the V4L2 interface of Espressif's `esp_video` component and encoded
/// to JPEG by the chip's JPEG peripheral. Sensors with a RAW Bayer output are routed through the
/// ISP, where `esp_ipa` runs auto exposure and auto white balance. `esp_video` wires that up on its
/// own, so there is nothing to configure here beyond enabling it at build time.
class MipiCsiCamera final : public camera::Camera {
 public:
  void set_sensor_name(const char *sensor_name) { this->sensor_name_ = sensor_name; }
  void set_resolution(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  void set_pixel_format(PixelFormat format) { this->pixel_format_ = format; }
  void set_jpeg_quality(uint8_t quality) { this->jpeg_quality_ = quality; }
  void set_horizontal_flip(bool flip) { this->horizontal_flip_ = flip; }
  void set_vertical_flip(bool flip) { this->vertical_flip_ = flip; }
  void set_framerate(uint8_t framerate) { this->framerate_ = framerate; }
  void set_idle_update_interval(uint32_t interval) { this->idle_update_interval_ = interval; }
  void set_frame_buffer_count(uint8_t count) { this->frame_buffer_count_ = count; }
  void set_i2c_bus(i2c::InternalI2CBus *bus) { this->i2c_bus_ = bus; }
  void set_sccb_frequency(uint32_t frequency) { this->sccb_frequency_ = frequency; }
  void set_init_ldo(bool init_ldo) { this->init_ldo_ = init_ldo; }
  void set_reset_pin(int8_t pin) { this->reset_pin_ = pin; }
  void set_power_pin(int8_t pin) { this->power_pin_ = pin; }
  void set_external_clock(int8_t pin, uint32_t frequency) {
    this->xclk_pin_ = pin;
    this->xclk_frequency_ = frequency;
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void add_listener(camera::CameraListener *listener) override { this->listeners_.push_back(listener); }
  camera::CameraImageReader *create_image_reader() override { return new MipiCsiImageReader(); }  // NOLINT
  void request_image(camera::CameraRequester requester) override;
  void start_stream(camera::CameraRequester requester) override;
  void stop_stream(camera::CameraRequester requester) override;

 protected:
  /// One V4L2 capture buffer mapped into our address space.
  struct FrameBuffer {
    uint8_t *data;
    size_t length;
  };

  bool start_external_clock_();
  bool init_video_();
  bool select_sensor_format_();
  bool configure_device_();
  bool start_streaming_();
  void report_throughput_(uint32_t now);
  /// Logs every MIPI-CSI format the sensor offers, used when the configured resolution is rejected.
  void log_sensor_formats_();
  /// Ranks two sensor formats of the same resolution against each other.
  bool is_better_sensor_format_(const esp_cam_sensor_format_t &candidate, const esp_cam_sensor_format_t &current) const;
  /// Reduces the configured frame rate to one the driver can actually produce.
  uint8_t resolve_framerate_(uint8_t sensor_fps) const;
  /// Applies a V4L2 user control, logging a warning if the sensor does not support it.
  void apply_control_(uint32_t id, int32_t value, const char *name);
  /// Reads back the format the driver settled on, which the JPEG encoder has to match exactly.
  bool read_back_format_(uint32_t expected_fourcc, uint8_t bytes_per_pixel, const char *name);
  bool has_requested_image_() const { return this->single_requesters_ != 0 || this->stream_requesters_ != 0; }

  static void capture_task(void *param);

  const char *sensor_name_{""};
  /// Configured capture size, or zero to keep whatever resolution the sensor driver starts with.
  uint16_t width_{0};
  uint16_t height_{0};
  PixelFormat pixel_format_{PixelFormat::PIXEL_FORMAT_RGB565};
  uint8_t jpeg_quality_{40};
  bool horizontal_flip_{false};
  bool vertical_flip_{false};
  uint8_t framerate_{10};
  uint32_t idle_update_interval_{15000};
  uint8_t frame_buffer_count_{2};
  i2c::InternalI2CBus *i2c_bus_{nullptr};
  uint32_t sccb_frequency_{100000};
  bool init_ldo_{true};
  int8_t reset_pin_{-1};
  int8_t power_pin_{-1};
  int8_t xclk_pin_{-1};
  uint32_t xclk_frequency_{0};

  int fd_{-1};
  /// Frame size the driver reports and the encoder is set up for. A frame that arrives at a
  /// different size did not come out of the pipeline we configured.
  size_t expected_frame_size_{0};
  // Rolling counters behind the throughput report, which tells whether the delivered frame rate is
  // anywhere near the configured one.
  uint32_t published_frames_{0};
  size_t published_bytes_{0};
  uint32_t last_stats_{0};
  FixedVector<FrameBuffer> buffers_;
  JpegEncoder encoder_;
  QueueHandle_t result_queue_{nullptr};
  /// Set by the main loop to ask the capture task for one frame, cleared by the task when done.
  std::atomic<bool> frame_wanted_{false};

  std::shared_ptr<MipiCsiImage> current_image_;
  std::vector<camera::CameraListener *> listeners_;
  std::atomic<uint8_t> single_requesters_{0};
  std::atomic<uint8_t> stream_requesters_{0};
  /// Requesters recorded when the frame currently being captured was asked for.
  uint8_t pending_requesters_{0};
  uint32_t update_interval_{100};
  uint32_t last_update_{0};
  uint32_t last_idle_request_{0};
};

class MipiCsiImageTrigger final : public Trigger<CameraImageData>, public camera::CameraListener {
 public:
  explicit MipiCsiImageTrigger(MipiCsiCamera *parent) { parent->add_listener(this); }
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override {
    this->trigger(CameraImageData{image->get_data_buffer(), image->get_data_length()});
  }
};

class MipiCsiStreamStartTrigger final : public Trigger<>, public camera::CameraListener {
 public:
  explicit MipiCsiStreamStartTrigger(MipiCsiCamera *parent) { parent->add_listener(this); }
  void on_stream_start() override { this->trigger(); }
};

class MipiCsiStreamStopTrigger final : public Trigger<>, public camera::CameraListener {
 public:
  explicit MipiCsiStreamStopTrigger(MipiCsiCamera *parent) { parent->add_listener(this); }
  void on_stream_stop() override { this->trigger(); }
};

}  // namespace esphome::mipi_csi

#endif
