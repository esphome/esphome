#include "esp_video_camera.h"

#ifdef USE_ESP_IDF

#include "i2c_helper.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "esp_heap_caps.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

extern "C" {
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "linux/videodev2.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#endif
}

#ifndef V4L2_CID_JPEG_COMPRESSION_QUALITY
#define V4L2_CID_JPEG_COMPRESSION_QUALITY (V4L2_CID_JPEG_CLASS_BASE + 1)
#endif

namespace esphome::esp_video_camera {

static const char *const TAG = "esp_video_camera";

// ===========================================================================
// Pipeline init helpers (run esp_video_init on core 0, optional LEDC XCLK)
// ===========================================================================
namespace {

struct VideoInitParams {
  esp_video_init_config_t *config;
  esp_err_t result;
  SemaphoreHandle_t done;
};

// ESP32-P4 camera hardware must be initialised on core 0; run esp_video_init
// there regardless of which core ESPHome runs on.
void video_init_task_core0(void *param) {
  auto *p = static_cast<VideoInitParams *>(param);
  p->result = esp_video_init(p->config);
  xSemaphoreGive(p->done);
  vTaskDelete(nullptr);
}

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
// Pump USB Host Library events. esp_video is told not to own the USB host lib
// (init_usb_host_lib = false) so that we can tolerate it already being
// installed by another component; when we install it ourselves we run this
// daemon, when it is shared the existing owner pumps the events instead.
void usb_host_lib_daemon_task(void *param) {
  while (true) {
    uint32_t event_flags;
    if (usb_host_lib_handle_events(portMAX_DELAY, &event_flags) == ESP_OK) {
      if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        usb_host_device_free_all();
    }
  }
}
#endif

// Generate the sensor XCLK with LEDC. For MIPI-CSI sensors esp_video_init() does
// not start XCLK, so non-M5Stack boards must do it before init or the sensor
// stays silent on I2C.
esp_err_t init_xclk_ledc(gpio_num_t gpio_num, uint32_t freq_hz) {
  ledc_timer_config_t timer_conf = {};
  timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
  timer_conf.timer_num = LEDC_TIMER_0;
  timer_conf.duty_resolution = LEDC_TIMER_1_BIT;
  timer_conf.freq_hz = freq_hz;
  timer_conf.clk_cfg = LEDC_AUTO_CLK;
  esp_err_t ret = ledc_timer_config(&timer_conf);
  if (ret != ESP_OK)
    return ret;

  ledc_channel_config_t ch_conf = {};
  ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
  ch_conf.channel = LEDC_CHANNEL_0;
  ch_conf.timer_sel = LEDC_TIMER_0;
  ch_conf.intr_type = LEDC_INTR_DISABLE;
  ch_conf.gpio_num = gpio_num;
  ch_conf.duty = 1;  // 50 % duty cycle
  ch_conf.hpoint = 0;
  return ledc_channel_config(&ch_conf);
}

// Parse a resolution string into width/height. Accepts the aliases validated by
// the Python schema or an explicit "WIDTHxHEIGHT". Returns false for "auto".
bool parse_resolution(const std::string &res, uint32_t &width, uint32_t &height) {
  if (res.empty() || res == "auto")
    return false;

  struct ResAlias {
    const char *name;
    uint32_t width;
    uint32_t height;
  };
  static constexpr ResAlias ALIASES[] = {
      {"QVGA", 320, 240}, {"VGA", 640, 480}, {"480P", 640, 480}, {"720P", 1280, 720}, {"1080P", 1920, 1080},
  };
  for (const auto &alias : ALIASES) {
    if (res == alias.name) {
      width = alias.width;
      height = alias.height;
      return true;
    }
  }

  // Parse "WIDTHxHEIGHT" (already validated as digits by the Python schema).
  size_t x_pos = res.find('x');
  if (x_pos == std::string::npos || x_pos == 0 || x_pos + 1 >= res.size())
    return false;
  uint32_t w = 0, h = 0;
  for (size_t i = 0; i < x_pos; i++) {
    if (res[i] < '0' || res[i] > '9')
      return false;
    w = w * 10 + (res[i] - '0');
  }
  for (size_t i = x_pos + 1; i < res.size(); i++) {
    if (res[i] < '0' || res[i] > '9')
      return false;
    h = h * 10 + (res[i] - '0');
  }
  if (w == 0 || h == 0)
    return false;
  width = w;
  height = h;
  return true;
}

}  // namespace

// ===========================================================================
// ESPVideoCameraImage
// ===========================================================================
ESPVideoCameraImage::ESPVideoCameraImage(uint8_t *data, size_t length, uint8_t requesters)
    : data_(data), length_(length), requesters_(requesters) {}

ESPVideoCameraImage::~ESPVideoCameraImage() {
  if (this->data_ != nullptr) {
    heap_caps_free(this->data_);
    this->data_ = nullptr;
  }
}

bool ESPVideoCameraImage::was_requested_by(camera::CameraRequester requester) const {
  return (this->requesters_ & (1 << requester)) != 0;
}

// ===========================================================================
// ESPVideoCameraImageReader
// ===========================================================================
void ESPVideoCameraImageReader::set_image(std::shared_ptr<camera::CameraImage> image) {
  this->image_ = std::move(image);
  this->offset_ = 0;
}

size_t ESPVideoCameraImageReader::available() const {
  if (this->image_ == nullptr)
    return 0;
  return this->image_->get_data_length() - this->offset_;
}

uint8_t *ESPVideoCameraImageReader::peek_data_buffer() {
  if (this->image_ == nullptr)
    return nullptr;
  return this->image_->get_data_buffer() + this->offset_;
}

void ESPVideoCameraImageReader::consume_data(size_t consumed) { this->offset_ += consumed; }

void ESPVideoCameraImageReader::return_image() {
  this->image_.reset();
  this->offset_ = 0;
}

// ===========================================================================
// ESPVideoCamera — setup / pipeline init
// ===========================================================================
void ESPVideoCamera::setup() {
  if (!this->init_pipeline_()) {
    this->mark_failed();
    return;
  }

  // Resolve the device alias to a concrete /dev/videoN path.
  const std::string &d = this->device_;
  this->is_hw_jpeg_ = false;
  if (d.empty() || d == "jpeg" || d == ESP_VIDEO_JPEG_DEVICE_NAME) {
    this->resolved_device_ = ESP_VIDEO_JPEG_DEVICE_NAME;  // /dev/video10
    this->is_hw_jpeg_ = true;
  } else if (d == "csi") {
    this->resolved_device_ = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;  // /dev/video0
  } else if (d.starts_with("uvc")) {
    // "uvc" -> /dev/video40, "uvcN" -> /dev/video4N (N validated as a digit).
    const char *index = (d.size() == 4) ? (d.c_str() + 3) : "0";
    this->resolved_device_ = std::string(ESP_VIDEO_USB_UVC_NAME_PREFIX) + index;
  } else {
    this->resolved_device_ = d;
  }

  int test_fd = open(this->resolved_device_.c_str(), O_RDWR | O_NONBLOCK);
  if (test_fd < 0) {
    ESP_LOGE(TAG, "V4L2 device '%s' unavailable (errno=%d: %s)", this->resolved_device_.c_str(), errno,
             strerror(errno));
    this->mark_failed();
    return;
  }
  close(test_fd);

  ESP_LOGI(TAG, "Camera ready on %s (source: %s)", this->resolved_device_.c_str(), this->device_.c_str());
}

bool ESPVideoCamera::init_pipeline_() {
  if (this->i2c_bus_ == nullptr) {
    ESP_LOGE(TAG, "No I2C bus set");
    return false;
  }
  i2c_master_bus_handle_t i2c_handle = get_i2c_bus_handle(this->i2c_bus_);
  if (i2c_handle == nullptr) {
    ESP_LOGE(TAG, "Could not obtain the ESP-IDF I2C bus handle");
    return false;
  }

  // A "uvc" device streams from a USB camera only. In that case skip the
  // MIPI-CSI pipeline entirely: esp_video_init() runs sensor detection only
  // when config->csi != NULL, so leaving it NULL avoids trying (and failing)
  // to detect a MIPI sensor that isn't present on a USB-only board.
  const bool uvc_only = this->device_.rfind("uvc", 0) == 0;

  // Start XCLK via LEDC if requested (MIPI sensors need it before init).
  if (!uvc_only && this->enable_xclk_init_ && this->xclk_pin_ != (gpio_num_t) -1) {
    if (init_xclk_ledc(this->xclk_pin_, this->xclk_freq_) != ESP_OK) {
      ESP_LOGE(TAG, "XCLK init failed");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  esp_video_init_csi_config_t csi_config = {};
  csi_config.sccb_config.init_sccb = false;  // reuse the ESPHome I2C bus
  csi_config.sccb_config.i2c_handle = i2c_handle;
  csi_config.sccb_config.freq = 400000;
  csi_config.reset_pin = (gpio_num_t) -1;
  csi_config.pwdn_pin = (gpio_num_t) -1;
  // Note: esp_video >= 2.x no longer takes xclk_pin/xclk_freq in the CSI config.
  // The sensor XCLK is generated separately via LEDC (see init_xclk_ledc above).

  esp_video_init_config_t video_config = {};
  if (!uvc_only)
    video_config.csi = &csi_config;

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  esp_video_init_usb_uvc_config_t uvc_config = {};
  if (this->enable_uvc_) {
    uvc_config.uvc.uvc_dev_num = 1;
    uvc_config.uvc.task_stack = 4096;
    uvc_config.uvc.task_priority = 5;
    uvc_config.uvc.task_affinity = -1;

    // The USB Host Library can only be installed once per system. Manage it here
    // instead of letting esp_video own it, so that if another component (e.g.
    // ESPHome's usb_host) has already installed it we share the existing stack
    // instead of aborting esp_video_init(). When we install it ourselves we also
    // run the library event daemon; when it is already installed we leave the
    // events to the existing owner.
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    esp_err_t host_ret = usb_host_install(&host_config);
    if (host_ret == ESP_OK) {
      xTaskCreatePinnedToCore(usb_host_lib_daemon_task, "usb_lib", 4096, nullptr, 5, nullptr, tskNO_AFFINITY);
    } else if (host_ret == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "USB Host already installed by another component; sharing it for UVC");
    } else {
      ESP_LOGE(TAG, "usb_host_install() failed: %s", esp_err_to_name(host_ret));
    }
    uvc_config.usb.init_usb_host_lib = false;  // we manage the USB host library (see above)
    uvc_config.usb.task_stack = 4096;
    uvc_config.usb.task_priority = 5;
    uvc_config.usb.task_affinity = -1;
    video_config.usb_uvc = &uvc_config;
  }
#endif

  // Run esp_video_init() on core 0 (hardware requirement).
  SemaphoreHandle_t done = xSemaphoreCreateBinary();
  if (done == nullptr)
    return false;
  VideoInitParams params = {};
  params.config = &video_config;
  params.done = done;
  TaskHandle_t task = nullptr;
  if (xTaskCreatePinnedToCore(video_init_task_core0, "esp_video_init", 8192, &params, 5, &task, 0) != pdPASS) {
    vSemaphoreDelete(done);
    return false;
  }
  if (xSemaphoreTake(done, pdMS_TO_TICKS(10000)) != pdTRUE) {
    ESP_LOGE(TAG, "esp_video_init() timed out");
    vSemaphoreDelete(done);
    return false;
  }
  vSemaphoreDelete(done);

  if (params.result != ESP_OK) {
    ESP_LOGE(TAG, "esp_video_init() failed: %s", esp_err_to_name(params.result));
    return false;
  }
  this->pipeline_ready_ = true;
  return true;
}

// ===========================================================================
// ESPVideoCamera — streaming / capture
// ===========================================================================
void ESPVideoCamera::loop() {
  if (!this->streaming_)
    return;

  if (this->is_hw_jpeg_) {
    this->loop_jpeg_pipeline_();
  } else {
    this->loop_direct_capture_();
  }

  if (this->stream_requesters_ == 0 && this->single_requesters_ == 0)
    this->stop_capture_();
}

void ESPVideoCamera::deliver_frame_(const uint8_t *data, size_t length) {
  if (length == 0)
    return;
  uint32_t now = millis();
  if (this->min_interval_ms_ > 0 && (now - this->last_frame_ms_) < this->min_interval_ms_)
    return;  // throttled to max_framerate
  this->last_frame_ms_ = now;

  uint8_t *copy = (uint8_t *) heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (copy == nullptr)
    copy = (uint8_t *) heap_caps_malloc(length, MALLOC_CAP_8BIT);
  if (copy == nullptr) {
    ESP_LOGW(TAG, "Failed to allocate %u bytes (frame dropped)", (unsigned) length);
    return;
  }
  memcpy(copy, data, length);
  this->current_image_ =
      std::make_shared<ESPVideoCameraImage>(copy, length, this->single_requesters_ | this->stream_requesters_);
  for (auto *listener : this->listeners_)
    listener->on_camera_image(this->current_image_);
  this->single_requesters_ = 0;
}

void ESPVideoCamera::loop_direct_capture_() {
  // The device already delivers JPEG/MJPEG frames; one MMAP capture queue.
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->capture_fd_, VIDIOC_DQBUF, &buf) < 0) {
    if (errno != EAGAIN)
      ESP_LOGW(TAG, "VIDIOC_DQBUF failed: %s", strerror(errno));
    return;
  }

  if (buf.index < (uint32_t) this->num_capture_buffers_)
    this->deliver_frame_((const uint8_t *) this->capture_buffers_[buf.index].start, buf.bytesused);

  if (ioctl(this->capture_fd_, VIDIOC_QBUF, &buf) < 0)
    ESP_LOGW(TAG, "VIDIOC_QBUF failed: %s", strerror(errno));
}

void ESPVideoCamera::loop_jpeg_pipeline_() {
  // Dequeue one RGB565 frame from the sensor/ISP device (non-blocking).
  struct v4l2_buffer cap_buf;
  memset(&cap_buf, 0, sizeof(cap_buf));
  cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  cap_buf.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->capture_fd_, VIDIOC_DQBUF, &cap_buf) < 0) {
    if (errno != EAGAIN)
      ESP_LOGW(TAG, "capture DQBUF failed: %s", strerror(errno));
    return;
  }

  if (cap_buf.index < (uint32_t) this->num_capture_buffers_ && cap_buf.bytesused > 0) {
    // Feed the raw frame to the encoder OUTPUT queue (USERPTR) and re-arm the
    // CAPTURE queue (JPEG output). The JPEG fd is blocking, so the DQBUFs below
    // wait for the (fast) hardware encode to finish.
    struct v4l2_buffer out_buf;
    memset(&out_buf, 0, sizeof(out_buf));
    out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    out_buf.memory = V4L2_MEMORY_USERPTR;
    out_buf.index = 0;
    out_buf.m.userptr = (unsigned long) this->capture_buffers_[cap_buf.index].start;
    out_buf.length = this->capture_buffers_[cap_buf.index].length;
    out_buf.bytesused = cap_buf.bytesused;

    struct v4l2_buffer jpeg_buf;
    memset(&jpeg_buf, 0, sizeof(jpeg_buf));
    jpeg_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    jpeg_buf.memory = V4L2_MEMORY_MMAP;
    jpeg_buf.index = 0;

    if (ioctl(this->jpeg_fd_, VIDIOC_QBUF, &out_buf) == 0 && ioctl(this->jpeg_fd_, VIDIOC_QBUF, &jpeg_buf) == 0) {
      // Reclaim the consumed input buffer.
      struct v4l2_buffer done_buf;
      memset(&done_buf, 0, sizeof(done_buf));
      done_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      done_buf.memory = V4L2_MEMORY_USERPTR;
      ioctl(this->jpeg_fd_, VIDIOC_DQBUF, &done_buf);

      // Read back the encoded JPEG.
      memset(&jpeg_buf, 0, sizeof(jpeg_buf));
      jpeg_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      jpeg_buf.memory = V4L2_MEMORY_MMAP;
      if (ioctl(this->jpeg_fd_, VIDIOC_DQBUF, &jpeg_buf) == 0) {
        this->deliver_frame_((const uint8_t *) this->jpeg_out_buffer_.start, jpeg_buf.bytesused);
      } else {
        ESP_LOGW(TAG, "JPEG DQBUF failed: %s", strerror(errno));
      }
    } else {
      ESP_LOGW(TAG, "JPEG encoder QBUF failed: %s", strerror(errno));
    }
  }

  // Return the raw frame to the sensor/ISP device.
  if (ioctl(this->capture_fd_, VIDIOC_QBUF, &cap_buf) < 0)
    ESP_LOGW(TAG, "capture QBUF failed: %s", strerror(errno));
}

camera::CameraImageReader *ESPVideoCamera::create_image_reader() { return new ESPVideoCameraImageReader(); }

void ESPVideoCamera::request_image(camera::CameraRequester requester) {
  this->single_requesters_ |= (1U << requester);
  this->update_capture_state_();
}

void ESPVideoCamera::start_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_)
    listener->on_stream_start();
  this->stream_requesters_ |= (1U << requester);
  this->update_capture_state_();
}

void ESPVideoCamera::stop_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_)
    listener->on_stream_stop();
  this->stream_requesters_ &= ~(1U << requester);
  this->update_capture_state_();
}

void ESPVideoCamera::update_capture_state_() {
  bool wanted = (this->stream_requesters_ != 0) || (this->single_requesters_ != 0);
  if (wanted && !this->streaming_)
    this->start_capture_();
}

bool ESPVideoCamera::configure_capture_format_(uint32_t pixelformat) {
  uint32_t width = 0, height = 0;
  bool force_res = parse_resolution(this->resolution_, width, height);

  struct v4l2_format fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(this->capture_fd_, VIDIOC_G_FMT, &fmt);  // best-effort starting point
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.pixelformat = pixelformat;
  if (force_res) {
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
  }
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (ioctl(this->capture_fd_, VIDIOC_S_FMT, &fmt) < 0)
    ESP_LOGW(TAG, "VIDIOC_S_FMT (best-effort resolution) failed: %s", strerror(errno));

  // Read back the resolution actually negotiated by the sensor/ISP.
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->capture_fd_, VIDIOC_G_FMT, &fmt) == 0) {
    this->capture_width_ = fmt.fmt.pix.width;
    this->capture_height_ = fmt.fmt.pix.height;
  } else {
    this->capture_width_ = width;
    this->capture_height_ = height;
  }
  ESP_LOGI(TAG, "Capture resolution: %ux%u", (unsigned) this->capture_width_, (unsigned) this->capture_height_);
  return true;
}

bool ESPVideoCamera::setup_capture_buffers_() {
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = MAX_BUFFERS;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->capture_fd_, VIDIOC_REQBUFS, &req) < 0) {
    ESP_LOGE(TAG, "VIDIOC_REQBUFS failed: %s", strerror(errno));
    return false;
  }

  this->num_capture_buffers_ = 0;
  for (unsigned int i = 0; i < req.count && i < MAX_BUFFERS; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (ioctl(this->capture_fd_, VIDIOC_QUERYBUF, &buf) < 0) {
      ESP_LOGE(TAG, "VIDIOC_QUERYBUF[%u] failed: %s", i, strerror(errno));
      return false;
    }
    this->capture_buffers_[i].length = buf.length;
    this->capture_buffers_[i].start =
        mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->capture_fd_, buf.m.offset);
    if (this->capture_buffers_[i].start == MAP_FAILED) {
      this->capture_buffers_[i].start = nullptr;
      ESP_LOGE(TAG, "mmap[%u] failed: %s", i, strerror(errno));
      return false;
    }
    this->num_capture_buffers_++;
    if (ioctl(this->capture_fd_, VIDIOC_QBUF, &buf) < 0) {
      ESP_LOGE(TAG, "VIDIOC_QBUF[%u] failed: %s", i, strerror(errno));
      return false;
    }
  }
  return true;
}

bool ESPVideoCamera::start_capture_() {
  if (this->streaming_)
    return true;
  if (this->is_failed())
    return false;

  bool ok = this->is_hw_jpeg_ ? this->start_jpeg_pipeline_() : this->start_direct_capture_();
  if (!ok) {
    this->stop_capture_();
    return false;
  }
  this->streaming_ = true;
  this->last_frame_ms_ = 0;
  return true;
}

bool ESPVideoCamera::start_direct_capture_() {
  this->capture_fd_ = open(this->resolved_device_.c_str(), O_RDWR | O_NONBLOCK);
  if (this->capture_fd_ < 0) {
    ESP_LOGE(TAG, "open(%s) failed: %s", this->resolved_device_.c_str(), strerror(errno));
    return false;
  }
  if (!this->configure_capture_format_(V4L2_PIX_FMT_MJPEG))
    return false;
  if (!this->setup_capture_buffers_())
    return false;
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->capture_fd_, VIDIOC_STREAMON, &type) < 0) {
    ESP_LOGE(TAG, "VIDIOC_STREAMON failed: %s", strerror(errno));
    return false;
  }
  return true;
}

bool ESPVideoCamera::start_jpeg_pipeline_() {
  // Stage 1: sensor/ISP capture device producing RGB565 frames.
  this->capture_fd_ = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR | O_NONBLOCK);
  if (this->capture_fd_ < 0) {
    ESP_LOGE(TAG, "open(%s) failed: %s", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, strerror(errno));
    return false;
  }
  if (!this->configure_capture_format_(V4L2_PIX_FMT_RGB565))
    return false;
  if (!this->setup_capture_buffers_())
    return false;
  int ctype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->capture_fd_, VIDIOC_STREAMON, &ctype) < 0) {
    ESP_LOGE(TAG, "capture STREAMON failed: %s", strerror(errno));
    return false;
  }

  // Stage 2: JPEG hardware encoder (M2M). Blocking so the per-frame DQBUFs wait
  // for the (fast) hardware encode instead of busy-looping on EAGAIN.
  this->jpeg_fd_ = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
  if (this->jpeg_fd_ < 0) {
    ESP_LOGE(TAG, "open(%s) failed: %s", ESP_VIDEO_JPEG_DEVICE_NAME, strerror(errno));
    return false;
  }

  struct v4l2_format fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width = this->capture_width_;
  fmt.fmt.pix.height = this->capture_height_;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  if (ioctl(this->jpeg_fd_, VIDIOC_S_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "JPEG OUTPUT S_FMT failed: %s", strerror(errno));
    return false;
  }
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  if (ioctl(this->jpeg_fd_, VIDIOC_S_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "JPEG CAPTURE S_FMT failed: %s", strerror(errno));
    return false;
  }

  struct v4l2_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  ctrl.value = this->jpeg_quality_;
  ioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl);

  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = MAX_BUFFERS;
  req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  req.memory = V4L2_MEMORY_USERPTR;
  if (ioctl(this->jpeg_fd_, VIDIOC_REQBUFS, &req) < 0) {
    ESP_LOGE(TAG, "JPEG OUTPUT REQBUFS failed: %s", strerror(errno));
    return false;
  }
  memset(&req, 0, sizeof(req));
  req.count = 1;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->jpeg_fd_, VIDIOC_REQBUFS, &req) < 0) {
    ESP_LOGE(TAG, "JPEG CAPTURE REQBUFS failed: %s", strerror(errno));
    return false;
  }

  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = 0;
  if (ioctl(this->jpeg_fd_, VIDIOC_QUERYBUF, &buf) < 0) {
    ESP_LOGE(TAG, "JPEG QUERYBUF failed: %s", strerror(errno));
    return false;
  }
  this->jpeg_out_buffer_.length = buf.length;
  this->jpeg_out_buffer_.start =
      mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->jpeg_fd_, buf.m.offset);
  if (this->jpeg_out_buffer_.start == MAP_FAILED) {
    this->jpeg_out_buffer_.start = nullptr;
    ESP_LOGE(TAG, "JPEG mmap failed: %s", strerror(errno));
    return false;
  }

  int otype = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  int jtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &otype) < 0 || ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &jtype) < 0) {
    ESP_LOGE(TAG, "JPEG STREAMON failed: %s", strerror(errno));
    return false;
  }
  return true;
}

void ESPVideoCamera::stop_capture_() {
  if (this->jpeg_fd_ >= 0) {
    int otype = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int jtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &otype);
    ioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &jtype);
    if (this->jpeg_out_buffer_.start != nullptr) {
      munmap(this->jpeg_out_buffer_.start, this->jpeg_out_buffer_.length);
      this->jpeg_out_buffer_.start = nullptr;
    }
    close(this->jpeg_fd_);
    this->jpeg_fd_ = -1;
  }
  if (this->capture_fd_ >= 0) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->capture_fd_, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < this->num_capture_buffers_; i++) {
      if (this->capture_buffers_[i].start != nullptr) {
        munmap(this->capture_buffers_[i].start, this->capture_buffers_[i].length);
        this->capture_buffers_[i].start = nullptr;
      }
    }
    close(this->capture_fd_);
    this->capture_fd_ = -1;
  }
  this->num_capture_buffers_ = 0;
  this->streaming_ = false;
}

void ESPVideoCamera::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP-Video Camera:");
  ESP_LOGCONFIG(TAG, "  Name: %s", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Source: %s (%s)", this->device_.c_str(), this->resolved_device_.c_str());
  ESP_LOGCONFIG(TAG, "  Resolution: %s", this->resolution_.c_str());
  if (this->is_hw_jpeg_)
    ESP_LOGCONFIG(TAG, "  JPEG quality: %d", this->jpeg_quality_);
  ESP_LOGCONFIG(TAG, "  Max framerate: %.1f fps", this->max_framerate_);
  if (this->is_failed())
    ESP_LOGCONFIG(TAG, "  State: FAILED");
}

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF
