#include "esp_video_camera.h"
#include "i2c_helper.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "esp_heap_caps.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

extern "C" {
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "linux/videodev2.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
}

#ifndef V4L2_CID_JPEG_COMPRESSION_QUALITY
#define V4L2_CID_JPEG_COMPRESSION_QUALITY (V4L2_CID_JPEG_CLASS_BASE + 1)
#endif

namespace esphome {
namespace esp_video_camera {

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
  if (d == "jpeg" || d.empty()) {
    this->resolved_device_ = ESP_VIDEO_JPEG_DEVICE_NAME;  // /dev/video10
    this->is_hw_jpeg_ = true;
  } else if (d == "uvc") {
    this->resolved_device_ = ESP_VIDEO_USB_UVC_NAME_PREFIX "0";  // /dev/video40
  } else if (d.rfind("uvc", 0) == 0 && d.size() == 4) {
    this->resolved_device_ = std::string(ESP_VIDEO_USB_UVC_NAME_PREFIX) + d.substr(3);
  } else if (d == "csi") {
    this->resolved_device_ = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;  // /dev/video0
  } else {
    this->resolved_device_ = d;
    this->is_hw_jpeg_ = (d == ESP_VIDEO_JPEG_DEVICE_NAME);
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

  // Start XCLK via LEDC if requested (MIPI sensors need it before init).
  if (this->enable_xclk_init_ && this->xclk_pin_ != (gpio_num_t) -1) {
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
  csi_config.xclk_pin = this->xclk_pin_;
  csi_config.xclk_freq = this->xclk_freq_;

  esp_video_init_config_t video_config = {};
  video_config.csi = &csi_config;

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  esp_video_init_usb_uvc_config_t uvc_config = {};
  if (this->enable_uvc_) {
    uvc_config.uvc.uvc_dev_num = 1;
    uvc_config.uvc.task_stack = 4096;
    uvc_config.uvc.task_priority = 5;
    uvc_config.uvc.task_affinity = -1;
    uvc_config.usb.init_usb_host_lib = true;
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

  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->fd_, VIDIOC_DQBUF, &buf) < 0) {
    if (errno == EAGAIN)
      return;
    ESP_LOGW(TAG, "VIDIOC_DQBUF failed: %s", strerror(errno));
    return;
  }

  bool throttled = false;
  uint32_t now = millis();
  if (this->min_interval_ms_ > 0 && (now - this->last_frame_ms_) < this->min_interval_ms_)
    throttled = true;

  if (!throttled && buf.index < (uint32_t) this->num_buffers_ && buf.bytesused > 0) {
    this->last_frame_ms_ = now;
    size_t len = buf.bytesused;
    uint8_t *copy = (uint8_t *) heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (copy == nullptr)
      copy = (uint8_t *) heap_caps_malloc(len, MALLOC_CAP_8BIT);
    if (copy != nullptr) {
      memcpy(copy, this->buffers_[buf.index].start, len);
      this->current_image_ =
          std::make_shared<ESPVideoCameraImage>(copy, len, this->single_requesters_ | this->stream_requesters_);
      for (auto *listener : this->listeners_)
        listener->on_camera_image(this->current_image_);
    } else {
      ESP_LOGW(TAG, "Failed to allocate %u bytes (frame dropped)", (unsigned) len);
    }
    this->single_requesters_ = 0;
  }

  if (ioctl(this->fd_, VIDIOC_QBUF, &buf) < 0)
    ESP_LOGW(TAG, "VIDIOC_QBUF failed: %s", strerror(errno));

  if (this->stream_requesters_ == 0 && this->single_requesters_ == 0)
    this->stop_capture_();
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

bool ESPVideoCamera::parse_resolution_(const std::string &res, uint32_t &width, uint32_t &height) {
  if (res.empty() || res == "auto")
    return false;
  if (res == "QVGA") { width = 320; height = 240; return true; }
  if (res == "VGA" || res == "480P") { width = 640; height = 480; return true; }
  if (res == "720P") { width = 1280; height = 720; return true; }
  if (res == "1080P") { width = 1920; height = 1080; return true; }
  unsigned int w = 0, h = 0;
  if (sscanf(res.c_str(), "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
    width = w;
    height = h;
    return true;
  }
  return false;
}

void ESPVideoCamera::configure_format_() {
  uint32_t width = 0, height = 0;
  bool force_res = parse_resolution_(this->resolution_, width, height);

  if (!this->is_hw_jpeg_ || force_res) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(this->fd_, VIDIOC_G_FMT, &fmt) == 0) {
      if (!this->is_hw_jpeg_)
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
      if (force_res) {
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
      }
      fmt.fmt.pix.field = V4L2_FIELD_NONE;
      if (ioctl(this->fd_, VIDIOC_S_FMT, &fmt) < 0)
        ESP_LOGW(TAG, "VIDIOC_S_FMT (best-effort resolution) failed: %s", strerror(errno));
    }
  }

  if (this->is_hw_jpeg_) {
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    ctrl.value = this->jpeg_quality_;
    ioctl(this->fd_, VIDIOC_S_CTRL, &ctrl);
  }
}

bool ESPVideoCamera::start_capture_() {
  if (this->streaming_)
    return true;
  if (this->is_failed())
    return false;

  this->fd_ = open(this->resolved_device_.c_str(), O_RDWR | O_NONBLOCK);
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "open(%s) failed: %s", this->resolved_device_.c_str(), strerror(errno));
    return false;
  }

  this->configure_format_();

  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = MAX_BUFFERS;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->fd_, VIDIOC_REQBUFS, &req) < 0) {
    ESP_LOGE(TAG, "VIDIOC_REQBUFS failed: %s", strerror(errno));
    this->stop_capture_();
    return false;
  }

  this->num_buffers_ = 0;
  for (unsigned int i = 0; i < req.count && i < MAX_BUFFERS; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (ioctl(this->fd_, VIDIOC_QUERYBUF, &buf) < 0) {
      ESP_LOGE(TAG, "VIDIOC_QUERYBUF[%u] failed: %s", i, strerror(errno));
      this->stop_capture_();
      return false;
    }
    this->buffers_[i].length = buf.length;
    this->buffers_[i].start =
        mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd_, buf.m.offset);
    if (this->buffers_[i].start == MAP_FAILED) {
      ESP_LOGE(TAG, "mmap[%u] failed: %s", i, strerror(errno));
      this->buffers_[i].start = nullptr;
      this->stop_capture_();
      return false;
    }
    this->num_buffers_++;
    if (ioctl(this->fd_, VIDIOC_QBUF, &buf) < 0) {
      ESP_LOGE(TAG, "VIDIOC_QBUF[%u] failed: %s", i, strerror(errno));
      this->stop_capture_();
      return false;
    }
  }

  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->fd_, VIDIOC_STREAMON, &type) < 0) {
    ESP_LOGE(TAG, "VIDIOC_STREAMON failed: %s", strerror(errno));
    this->stop_capture_();
    return false;
  }

  this->streaming_ = true;
  this->last_frame_ms_ = 0;
  return true;
}

void ESPVideoCamera::stop_capture_() {
  if (this->fd_ >= 0) {
    if (this->streaming_) {
      int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      ioctl(this->fd_, VIDIOC_STREAMOFF, &type);
    }
    for (int i = 0; i < this->num_buffers_; i++) {
      if (this->buffers_[i].start != nullptr) {
        munmap(this->buffers_[i].start, this->buffers_[i].length);
        this->buffers_[i].start = nullptr;
      }
    }
    close(this->fd_);
    this->fd_ = -1;
  }
  this->num_buffers_ = 0;
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

}  // namespace esp_video_camera
}  // namespace esphome
