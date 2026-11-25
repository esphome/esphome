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

ESPHOME_LOG_TAG(TAG, "mipi_csi");

static constexpr uint32_t CAPTURE_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t CAPTURE_TASK_PRIORITY = 1;
#if CONFIG_FREERTOS_UNICORE
static constexpr BaseType_t CAPTURE_TASK_CORE = tskNO_AFFINITY;
#else
static constexpr BaseType_t CAPTURE_TASK_CORE = 1;
#endif
/// How long the capture task waits before retrying after a dequeue error.
static constexpr uint32_t CAPTURE_RETRY_DELAY_MS = 100;

/* ---------------- MipiCsiCamera: setup ---------------- */

void MipiCsiCamera::setup() {
  setup_output_pin(this->power_pin_, true);
  // Sensors need their clock running while they come out of reset
  if (!this->start_external_clock_()) {
    this->fail_(LOG_STR("Failed to start external clock"));
    return;
  }
  pulse_reset(this->reset_pin_, {.low_ms = 10, .high_after_ms = 20});
  if (!this->init_video_()) {
    this->fail_(LOG_STR("Failed to initialize video pipeline"));
    return;
  }
  const size_t frame_size = this->configure_device_();
  if (frame_size == 0) {
    this->fail_(LOG_STR("Failed to configure video device"));
    return;
  }

  this->reusable_image_ = std::make_shared<MipiCsiImage>();
  this->result_queue_ = xQueueCreate(1, sizeof(size_t));
  if (this->result_queue_ == nullptr) {
    this->fail_(LOG_STR("Failed to create frame queue"));
    return;
  }
  if (!this->start_streaming_(frame_size)) {
    this->fail_(LOG_STR("Failed to start video stream"));
    return;
  }
  if (xTaskCreatePinnedToCore(&MipiCsiCamera::capture_task, "mipi_csi", CAPTURE_TASK_STACK_SIZE, this,
                              CAPTURE_TASK_PRIORITY, nullptr, CAPTURE_TASK_CORE) != pdPASS) {
    this->fail_(LOG_STR("Failed to start capture task"));
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
  csi_config.reset_pin = GPIO_NUM_NC;
  csi_config.pwdn_pin = GPIO_NUM_NC;
#ifdef USE_MIPI_CSI_EXTERNAL_LDO
  csi_config.dont_init_ldo = true;
#endif

  esp_video_init_config_t video_config{};
  video_config.csi = &csi_config;

  // esp_video routes RAW sensors through the ISP and bypasses it for RGB/YUV ones
  err = esp_video_init_with_flags(&video_config, ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera sensor initialization failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

template<typename F> void MipiCsiCamera::for_each_sensor_format_(F &&fn) {
  v4l2_sensor_format_enum enumerator{};
  for (enumerator.index = 0; ioctl(this->fd_, VIDIOC_ENUM_SENSOR_FMT, &enumerator) == 0; enumerator.index++) {
    if (enumerator.format.port == ESP_CAM_SENSOR_MIPI_CSI)
      fn(enumerator.format);
  }
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
    this->for_each_sensor_format_([&](const esp_cam_sensor_format_t &candidate) {
      if (candidate.width != this->width_ || candidate.height != this->height_)
        return;
      if (!found || this->is_better_sensor_format_(candidate, chosen)) {
        chosen = candidate;
        found = true;
      }
    });

    if (!found) {
      ESP_LOGE(TAG, "The sensor cannot capture %ux%u. Supported resolutions are:", this->width_, this->height_);
      this->for_each_sensor_format_([](const esp_cam_sensor_format_t &format) {
        ESP_LOGE(TAG, "  %ux%u at %u fps (%s)", format.width, format.height, format.fps,
                 format.name != nullptr ? format.name : LOG_STR_LITERAL("unnamed"));
      });
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

bool MipiCsiCamera::is_better_sensor_format_(const esp_cam_sensor_format_t &candidate,
                                             const esp_cam_sensor_format_t &current) const {
  // Formats that carry ISP information are the RAW ones, and only those let the ISP run the image
  // tuning algorithms, so they are always preferred.
  if ((candidate.isp_info != nullptr) != (current.isp_info != nullptr))
    return candidate.isp_info != nullptr;
  // Without the ISP, only a sensor output in the configured pixel format works on every chip revision
  if ((candidate.format == this->sensor_format_) != (current.format == this->sensor_format_))
    return candidate.format == this->sensor_format_;

  // Prefer the rate closest to the configured one that the driver can reach by dropping frames
  return this->reachable_framerate_(candidate.fps) > this->reachable_framerate_(current.fps);
}

uint8_t MipiCsiCamera::reachable_framerate_(uint8_t sensor_fps) const {
  if (sensor_fps == 0)
    return this->framerate_;
  // The driver can only divide the sensor's rate
  uint8_t fps = std::min(this->framerate_, sensor_fps);
  while (sensor_fps % fps != 0)
    fps--;
  return fps;
}

uint8_t MipiCsiCamera::resolve_framerate_(uint8_t sensor_fps) const {
  const uint8_t fps = this->reachable_framerate_(sensor_fps);
  if (fps != this->framerate_) {
    ESP_LOGW(TAG, "The sensor runs at %u fps, so %u fps is used instead of %u fps", sensor_fps, fps, this->framerate_);
  }
  return fps;
}

size_t MipiCsiCamera::configure_device_() {
  this->fd_ = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "Opening %s failed: %s", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, strerror(errno));
    return 0;
  }

  if (!this->select_sensor_format_())
    return 0;

  v4l2_format format{};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = this->width_;
  format.fmt.pix.height = this->height_;
  format.fmt.pix.pixelformat = this->fourcc_;
  if (ioctl(this->fd_, VIDIOC_S_FMT, &format) != 0) {
    ESP_LOGE(TAG, "The camera cannot deliver '%.4s' at %ux%u: %s", reinterpret_cast<const char *>(&this->fourcc_),
             this->width_, this->height_, strerror(errno));
    return 0;
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

  if (this->flip_flags_ & FLIP_HORIZONTAL_SET)
    this->apply_control_(V4L2_CID_HFLIP, (this->flip_flags_ & FLIP_HORIZONTAL_ON) != 0, LOG_STR("horizontal flip"));
  if (this->flip_flags_ & FLIP_VERTICAL_SET)
    this->apply_control_(V4L2_CID_VFLIP, (this->flip_flags_ & FLIP_VERTICAL_ON) != 0, LOG_STR("vertical flip"));

  if (!this->read_back_format_())
    return 0;

  const size_t frame_size = static_cast<size_t>(this->width_) * this->height_ * this->bytes_per_pixel_;
  return this->encoder_.init(this->width_, this->height_, frame_size) ? frame_size : 0;
}

bool MipiCsiCamera::read_back_format_() {
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
  const char *actual_fourcc = reinterpret_cast<const char *>(&actual.fmt.pix.pixelformat);
  ESP_LOGD(TAG, "Capture format: %" PRIu32 "x%" PRIu32 " '%.4s'", actual.fmt.pix.width, actual.fmt.pix.height,
           actual_fourcc);

  // A different pixel format means every byte of the frame would be read with the wrong layout, so
  // the encoder would turn out a corrupted picture rather than a wrong-looking one.
  if (actual.fmt.pix.pixelformat != this->fourcc_) {
    ESP_LOGE(TAG, "The camera delivers '%.4s' instead of the configured '%.4s'; pick another 'pixel_format'",
             actual_fourcc, reinterpret_cast<const char *>(&this->fourcc_));
    return false;
  }

  this->width_ = actual.fmt.pix.width;
  this->height_ = actual.fmt.pix.height;
  return true;
}

void MipiCsiCamera::apply_control_(uint32_t id, int32_t value, const LogString *name) {
  v4l2_ext_control control{};
  control.id = id;
  control.value = value;

  v4l2_ext_controls controls{};
  controls.ctrl_class = V4L2_CTRL_CLASS_USER;
  controls.count = 1;
  controls.controls = &control;

  if (ioctl(this->fd_, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
    ESP_LOGW(TAG, "Setting %s failed: %s", LOG_STR_ARG(name), strerror(errno));
  }
}

bool MipiCsiCamera::start_streaming_(size_t frame_size) {
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

    if (buffer.length < frame_size) {
      ESP_LOGE(TAG, "Frame buffer %" PRIu32 " holds %" PRIu32 " bytes but a frame needs %zu", i, buffer.length,
               frame_size);
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

void MipiCsiCamera::fail_(const LogString *reason) {
  if (this->result_queue_ != nullptr) {
    vQueueDelete(this->result_queue_);
    this->result_queue_ = nullptr;
  }

  if (this->fd_ >= 0) {
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
  this->mark_failed(reason);
}

static const LogString *flip_state(uint8_t flags, uint8_t set_bit, uint8_t on_bit) {
  if ((flags & set_bit) == 0)
    return LOG_STR("default");
  return (flags & on_bit) != 0 ? LOG_STR("on") : LOG_STR("off");
}

void MipiCsiCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MIPI-CSI camera '%s':\n"
                "  Sensor: %s\n"
                "  Resolution: %ux%u\n"
                "  Capture format: %.4s\n"
                "  JPEG quality: %u\n"
                "  Frame rate: %u fps\n"
                "  Frame buffers: %u\n"
                "  Flip: horizontal %s, vertical %s",
                LOG_STR_ARG(this->get_log_name()), this->sensor_name_, this->width_, this->height_,
                reinterpret_cast<const char *>(&this->fourcc_), this->encoder_.get_quality(), this->framerate_,
                this->frame_buffer_count_,
                LOG_STR_ARG(flip_state(this->flip_flags_, FLIP_HORIZONTAL_SET, FLIP_HORIZONTAL_ON)),
                LOG_STR_ARG(flip_state(this->flip_flags_, FLIP_VERTICAL_SET, FLIP_VERTICAL_ON)));
  if (this->xclk_pin_ >= 0) {
    ESP_LOGCONFIG(TAG, "  External clock: GPIO%d at %" PRIu32 " Hz", this->xclk_pin_, this->xclk_frequency_);
  }
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Power Pin: ", this->power_pin_);
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
  if (this->current_image_ && this->current_image_.use_count() == 2) {
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
        return;
      }
      this->reusable_image_->set_frame(this->encoder_.get_output_buffer(), length, this->pending_requesters_);
      this->current_image_ = this->reusable_image_;
      this->pending_requesters_ = 0;
      for (uint8_t i = 0; i < this->listener_count_; i++) {
        auto *listener = this->listeners_[i];
        listener->on_camera_image(this->current_image_);
      }
#ifdef USE_MIPI_CSI_IMAGE_CALLBACK
      this->image_callback_.call(CameraImageData{this->current_image_->get_data_buffer(), length});
#endif
      return;
    }
  }

  // Ask Home Assistant's still image to refresh every so often even when nobody is watching.
  if (!this->has_requested_image_() && this->idle_update_interval_ != 0 &&
      now - this->last_idle_request_ >= this->idle_update_interval_) {
    this->last_idle_request_ = now;
    this->request_image(camera::IDLE);
  }

  // Ask the capture task for the next frame. Not while an image is still in flight, because both
  // share the encoder's single output buffer.
  if (this->current_image_ || this->frame_wanted_.load() || uxQueueMessagesWaiting(this->result_queue_) != 0 ||
      !this->has_requested_image_())
    return;
  if (now - this->last_request_ < 1000U / this->framerate_)
    return;

  this->pending_requesters_ = this->single_requesters_.exchange(0) | this->stream_requesters_.load();
  this->last_request_ = now;
  this->frame_wanted_.store(true);
}

/* ---------------- MipiCsiCamera: requests ---------------- */

// Called from the web server's task too: wake the loop so a long loop interval does not delay the frame
void MipiCsiCamera::request_image(camera::CameraRequester requester) {
  this->single_requesters_ |= (1U << requester);
  App.wake_loop_threadsafe();
}

void MipiCsiCamera::start_stream(camera::CameraRequester requester) {
  this->defer([this]() {
    for (uint8_t i = 0; i < this->listener_count_; i++) {
      auto *listener = this->listeners_[i];
      listener->on_stream_start();
    }
#ifdef USE_MIPI_CSI_STREAM_CALLBACK
    this->stream_callback_.call(true);
#endif
  });
  this->stream_requesters_ |= (1U << requester);
  App.wake_loop_threadsafe();
}

void MipiCsiCamera::stop_stream(camera::CameraRequester requester) {
  this->defer([this]() {
    for (uint8_t i = 0; i < this->listener_count_; i++) {
      auto *listener = this->listeners_[i];
      listener->on_stream_stop();
    }
#ifdef USE_MIPI_CSI_STREAM_CALLBACK
    this->stream_callback_.call(false);
#endif
  });
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
