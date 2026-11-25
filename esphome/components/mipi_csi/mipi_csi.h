#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <atomic>
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_cam_sensor_types.h>

#include "esphome/components/camera/camera.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "jpeg_encoder.h"

namespace esphome::mipi_csi {

/// Payload handed to the `on_image` automation.
struct CameraImageData {
  uint8_t *data;
  size_t length;
};

/// A JPEG frame in the encoder's buffer; the camera reuses it once every shared_ptr is released.
class MipiCsiImage : public camera::CameraImage {
 public:
  void set_frame(uint8_t *data, size_t length, uint8_t requesters) {
    this->data_ = data;
    this->length_ = length;
    this->requesters_ = requesters;
  }

  uint8_t *get_data_buffer() override { return this->data_; }
  size_t get_data_length() override { return this->length_; }
  bool was_requested_by(camera::CameraRequester requester) const override {
    return (this->requesters_ & (1U << requester)) != 0;
  }

 protected:
  uint8_t *data_{nullptr};
  size_t length_{0};
  uint8_t requesters_{0};
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

/// MIPI-CSI camera on the ESP32-P4: V4L2 capture through esp_video, JPEG by the hardware encoder.
class MipiCsiCamera final : public camera::Camera {
 public:
  /// @param fourcc V4L2 pixel format captured from the video device, which the encoder takes as
  /// @p jpeg_input with @p bytes_per_pixel.
  MipiCsiCamera(const char *sensor_name, i2c::InternalI2CBus *i2c_bus, uint32_t sccb_frequency, uint32_t fourcc,
                uint8_t bytes_per_pixel, jpeg_enc_input_format_t jpeg_input, jpeg_down_sampling_type_t sub_sample,
                uint8_t jpeg_quality, uint8_t frame_buffer_count, uint8_t framerate, uint32_t idle_update_interval,
                esp_cam_sensor_output_format_t sensor_format)
      : sensor_name_(sensor_name),
        i2c_bus_(i2c_bus),
        sccb_frequency_(sccb_frequency),
        fourcc_(fourcc),
        idle_update_interval_(idle_update_interval),
        encoder_(jpeg_input, sub_sample, jpeg_quality),
        bytes_per_pixel_(bytes_per_pixel),
        frame_buffer_count_(frame_buffer_count),
        framerate_(framerate),
        sensor_format_(sensor_format) {}

  void set_resolution(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  /// Bits as in FLIP_*: whether each flip is configured, and its value.
  void set_flip_flags(uint8_t flags) { this->flip_flags_ = flags; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_power_pin(GPIOPin *pin) { this->power_pin_ = pin; }
  void set_external_clock(int8_t pin, uint32_t frequency) {
    this->xclk_pin_ = pin;
    this->xclk_frequency_ = frequency;
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Called at setup by the consumers; a pointer array grown by one, without std::vector's code
  void add_listener(camera::CameraListener *listener) override {
    uint16_t capacity;
    this->listeners_ = static_cast<camera::CameraListener **>(
        callback_manager_grow(this->listeners_, this->listener_count_, capacity, sizeof(camera::CameraListener *)));
    this->listeners_[this->listener_count_++] = listener;
  }
#ifdef USE_MIPI_CSI_IMAGE_CALLBACK
  /// on_image: the frame is valid only during the call
  template<typename F> void add_on_image_callback(F &&callback) {
    this->image_callback_.add(std::forward<F>(callback));
  }
#endif
#ifdef USE_MIPI_CSI_STREAM_CALLBACK
  /// on_stream_start (true) / on_stream_stop (false)
  template<typename F> void add_on_stream_callback(F &&callback) {
    this->stream_callback_.add(std::forward<F>(callback));
  }
#endif
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

  static constexpr uint8_t FLIP_HORIZONTAL_SET = 1 << 0;
  static constexpr uint8_t FLIP_HORIZONTAL_ON = 1 << 1;
  static constexpr uint8_t FLIP_VERTICAL_SET = 1 << 2;
  static constexpr uint8_t FLIP_VERTICAL_ON = 1 << 3;

  /// Releases everything setup() acquired and marks the component failed.
  void fail_(const LogString *reason);
  bool start_external_clock_();
  bool init_video_();
  bool select_sensor_format_();
  /// Returns the size of one raw frame, or 0 on failure.
  size_t configure_device_();
  bool start_streaming_(size_t frame_size);
  /// Calls @p fn with every MIPI-CSI format the sensor offers.
  template<typename F> void for_each_sensor_format_(F &&fn);
  /// Ranks two sensor formats of the same resolution against each other.
  bool is_better_sensor_format_(const esp_cam_sensor_format_t &candidate, const esp_cam_sensor_format_t &current) const;
  /// Reduces the configured frame rate to one the driver can actually produce.
  uint8_t resolve_framerate_(uint8_t sensor_fps) const;
  uint8_t reachable_framerate_(uint8_t sensor_fps) const;
  /// Applies a V4L2 user control, logging a warning if the sensor does not support it.
  void apply_control_(uint32_t id, int32_t value, const LogString *name);
  /// Reads back the format the driver settled on, which the JPEG encoder has to match exactly.
  bool read_back_format_();
  bool has_requested_image_() const { return this->single_requesters_ != 0 || this->stream_requesters_ != 0; }

  static void capture_task(void *param);

  const char *sensor_name_;
  i2c::InternalI2CBus *i2c_bus_;
  uint32_t sccb_frequency_;
  uint32_t fourcc_;
  uint32_t idle_update_interval_;
  uint32_t xclk_frequency_{0};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *power_pin_{nullptr};
  int fd_{-1};
  FixedVector<FrameBuffer> buffers_;
  JpegEncoder encoder_;
  QueueHandle_t result_queue_{nullptr};
  std::shared_ptr<MipiCsiImage> reusable_image_;
  std::shared_ptr<MipiCsiImage> current_image_;
  camera::CameraListener **listeners_{nullptr};
  // Compiled only when the automation is configured, so never empty
#ifdef USE_MIPI_CSI_IMAGE_CALLBACK
  CallbackManager<void(CameraImageData)> image_callback_;
#endif
#ifdef USE_MIPI_CSI_STREAM_CALLBACK
  CallbackManager<void(bool)> stream_callback_;
#endif
  uint32_t last_request_{0};
  uint32_t last_idle_request_{0};
  uint16_t width_{0};
  uint16_t height_{0};
  uint8_t bytes_per_pixel_;
  uint8_t frame_buffer_count_;
  uint8_t framerate_;
  uint8_t flip_flags_{0};
  /// The sensor output that needs no conversion for the configured pixel format
  uint8_t sensor_format_;
  uint8_t listener_count_{0};
  int8_t xclk_pin_{-1};
  /// Requesters recorded when the frame currently being captured was asked for.
  uint8_t pending_requesters_{0};
  std::atomic<uint8_t> single_requesters_{0};
  std::atomic<uint8_t> stream_requesters_{0};
  /// Set by the main loop to ask the capture task for one frame, cleared by the task when done.
  std::atomic<bool> frame_wanted_{false};
};

}  // namespace esphome::mipi_csi

#endif
