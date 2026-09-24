#include "mipi_csi.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <driver/i2c_master.h>
#include <esp_cam_sensor_xclk.h>
#include <esp_video_device.h>
#include <esp_video_init.h>
#include <esp_video_ioctl.h>
#include <linux/videodev2.h>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mipi_csi {

static const char *const TAG = "mipi_csi";

static constexpr uint32_t CAPTURE_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t CAPTURE_TASK_PRIORITY = 1;
static constexpr BaseType_t CAPTURE_TASK_CORE = 1;
/// How often the delivered frame rate is summarised at debug level.
static constexpr uint32_t THROUGHPUT_REPORT_INTERVAL_MS = 5000;
/// How long the capture task waits before retrying after a dequeue error.
static constexpr uint32_t CAPTURE_RETRY_DELAY_MS = 100;

/// How a configured pixel format maps onto the video device and the JPEG encoder.
struct FormatMapping {
  uint32_t fourcc;
  jpeg_enc_input_format_t jpeg_input;
  jpeg_down_sampling_type_t sub_sample;
  uint8_t bytes_per_pixel;
  const char *name;
};

static FormatMapping get_format_mapping(PixelFormat format) {
  switch (format) {
    case PixelFormat::PIXEL_FORMAT_RGB888:
      return {V4L2_PIX_FMT_RGB24, JPEG_ENCODE_IN_FORMAT_RGB888, JPEG_DOWN_SAMPLING_YUV444, 3, "RGB888"};
    case PixelFormat::PIXEL_FORMAT_YUV422:
      return {V4L2_PIX_FMT_UYVY, JPEG_ENCODE_IN_FORMAT_YUV422, JPEG_DOWN_SAMPLING_YUV422, 2, "YUV422"};
    case PixelFormat::PIXEL_FORMAT_GRAYSCALE:
      return {V4L2_PIX_FMT_GREY, JPEG_ENCODE_IN_FORMAT_GRAY, JPEG_DOWN_SAMPLING_GRAY, 1, "GRAYSCALE"};
    case PixelFormat::PIXEL_FORMAT_RGB565:
    default:
      return {V4L2_PIX_FMT_RGB565, JPEG_ENCODE_IN_FORMAT_RGB565, JPEG_DOWN_SAMPLING_YUV422, 2, "RGB565"};
  }
}

/// Holds the four characters of a fourcc code plus a terminator, for logging.
using FourccName = char[5];

static void format_fourcc(uint32_t fourcc, FourccName out) {
  out[0] = static_cast<char>(fourcc & 0xFF);
  out[1] = static_cast<char>((fourcc >> 8) & 0xFF);
  out[2] = static_cast<char>((fourcc >> 16) & 0xFF);
  out[3] = static_cast<char>((fourcc >> 24) & 0xFF);
  out[4] = '\0';
}

/* ---------------- MipiCsiCamera: setup ---------------- */

void MipiCsiCamera::setup() {
  if (!this->start_external_clock_() || !this->init_video_() || !this->configure_device_()) {
    this->teardown_();
    this->mark_failed();
    return;
  }

  this->update_interval_ = 1000 / this->framerate_;
  this->result_queue_ = xQueueCreate(1, sizeof(size_t));
  if (this->result_queue_ == nullptr) {
    ESP_LOGE(TAG, "Not enough memory for the frame queue");
    this->teardown_();
    this->mark_failed();
    return;
  }

  if (xTaskCreatePinnedToCore(&MipiCsiCamera::capture_task, "mipi_csi", CAPTURE_TASK_STACK_SIZE, this,
                              CAPTURE_TASK_PRIORITY, nullptr, CAPTURE_TASK_CORE) != pdPASS) {
    ESP_LOGE(TAG, "Not enough memory to start the capture task");
    vQueueDelete(this->result_queue_);
    this->result_queue_ = nullptr;
    this->teardown_();
    this->mark_failed();
    return;
  }

  if (!this->start_streaming_()) {
    this->teardown_();
    this->mark_failed();
    return;
  }
}

bool MipiCsiCamera::start_external_clock_() {
  if (this->xclk_pin_ < 0)
    return true;

#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
  esp_cam_sensor_xclk_handle_t handle = nullptr;
  esp_err_t err = esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Allocating the sensor clock failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_cam_sensor_xclk_config_t config{};
  config.esp_clock_router_cfg.xclk_pin = static_cast<gpio_num_t>(this->xclk_pin_);
  config.esp_clock_router_cfg.xclk_freq_hz = this->xclk_frequency_;
  err = esp_cam_sensor_xclk_start(handle, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Starting the sensor clock failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
#else
  ESP_LOGE(TAG, "Sensor clock output is not available in this build");
  return false;
#endif
}

bool MipiCsiCamera::init_video_() {
  i2c_master_bus_handle_t i2c_handle = nullptr;
  esp_err_t err = i2c_master_get_bus_handle(this->i2c_bus_->get_port(), &i2c_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Could not use I2C port %d for the sensor: %s", this->i2c_bus_->get_port(), esp_err_to_name(err));
    return false;
  }

  esp_video_init_csi_config_t csi_config{};
  csi_config.sccb_config.init_sccb = false;
  csi_config.sccb_config.i2c_handle = i2c_handle;
  csi_config.sccb_config.freq = this->sccb_frequency_;
  csi_config.reset_pin = static_cast<gpio_num_t>(this->reset_pin_);
  csi_config.pwdn_pin = static_cast<gpio_num_t>(this->power_pin_);
  // When another peripheral already powers the D-PHY rail, claiming it a second time fails and
  // takes the whole camera down with it, so in that case it is left alone.
  csi_config.dont_init_ldo = !this->init_ldo_;

  esp_video_init_config_t video_config{};
  video_config.csi = &csi_config;

  // The ISP is always initialized as well: esp_video routes RAW sensors through it and starts the
  // IPA pipeline itself, and simply bypasses it for sensors that already output RGB or YUV.
  err = esp_video_init_with_flags(&video_config, ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera sensor initialization failed: %s", esp_err_to_name(err));
    if (this->init_ldo_) {
      ESP_LOGE(TAG, "If another peripheral already powers the camera's supply rail, set 'init_ldo: false'");
    }
    return false;
  }
  return true;
}

bool MipiCsiCamera::select_sensor_format_() {
  esp_cam_sensor_format_t chosen{};

  if (this->width_ == 0) {
    // No resolution configured, so keep the format the sensor driver starts up with.
    if (ioctl(this->fd_, VIDIOC_G_SENSOR_FMT, &chosen) != 0) {
      ESP_LOGE(TAG, "Reading the sensor's current format failed: %s", strerror(errno));
      return false;
    }
  } else {
    bool found = false;
    v4l2_sensor_format_enum enumerator{};
    for (enumerator.index = 0; ioctl(this->fd_, VIDIOC_ENUM_SENSOR_FMT, &enumerator) == 0; enumerator.index++) {
      const esp_cam_sensor_format_t &candidate = enumerator.format;
      if (candidate.port != ESP_CAM_SENSOR_MIPI_CSI || candidate.width != this->width_ ||
          candidate.height != this->height_) {
        continue;
      }
      if (!found || this->is_better_sensor_format_(candidate, chosen)) {
        chosen = candidate;
        found = true;
      }
    }

    if (!found) {
      ESP_LOGE(TAG, "The sensor cannot capture %ux%u. Supported resolutions are:", this->width_, this->height_);
      this->log_sensor_formats_();
      return false;
    }

    // The resolution belongs to the sensor, so it has to be selected here. VIDIOC_S_FMT only picks
    // the pixel format and rejects every resolution other than the sensor's current one.
    if (ioctl(this->fd_, VIDIOC_S_SENSOR_FMT, &chosen) != 0) {
      ESP_LOGE(TAG, "Switching the sensor to %ux%u failed: %s", this->width_, this->height_, strerror(errno));
      return false;
    }
  }

  this->width_ = chosen.width;
  this->height_ = chosen.height;
  this->framerate_ = this->resolve_framerate_(chosen.fps);
  ESP_LOGD(TAG, "Sensor format '%s': %ux%u at %u fps, %s",
           chosen.name != nullptr ? chosen.name : LOG_STR_LITERAL("unnamed"), chosen.width, chosen.height, chosen.fps,
           chosen.isp_info != nullptr ? LOG_STR_LITERAL("processed by the ISP")
                                      : LOG_STR_LITERAL("used as the sensor delivers it"));
  return true;
}

void MipiCsiCamera::log_sensor_formats_() {
  v4l2_sensor_format_enum enumerator{};
  for (enumerator.index = 0; ioctl(this->fd_, VIDIOC_ENUM_SENSOR_FMT, &enumerator) == 0; enumerator.index++) {
    const esp_cam_sensor_format_t &format = enumerator.format;
    if (format.port != ESP_CAM_SENSOR_MIPI_CSI)
      continue;
    ESP_LOGE(TAG, "  %ux%u at %u fps (%s)", format.width, format.height, format.fps,
             format.name != nullptr ? format.name : LOG_STR_LITERAL("unnamed"));
  }
}

bool MipiCsiCamera::is_better_sensor_format_(const esp_cam_sensor_format_t &candidate,
                                             const esp_cam_sensor_format_t &current) const {
  // Formats that carry ISP information are the RAW ones, and only those let the ISP run the image
  // tuning algorithms, so they are always preferred.
  if ((candidate.isp_info != nullptr) != (current.isp_info != nullptr))
    return candidate.isp_info != nullptr;

  // Prefer a sensor rate the configured rate divides into, because that is the only way the driver
  // can reach the configured rate exactly.
  const bool candidate_fits = candidate.fps != 0 && candidate.fps % this->framerate_ == 0;
  const bool current_fits = current.fps != 0 && current.fps % this->framerate_ == 0;
  if (candidate_fits != current_fits)
    return candidate_fits;

  return false;
}

uint8_t MipiCsiCamera::resolve_framerate_(uint8_t sensor_fps) const {
  if (sensor_fps == 0)
    return this->framerate_;

  // The driver slows the stream down by dropping whole frames, so it can only divide the sensor's
  // rate. Step down to the closest rate that divides it evenly.
  for (uint8_t fps = std::min<uint8_t>(this->framerate_, sensor_fps); fps >= 1; fps--) {
    if (sensor_fps % fps == 0) {
      if (fps != this->framerate_) {
        ESP_LOGW(TAG, "The sensor runs at %u fps, so %u fps is used instead of %u fps", sensor_fps, fps,
                 this->framerate_);
      }
      return fps;
    }
  }
  return sensor_fps;
}

bool MipiCsiCamera::configure_device_() {
  this->fd_ = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "Opening %s failed: %s", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, strerror(errno));
    return false;
  }

  if (!this->select_sensor_format_())
    return false;

  const FormatMapping mapping = get_format_mapping(this->pixel_format_);

  v4l2_format format{};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = this->width_;
  format.fmt.pix.height = this->height_;
  format.fmt.pix.pixelformat = mapping.fourcc;
  if (ioctl(this->fd_, VIDIOC_S_FMT, &format) != 0) {
    ESP_LOGE(TAG, "The camera cannot deliver %s at %ux%u: %s", mapping.name, this->width_, this->height_,
             strerror(errno));
    return false;
  }

  // Must come after the format has been set, and not every sensor offers a choice of frame rates.
  v4l2_streamparm stream_param{};
  stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  stream_param.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
  stream_param.parm.capture.timeperframe.numerator = 1;
  stream_param.parm.capture.timeperframe.denominator = this->framerate_;
  if (ioctl(this->fd_, VIDIOC_S_PARM, &stream_param) != 0) {
    ESP_LOGW(TAG, "The sensor kept its own frame rate: %s", strerror(errno));
  }

  this->apply_control_(V4L2_CID_HFLIP, this->horizontal_flip_, "horizontal flip");
  this->apply_control_(V4L2_CID_VFLIP, this->vertical_flip_, "vertical flip");

  if (!this->read_back_format_(mapping.fourcc, mapping.bytes_per_pixel, mapping.name))
    return false;

  return this->encoder_.init(this->width_, this->height_, mapping.jpeg_input, mapping.sub_sample, this->jpeg_quality_,
                             this->expected_frame_size_);
}

bool MipiCsiCamera::read_back_format_(uint32_t expected_fourcc, uint8_t bytes_per_pixel, const char *name) {
  v4l2_format actual{};
  actual.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->fd_, VIDIOC_G_FMT, &actual) != 0) {
    ESP_LOGE(TAG, "Reading back the capture format failed: %s", strerror(errno));
    return false;
  }

  // The driver is free to settle on something other than what was asked for, and the encoder has to
  // be set up for what actually comes out, not for what was requested. Only the geometry and the
  // pixel format are meaningful here: esp_video hands back the very struct it was given, so its
  // bytesperline and sizeimage fields stay at whatever the caller left them.
  FourccName fourcc_name;
  format_fourcc(actual.fmt.pix.pixelformat, fourcc_name);
  ESP_LOGD(TAG, "Capture format: %" PRIu32 "x%" PRIu32 " '%s'", actual.fmt.pix.width, actual.fmt.pix.height,
           fourcc_name);

  // A different pixel format means every byte of the frame would be read with the wrong layout, so
  // the encoder would turn out a corrupted picture rather than a wrong-looking one.
  if (actual.fmt.pix.pixelformat != expected_fourcc) {
    FourccName expected_name;
    format_fourcc(expected_fourcc, expected_name);
    ESP_LOGE(TAG, "The camera delivers '%s' instead of the configured %s ('%s'); pick another 'pixel_format'",
             fourcc_name, name, expected_name);
    return false;
  }

  this->width_ = actual.fmt.pix.width;
  this->height_ = actual.fmt.pix.height;
  this->expected_frame_size_ = static_cast<size_t>(this->width_) * this->height_ * bytes_per_pixel;
  return true;
}

void MipiCsiCamera::apply_control_(uint32_t id, int32_t value, const char *name) {
  v4l2_ext_control control{};
  control.id = id;
  control.value = value;

  v4l2_ext_controls controls{};
  controls.ctrl_class = V4L2_CTRL_CLASS_USER;
  controls.count = 1;
  controls.controls = &control;

  if (ioctl(this->fd_, VIDIOC_S_EXT_CTRLS, &controls) == 0)
    return;

  // Sensors start up unflipped, so failing to switch a flip off changes nothing and is not worth a
  // warning. Failing to switch one on means the picture will not look the way it was asked to.
  if (value != 0) {
    ESP_LOGW(TAG, "The sensor does not support %s", name);
  } else {
    ESP_LOGD(TAG, "The sensor does not support %s, which it is not using anyway", name);
  }
}

bool MipiCsiCamera::start_streaming_() {
  v4l2_requestbuffers request{};
  request.count = this->frame_buffer_count_;
  request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  request.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->fd_, VIDIOC_REQBUFS, &request) != 0) {
    ESP_LOGE(TAG, "Requesting %u frame buffers failed: %s", this->frame_buffer_count_, strerror(errno));
    return false;
  }

  if (!this->buffers_.try_init(request.count)) {
    ESP_LOGE(TAG, "Not enough memory to track %" PRIu32 " frame buffers", request.count);
    return false;
  }

  for (uint32_t i = 0; i < request.count; i++) {
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;
    if (ioctl(this->fd_, VIDIOC_QUERYBUF, &buffer) != 0) {
      ESP_LOGE(TAG, "Querying frame buffer %" PRIu32 " failed: %s", i, strerror(errno));
      return false;
    }

    void *data = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd_, buffer.m.offset);
    if (data == MAP_FAILED) {
      ESP_LOGE(TAG, "Mapping frame buffer %" PRIu32 " failed: %s", i, strerror(errno));
      return false;
    }
    this->buffers_.push_back({static_cast<uint8_t *>(data), buffer.length});

    if (buffer.length < this->expected_frame_size_) {
      ESP_LOGE(TAG, "Frame buffer %" PRIu32 " holds %" PRIu32 " bytes but a frame needs %zu", i, buffer.length,
               this->expected_frame_size_);
      return false;
    }

    if (ioctl(this->fd_, VIDIOC_QBUF, &buffer) != 0) {
      ESP_LOGE(TAG, "Queueing frame buffer %" PRIu32 " failed: %s", i, strerror(errno));
      return false;
    }
  }

  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->fd_, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Starting the video stream failed: %s", strerror(errno));
    return false;
  }
  return true;
}

void MipiCsiCamera::teardown_() {
  if (this->fd_ < 0)
    return;

  // Ignored if the stream was never turned on: VIDIOC_STREAMOFF then just reports an error.
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(this->fd_, VIDIOC_STREAMOFF, &type);

  for (auto &buffer : this->buffers_) {
    munmap(buffer.data, buffer.length);
  }
  this->buffers_.release();

  close(this->fd_);
  this->fd_ = -1;
}

void MipiCsiCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MIPI-CSI camera '%s':\n"
                "  Sensor: %s\n"
                "  Resolution: %ux%u\n"
                "  Capture format: %s\n"
                "  JPEG quality: %u\n"
                "  Frame rate: %u fps\n"
                "  Frame buffers: %u\n"
                "  Flip: horizontal %s, vertical %s",
                this->get_name().c_str(), this->sensor_name_, this->width_, this->height_,
                get_format_mapping(this->pixel_format_).name, this->jpeg_quality_, this->framerate_,
                this->frame_buffer_count_, YESNO(this->horizontal_flip_), YESNO(this->vertical_flip_));
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed");
  }
}

/* ---------------- MipiCsiCamera: capture ---------------- */

void MipiCsiCamera::capture_task(void *param) {
  auto *self = static_cast<MipiCsiCamera *>(param);

  // The stream runs continuously so that the ISP's auto exposure and white balance stay converged.
  // Frames nobody asked for are simply handed straight back to the driver.
  while (true) {
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(self->fd_, VIDIOC_DQBUF, &buffer) != 0) {
      ESP_LOGW(TAG, "Dequeueing a frame failed: %s", strerror(errno));
      vTaskDelay(pdMS_TO_TICKS(CAPTURE_RETRY_DELAY_MS));
      continue;
    }

    if (self->frame_wanted_.load() && (buffer.flags & V4L2_BUF_FLAG_ERROR) == 0) {
      // The encoder is handed the whole mapped buffer, as Espressif's reference does: the length is
      // what bounds the cache invalidation before the JPEG engine's DMA reads it, so anything short
      // of the region the camera wrote leaves stale data in the picture.
      //
      // buffer.bytesused is not worth checking here. The CSI driver sets it to the configured frame
      // size rather than to the number of bytes it actually received, so it can never reveal a
      // short frame.
      const FrameBuffer &mapped = self->buffers_[buffer.index];
      size_t length = self->encoder_.encode(mapped.data, mapped.length);
      // Reported even when encoding failed: a length of zero tells the main loop that the request
      // it recorded was not served, so that a still image request is not silently dropped.
      xQueueSend(self->result_queue_, &length, portMAX_DELAY);
      App.wake_loop_threadsafe();
      // Cleared last, so that an unset flag always means the result is already on the queue.
      self->frame_wanted_.store(false);
    }

    if (ioctl(self->fd_, VIDIOC_QBUF, &buffer) != 0) {
      ESP_LOGW(TAG, "Returning a frame buffer failed: %s", strerror(errno));
    }
  }
}

void MipiCsiCamera::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // Release the previous image once every consumer has finished reading it, which frees the
  // encoder's output buffer for the next frame.
  if (this->current_image_ && this->current_image_.use_count() == 1) {
    this->current_image_.reset();
  }

  // Publish a frame the capture task has finished encoding.
  if (!this->current_image_) {
    size_t length;
    if (xQueueReceive(this->result_queue_, &length, 0) == pdTRUE) {
      if (length == 0) {
        // Encoding failed. Hand the one-shot requests back so the clients waiting for a still image
        // are served by the next frame instead of waiting forever. Streams ask again on their own.
        this->single_requesters_ |= static_cast<uint8_t>(this->pending_requesters_ & ~this->stream_requesters_.load());
        this->pending_requesters_ = 0;
        this->last_update_ = now;
        return;
      }
      this->current_image_ =
          std::make_shared<MipiCsiImage>(this->encoder_.get_output_buffer(), length, this->pending_requesters_);
      this->pending_requesters_ = 0;
      this->last_update_ = now;
      this->published_frames_++;
      this->published_bytes_ += length;
      for (auto *listener : this->listeners_) {
        listener->on_camera_image(this->current_image_);
      }
      return;
    }
  }

  this->report_throughput_(now);

  // Ask Home Assistant's still image to refresh every so often even when nobody is watching.
  if (!this->has_requested_image_() && this->idle_update_interval_ != 0 &&
      now - this->last_idle_request_ >= this->idle_update_interval_) {
    this->last_idle_request_ = now;
    this->request_image(camera::IDLE);
  }

  // Ask the capture task for the next frame. Not while an image is still in flight, because both
  // share the encoder's single output buffer.
  if (this->current_image_ || this->frame_wanted_.load() || !this->has_requested_image_())
    return;
  if (now - this->last_update_ < this->update_interval_)
    return;

  this->pending_requesters_ = this->single_requesters_ | this->stream_requesters_;
  this->single_requesters_ = 0;
  this->frame_wanted_.store(true);
}

/* ---------------- MipiCsiCamera: requests ---------------- */

void MipiCsiCamera::report_throughput_(uint32_t now) {
  if (now - this->last_stats_ < THROUGHPUT_REPORT_INTERVAL_MS)
    return;
  // Only interesting while frames are actually being delivered.
  if (this->published_frames_ != 0) {
    const uint32_t elapsed = now - this->last_stats_;
    ESP_LOGD(TAG, "Delivered %" PRIu32 " frames in %" PRIu32 " ms (%.1f fps, avg %" PRIu32 " bytes), configured %u fps",
             this->published_frames_, elapsed, this->published_frames_ * 1000.0f / elapsed,
             this->published_bytes_ / this->published_frames_, this->framerate_);
    this->published_frames_ = 0;
    this->published_bytes_ = 0;
  }
  this->last_stats_ = now;
}

void MipiCsiCamera::request_image(camera::CameraRequester requester) { this->single_requesters_ |= (1U << requester); }

void MipiCsiCamera::start_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_) {
    listener->on_stream_start();
  }
  this->stream_requesters_ |= (1U << requester);
}

void MipiCsiCamera::stop_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_) {
    listener->on_stream_stop();
  }
  this->stream_requesters_ &= ~(1U << requester);
}

/* ---------------- MipiCsiImageReader ---------------- */

void MipiCsiImageReader::set_image(std::shared_ptr<camera::CameraImage> image) {
  this->image_ = std::move(image);
  this->offset_ = 0;
}

size_t MipiCsiImageReader::available() const {
  if (!this->image_)
    return 0;
  return this->image_->get_data_length() - this->offset_;
}

uint8_t *MipiCsiImageReader::peek_data_buffer() { return this->image_->get_data_buffer() + this->offset_; }

void MipiCsiImageReader::consume_data(size_t consumed) { this->offset_ += consumed; }

void MipiCsiImageReader::return_image() { this->image_.reset(); }

}  // namespace esphome::mipi_csi

#endif
