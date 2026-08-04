#include "esp_video_camera.h"

// This component is ESP32-P4 silicon (MIPI-CSI, ISP, hardware JPEG) and builds
// only against esp_video's V4L2 headers, so it compiles on that variant alone.
#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "i2c_helper.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "esp_heap_caps.h"
#include "esp_log.h"           // esp_log_level_set()
#include "esp_memory_utils.h"  // esp_ptr_external_ram()

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

extern "C" {
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
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

// How long setup() waits for esp_video_init() on core 0 before giving up.
static constexpr uint32_t INIT_TIMEOUT_MS = 10000;

// Frame buffers are rounded up to this many bytes so that consecutive frames of
// slightly different sizes reuse the same heap block (see deliver_frame_).
static constexpr size_t FRAME_ALLOC_GRANULARITY = 4096;

// Upper bound on how long a JPEG encode may hold the main loop (see
// start_jpeg_pipeline_). Generous next to a real 720p encode, small enough that
// a wedged encoder cannot trip the task watchdog.
static constexpr uint32_t JPEG_DQBUF_TIMEOUT_MS = 100;

// Same guard for the sensor/ISP capture device. It normally delivers a frame
// every 33-66 ms, so this is a hang guard, not a poll interval: a sensor that
// stops producing must not take the main loop down with it.
static constexpr uint32_t CAPTURE_DQBUF_TIMEOUT_MS = 500;

// How long the pipeline keeps running after the last consumer went away.
//
// Every VIDIOC_STREAMOFF on the MIPI-CSI device destroys the ISP processor
// (esp_video's csi_video_stop -> stop_isp -> esp_isp_del_processor) and every
// STREAMON builds a new one and re-runs the AE/AWB/gamma pipeline from
// scratch, while esp_video's own ISP controller task keeps running throughout.
// Espressif's examples stream on once and never stop, so cycling that on every
// browser reconnect or single-image request is both slow (the exposure has to
// re-converge, so the first frames are visibly wrong) and needless churn in a
// part of esp_video that is not written for it. Idling for a few seconds costs
// nothing and turns a reconnect or a burst of snapshots into a no-op.
static constexpr uint32_t CAPTURE_IDLE_TIMEOUT_MS = 5000;

// ===========================================================================
// Pipeline init helpers (run esp_video_init on core 0, optional LEDC XCLK)
// ===========================================================================
namespace {

// Shared between init_pipeline_() and the core-0 init task. It owns the configs
// esp_video_init() reads, so they must outlive init_pipeline_(): the wait below
// can time out while the task is still running, and stack-allocated configs
// would be gone by then. Both parties hold a reference; the last one to let go
// destroys it.
struct VideoInitContext {
  esp_video_init_csi_config_t csi_config{};
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  esp_video_init_usb_uvc_config_t uvc_config{};
#endif
  esp_video_init_config_t video_config{};
  SemaphoreHandle_t done{nullptr};
  esp_err_t result{ESP_FAIL};
  std::atomic<int> refs{2};

  void release() {
    if (this->refs.fetch_sub(1) == 1) {
      if (this->done != nullptr)
        vSemaphoreDelete(this->done);
      delete this;
    }
  }
};

// ESP32-P4 camera hardware must be initialised on core 0; run esp_video_init
// there regardless of which core ESPHome runs on.
void video_init_task_core0(void *param) {
  auto *ctx = static_cast<VideoInitContext *>(param);
  ctx->result = esp_video_init(&ctx->video_config);
  xSemaphoreGive(ctx->done);
  ctx->release();
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

// Parse a resolution string into width/height. The Python schema normalises
// every accepted spelling (including the QVGA/VGA/720P/... aliases) to
// "WIDTHxHEIGHT", so that is the only form handled here. Returns false for
// "auto".
bool parse_resolution(const std::string &res, uint32_t &width, uint32_t &height) {
  if (res.empty() || res == "auto")
    return false;

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

// Render a V4L2 fourcc for logging, e.g. V4L2_PIX_FMT_MJPEG -> "MJPG".
std::string fourcc_to_string(uint32_t fourcc) {
  std::string out(4, ' ');
  for (int i = 0; i < 4; i++) {
    char c = (char) ((fourcc >> (8 * i)) & 0xFF);
    out[i] = (c >= 0x20 && c < 0x7F) ? c : '?';
  }
  return out;
}

// A capture device can vanish under us (a USB-UVC camera being unplugged is the
// common case). Only the errno values that unambiguously mean "the device is
// gone" count: EAGAIN is the empty non-blocking queue, and EIO is reported for
// transient frame errors too, so neither one may tear the capture down.
bool errno_means_device_gone(int err) { return err == ENODEV || err == ENXIO; }

// Bound how long VIDIOC_DQBUF may block on a device.
//
// O_NONBLOCK does nothing here: esp_video_vfs_open() ignores its `flags`
// argument and esp_video never returns EAGAIN anywhere, so every DQBUF is a
// blocking wait whose default is dqbuf_timeout_ticks = portMAX_DELAY. Since
// these DQBUFs run in the ESPHome main loop, an unbounded wait means the task
// watchdog fires. VIDIOC_S_DQBUF_TIMEOUT is esp_video's private ioctl for it.
//
// A timeout surfaces as ESP_FAIL, which esp_err_to_errno() maps to EPERM.
void set_dqbuf_timeout(int fd, uint32_t timeout_ms, const char *what) {
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) < 0)
    ESP_LOGW(TAG, "Could not bound the %s DQBUF wait: %s", what, strerror(errno));
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
  // Drop esp_video's per-frame ISP statistics dump before anything can emit it.
  //
  // That dump (print_stats_info) runs in esp_video's ISP controller task, which
  // has a hard-coded 4 KB stack at priority 11 and already runs the whole IPA
  // pipeline on it. It is compiled in whenever the IDF log level is DEBUG,
  // which is ESPHome's default, and it prints tens of lines per frame. Each one
  // reaches ESPHome from a non-main task, so it is formatted with a 144-byte
  // buffer on that same 4 KB stack -- and a blown stack in that task is a
  // "Load access fault" panic, not a clean stack-overflow report. Info and
  // above still come through; only the per-frame spam is dropped.
  esp_log_level_set("ISP", ESP_LOG_INFO);

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

  // The hardware-JPEG path encodes frames produced by the MIPI-CSI device, so the
  // encoder alone is not enough: /dev/video0 only exists once esp_video_init()
  // has actually found a sensor on the SCCB bus. The encoder is always there, so
  // without this check the component reports itself ready and then silently never
  // delivers a frame.
  if (this->is_hw_jpeg_) {
    int csi_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR | O_NONBLOCK);
    if (csi_fd < 0) {
      ESP_LOGE(TAG,
               "No MIPI-CSI sensor detected: %s is unavailable. Check the sensor wiring and that it "
               "answers on the configured I2C bus; the drivers built into this firmware are listed "
               "in the config dump above.",
               ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
      this->mark_failed();
      return;
    }
    close(csi_fd);
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
  const bool uvc_only = this->device_.starts_with("uvc");

  // Start XCLK via LEDC if requested (MIPI sensors need it before init).
  if (!uvc_only && this->enable_xclk_init_ && this->xclk_pin_ != (gpio_num_t) -1) {
    if (init_xclk_ledc(this->xclk_pin_, this->xclk_freq_) != ESP_OK) {
      ESP_LOGE(TAG, "XCLK init failed");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  // Heap-allocated so the configs stay valid for the init task even if the wait
  // below times out (see VideoInitContext).
  auto *ctx = new VideoInitContext();
  ctx->done = xSemaphoreCreateBinary();
  if (ctx->done == nullptr) {
    ctx->refs.store(1);  // no task was started
    ctx->release();
    return false;
  }

  esp_video_init_csi_config_t &csi_config = ctx->csi_config;
  csi_config.sccb_config.init_sccb = false;  // reuse the ESPHome I2C bus
  csi_config.sccb_config.i2c_handle = i2c_handle;
  csi_config.sccb_config.freq = 400000;
  csi_config.reset_pin = (gpio_num_t) -1;
  csi_config.pwdn_pin = (gpio_num_t) -1;
  // Note: esp_video >= 2.x no longer takes xclk_pin/xclk_freq in the CSI config.
  // The sensor XCLK is generated separately via LEDC (see init_xclk_ledc above).

  esp_video_init_config_t &video_config = ctx->video_config;
  if (!uvc_only)
    video_config.csi = &csi_config;

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  esp_video_init_usb_uvc_config_t &uvc_config = ctx->uvc_config;
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
      // Without the USB Host library the UVC device can never enumerate, so
      // there is nothing to gain from continuing into esp_video_init().
      ESP_LOGE(TAG, "usb_host_install() failed: %s", esp_err_to_name(host_ret));
      ctx->refs.store(1);  // no task was started
      ctx->release();
      return false;
    }
    uvc_config.usb.init_usb_host_lib = false;  // we manage the USB host library (see above)
    uvc_config.usb.task_stack = 4096;
    uvc_config.usb.task_priority = 5;
    uvc_config.usb.task_affinity = -1;
    video_config.usb_uvc = &uvc_config;
  }
#endif

  // Run esp_video_init() on core 0 (hardware requirement).
  if (xTaskCreatePinnedToCore(video_init_task_core0, "esp_video_init", 8192, ctx, 5, nullptr, 0) != pdPASS) {
    ESP_LOGE(TAG, "Could not start the esp_video_init task");
    ctx->refs.store(1);  // no task took a reference
    ctx->release();
    return false;
  }

  bool ok = false;
  if (xSemaphoreTake(ctx->done, pdMS_TO_TICKS(INIT_TIMEOUT_MS)) != pdTRUE) {
    // The task keeps running and keeps its reference to ctx; it frees the
    // context itself once esp_video_init() eventually returns.
    ESP_LOGE(TAG, "esp_video_init() timed out");
  } else if (ctx->result != ESP_OK) {
    ESP_LOGE(TAG, "esp_video_init() failed: %s", esp_err_to_name(ctx->result));
  } else {
    ok = true;
  }
  ctx->release();

  this->pipeline_ready_ = ok;
  return ok;
}

// ===========================================================================
// ESPVideoCamera — streaming / capture
// ===========================================================================
void ESPVideoCamera::loop() {
  const bool wanted = (this->stream_requesters_ != 0) || (this->single_requesters_ != 0);

  if (!this->streaming_) {
    if (!wanted)
      return;
    // A device that vanished mid-stream (see handle_device_gone_) is re-opened on
    // a timer; a fresh request starts right away.
    if (this->capture_retry_pending_ && (int32_t) (millis() - this->capture_retry_at_ms_) < 0)
      return;
    this->capture_retry_pending_ = false;
    this->start_capture_();
    return;
  }

  if (this->is_hw_jpeg_) {
    this->loop_jpeg_pipeline_();
  } else {
    this->loop_direct_capture_();
  }

  // Keep dequeuing and re-queuing while idle so the buffers stay in flight;
  // deliver_frame_() drops the frames instead of copying them. Only tear the
  // pipeline down once nobody has come back for a while (CAPTURE_IDLE_TIMEOUT_MS).
  if (wanted) {
    this->idle_since_ms_ = 0;
  } else if (this->idle_since_ms_ == 0) {
    this->idle_since_ms_ = millis() | 1u;  // never 0, that is the "not idle" marker
  } else if ((millis() - this->idle_since_ms_) >= CAPTURE_IDLE_TIMEOUT_MS) {
    this->stop_capture_();
  }
}

bool ESPVideoCamera::handle_device_gone_(int err) {
  if (!errno_means_device_gone(err))
    return false;
  ESP_LOGW(TAG, "Capture device '%s' disappeared (%s); will retry", this->resolved_device_.c_str(), strerror(err));
  this->stop_capture_();
  this->capture_retry_pending_ = true;
  this->capture_retry_at_ms_ = millis() + CAPTURE_RETRY_INTERVAL_MS;
  return true;
}

void ESPVideoCamera::deliver_frame_(const uint8_t *data, size_t length) {
  if (length == 0)
    return;
  // Nobody is watching: the pipeline is only still running out its idle grace
  // period (see loop()), so throw the frame away instead of copying it.
  auto requesters = (uint8_t) (this->single_requesters_ | this->stream_requesters_);
  if (requesters == 0)
    return;
  uint32_t now = millis();
  if (this->min_interval_ms_ > 0 && (now - this->last_frame_ms_) < this->min_interval_ms_)
    return;  // throttled to max_framerate
  this->last_frame_ms_ = now;

  // JPEG frames vary in size from frame to frame. Allocating the exact length
  // every time hands the allocator a different size class on each iteration and
  // fragments the heap; rounding up to a block boundary means the freed frame is
  // almost always reusable for the next one.
  size_t alloc_size = (length + FRAME_ALLOC_GRANULARITY - 1) & ~(size_t) (FRAME_ALLOC_GRANULARITY - 1);
  uint8_t *copy = (uint8_t *) heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (copy == nullptr)
    copy = (uint8_t *) heap_caps_malloc(alloc_size, MALLOC_CAP_8BIT);
  if (copy == nullptr) {
    ESP_LOGW(TAG, "Failed to allocate %u bytes (frame dropped)", (unsigned) alloc_size);
    return;
  }
  memcpy(copy, data, length);
  this->current_image_ = std::make_shared<ESPVideoCameraImage>(copy, length, requesters);
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
    int err = errno;
    if (err != EAGAIN && !this->handle_device_gone_(err))
      ESP_LOGW(TAG, "VIDIOC_DQBUF failed: %s", strerror(err));
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
    int err = errno;
    if (err != EAGAIN && !this->handle_device_gone_(err))
      ESP_LOGW(TAG, "capture DQBUF failed: %s", strerror(err));
    return;
  }

  // While the pipeline is only running out its idle grace period there is
  // nobody to hand a frame to, so skip the encode entirely and just cycle the
  // sensor buffer. The encoder is left exactly as STREAMON leaves it (its
  // CAPTURE buffer queued, OUTPUT idle), so the next requester resumes into a
  // ready encoder.
  const bool wanted = (this->stream_requesters_ != 0) || (this->single_requesters_ != 0);

  bool encoder_broken = false;
  if (wanted && cap_buf.index < (uint32_t) this->num_capture_buffers_ && cap_buf.bytesused > 0) {
    // M2M encode, in the order Espressif's own example uses (esp_video
    // examples/m2m, m2m_encode_frame): queue the raw frame on OUTPUT, dequeue
    // the encoded frame from CAPTURE, and only then reclaim the OUTPUT buffer.
    //
    // The order matters. The encoder releases the input buffer as part of
    // completing the output, so waiting on OUTPUT first deadlocks against an
    // encode that cannot finish -- which showed up as the OUTPUT DQBUF timing
    // out ("Not owner", esp_video's EPERM for ESP_FAIL) on every frame.
    //
    // The CAPTURE buffer is queued once in start_jpeg_pipeline_() and re-queued
    // below after its contents have been copied out, rather than re-queued
    // before every encode.
    struct v4l2_buffer out_buf;
    memset(&out_buf, 0, sizeof(out_buf));
    out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    out_buf.memory = V4L2_MEMORY_USERPTR;
    out_buf.index = 0;
    // v4l2_buffer::m.userptr is a `unsigned long` holding a pointer; go through
    // uintptr_t so the cast states that intent in a fixed-width type.
    out_buf.m.userptr = (uintptr_t) this->capture_buffers_[cap_buf.index].start;
    out_buf.length = this->capture_buffers_[cap_buf.index].length;
    out_buf.bytesused = cap_buf.bytesused;

    if (ioctl(this->jpeg_fd_, VIDIOC_QBUF, &out_buf) < 0) {
      ESP_LOGW(TAG, "JPEG encoder OUTPUT QBUF failed: %s", strerror(errno));
      // esp_video reports EINVAL for several distinct conditions here without
      // saying which: an element it still considers busy, a memory-type or index
      // mismatch, a userptr not aligned to the queue's cache alignment, or a
      // userptr outside the memory class the queue was configured for. Dump the
      // buffer once per capture so a recurrence names the cause.
      if (!this->logged_qbuf_failure_) {
        this->logged_qbuf_failure_ = true;
        auto ptr = (uintptr_t) this->capture_buffers_[cap_buf.index].start;
        ESP_LOGW(TAG, "  buffer %u: ptr=0x%08X psram=%s align32=%u align64=%u len=%u used=%u", (unsigned) cap_buf.index,
                 (unsigned) ptr, esp_ptr_external_ram((const void *) ptr) ? "yes" : "NO", (unsigned) (ptr % 32),
                 (unsigned) (ptr % 64), (unsigned) this->capture_buffers_[cap_buf.index].length,
                 (unsigned) cap_buf.bytesused);
      }
      encoder_broken = true;
    } else {
      // Encoded frame first.
      struct v4l2_buffer jpeg_buf;
      memset(&jpeg_buf, 0, sizeof(jpeg_buf));
      jpeg_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      jpeg_buf.memory = V4L2_MEMORY_MMAP;
      if (ioctl(this->jpeg_fd_, VIDIOC_DQBUF, &jpeg_buf) < 0) {
        ESP_LOGW(TAG, "JPEG CAPTURE DQBUF failed: %s", strerror(errno));
        encoder_broken = true;
      } else {
        // Then the input buffer the encoder has finished reading.
        struct v4l2_buffer done_buf;
        memset(&done_buf, 0, sizeof(done_buf));
        done_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        done_buf.memory = V4L2_MEMORY_USERPTR;
        if (ioctl(this->jpeg_fd_, VIDIOC_DQBUF, &done_buf) < 0) {
          // esp_video refuses to queue an element it still holds
          // (esp_video_queue_element: !ELEMENT_IS_FREE -> EINVAL), and this path
          // always uses OUTPUT index 0, so a missed reclaim would wedge every
          // later frame. Let the reset free it.
          ESP_LOGW(TAG, "JPEG encoder OUTPUT DQBUF failed: %s", strerror(errno));
          encoder_broken = true;
        }

        if (jpeg_buf.bytesused > 0)
          this->deliver_frame_((const uint8_t *) this->jpeg_out_buffer_.start, jpeg_buf.bytesused);

        // Hand the encoder its output buffer back now that it has been copied.
        if (!encoder_broken) {
          memset(&jpeg_buf, 0, sizeof(jpeg_buf));
          jpeg_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          jpeg_buf.memory = V4L2_MEMORY_MMAP;
          jpeg_buf.index = 0;
          if (ioctl(this->jpeg_fd_, VIDIOC_QBUF, &jpeg_buf) < 0) {
            ESP_LOGW(TAG, "JPEG CAPTURE re-QBUF failed: %s", strerror(errno));
            encoder_broken = true;
          }
        }
      }
    }
  }

  // Return the raw frame to the sensor/ISP device.
  if (ioctl(this->capture_fd_, VIDIOC_QBUF, &cap_buf) < 0)
    ESP_LOGW(TAG, "capture QBUF failed: %s", strerror(errno));

  // Do this last: it may tear the capture down, invalidating capture_fd_.
  if (encoder_broken && !this->reset_jpeg_encoder_()) {
    ESP_LOGE(TAG, "JPEG encoder is unrecoverable; stopping capture");
    this->stop_capture_();
  }
}

bool ESPVideoCamera::reset_jpeg_encoder_() {
  int otype = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  int jtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  // STREAMOFF hands every queued buffer back to userspace, which is the only way
  // out once a QBUF/DQBUF has left one stuck inside the driver.
  ioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &otype);
  ioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &jtype);
  // STREAMOFF also un-queues the encoder's output buffer, so hand it back before
  // restarting, in the same capture-then-output order as the initial start.
  if (!this->queue_jpeg_capture_buffer_())
    return false;
  if (ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &jtype) < 0 || ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &otype) < 0) {
    ESP_LOGE(TAG, "JPEG encoder STREAMON failed on reset: %s", strerror(errno));
    return false;
  }
  ESP_LOGW(TAG, "JPEG encoder queues were reset after an error");
  return true;
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
  // Deliberately does not touch the pipeline. request_image(), start_stream() and
  // stop_stream() run in the caller's task -- the web server's httpd task, or an
  // API connection task -- while loop() runs in the main task, on the other core.
  // Opening the V4L2 devices, mmapping the buffers and running STREAMON from one
  // task while the other dequeues frames races on capture_fd_, jpeg_fd_ and
  // capture_buffers_: the main task can observe streaming_ before the fds and
  // mapped pointers it guards are visible, and then dequeue through garbage.
  // The requester bitmasks are atomic; loop() owns the pipeline and picks the
  // change up on its next iteration.
}

bool ESPVideoCamera::configure_capture_format_(uint32_t pixelformat) {
  uint32_t width = 0, height = 0;
  bool force_res = parse_resolution(this->resolution_, width, height);

  // A MIPI-CSI sensor cannot be resized through V4L2. esp_video's
  // common_video_set_format() rejects any width/height that differs from the
  // sensor's active format, and VIDIOC_ENUM_FRAMESIZES only ever reports that
  // single size, so the only effect of asking would be an EINVAL on every
  // start. The sensor's format is chosen when the firmware is built, from
  // CONFIG_CAMERA_<SENSOR>_MIPI_IF_FORMAT_INDEX_DEFAULT -- which is what
  // `resolution:` drives (see __init__.py). USB-UVC devices are the opposite:
  // they carry a real format list and honour a requested size at runtime.
  const bool device_can_resize = this->resolved_device_ != ESP_VIDEO_MIPI_CSI_DEVICE_NAME;

  struct v4l2_format fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(this->capture_fd_, VIDIOC_G_FMT, &fmt);  // best-effort starting point
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.pixelformat = pixelformat;
  if (force_res && device_can_resize) {
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
  }
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (ioctl(this->capture_fd_, VIDIOC_S_FMT, &fmt) < 0)
    ESP_LOGW(TAG, "VIDIOC_S_FMT (best-effort resolution) failed: %s", strerror(errno));

  // Read back the format actually negotiated by the sensor/ISP. A device that
  // cannot honour the request keeps its own format instead of failing loudly, so
  // never assume the request went through.
  uint32_t negotiated = pixelformat;  // assumed, if the device has no G_FMT
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->capture_fd_, VIDIOC_G_FMT, &fmt) == 0) {
    this->capture_width_ = fmt.fmt.pix.width;
    this->capture_height_ = fmt.fmt.pix.height;
    negotiated = fmt.fmt.pix.pixelformat;
  } else {
    this->capture_width_ = width;
    this->capture_height_ = height;
  }
  ESP_LOGI(TAG, "Capture resolution: %ux%u (%s)", (unsigned) this->capture_width_, (unsigned) this->capture_height_,
           fourcc_to_string(negotiated).c_str());

  // The pixel format is not negotiable: the direct path publishes the frames to
  // Home Assistant as JPEG, and the hardware-JPEG path feeds them to the M2M
  // encoder configured for RGB565. Streaming whatever the device fell back to
  // would produce a corrupt image rather than an error.
  // JPEG and MJPEG are the same payload for our purposes; UVC devices report
  // either one.
  const bool format_ok = (negotiated == pixelformat) ||
                         (pixelformat == V4L2_PIX_FMT_MJPEG && negotiated == V4L2_PIX_FMT_JPEG) ||
                         (pixelformat == V4L2_PIX_FMT_JPEG && negotiated == V4L2_PIX_FMT_MJPEG);
  if (!format_ok) {
    ESP_LOGE(TAG, "Device '%s' does not support pixel format %s (negotiated %s instead)",
             this->resolved_device_.c_str(), fourcc_to_string(pixelformat).c_str(),
             fourcc_to_string(negotiated).c_str());
    return false;
  }

  // A resolution the device cannot do is not fatal — the pipeline runs at the
  // negotiated size — but it is never what the user asked for, so say so.
  if (force_res && (this->capture_width_ != width || this->capture_height_ != height)) {
    if (device_can_resize) {
      ESP_LOGW(TAG, "Requested resolution %ux%u is not supported by '%s'; streaming %ux%u instead", (unsigned) width,
               (unsigned) height, this->resolved_device_.c_str(), (unsigned) this->capture_width_,
               (unsigned) this->capture_height_);
    } else {
      // The build asked the sensor driver for `resolution:` but a different
      // sensor answered on the bus, so it came up in its own default format.
      ESP_LOGW(TAG,
               "The sensor is streaming %ux%u, not the configured %ux%u. A MIPI sensor's resolution is fixed when "
               "the firmware is built, so this means the detected sensor is not the one named in 'sensor_model'.",
               (unsigned) this->capture_width_, (unsigned) this->capture_height_, (unsigned) width, (unsigned) height);
    }
  }
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
  this->idle_since_ms_ = 0;
  this->logged_qbuf_failure_ = false;
  return true;
}

bool ESPVideoCamera::start_direct_capture_() {
  this->capture_fd_ = open(this->resolved_device_.c_str(), O_RDWR | O_NONBLOCK);
  if (this->capture_fd_ < 0) {
    ESP_LOGE(TAG, "open(%s) failed: %s", this->resolved_device_.c_str(), strerror(errno));
    return false;
  }
  set_dqbuf_timeout(this->capture_fd_, CAPTURE_DQBUF_TIMEOUT_MS, "capture");
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
  set_dqbuf_timeout(this->capture_fd_, CAPTURE_DQBUF_TIMEOUT_MS, "capture");
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

  // ...but bound that wait. A 720p hardware encode takes single-digit
  // milliseconds, so this only fires when the encoder is wedged -- and the DQBUF
  // error path then resets its queues (see reset_jpeg_encoder_).
  set_dqbuf_timeout(this->jpeg_fd_, JPEG_DQBUF_TIMEOUT_MS, "JPEG encoder");

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
  // The encoder validates width/height on its CAPTURE queue too, not just on
  // OUTPUT: a zeroed v4l2_format is rejected with EINVAL ("pixel format or
  // width or height is invalid"). Both queues describe the same frame, so pass
  // the negotiated capture geometry here as well.
  fmt.fmt.pix.width = this->capture_width_;
  fmt.fmt.pix.height = this->capture_height_;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  if (ioctl(this->jpeg_fd_, VIDIOC_S_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "JPEG CAPTURE S_FMT failed: %s", strerror(errno));
    return false;
  }

  // Compression quality goes through VIDIOC_S_EXT_CTRLS, not VIDIOC_S_CTRL:
  // esp_video's ioctl dispatcher has no VIDIOC_S_CTRL case at all, so the plain
  // form fails and the encoder silently keeps its built-in default of 80. That
  // is why jpeg_quality had no measurable effect on the encoded size.
  // Form taken from esp_video's own examples (m2m and uvc).
  struct v4l2_ext_control ext_ctrl;
  memset(&ext_ctrl, 0, sizeof(ext_ctrl));
  ext_ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  ext_ctrl.value = this->jpeg_quality_;

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.ctrl_class = V4L2_CID_JPEG_CLASS;
  ext_ctrls.count = 1;
  ext_ctrls.controls = &ext_ctrl;
  if (ioctl(this->jpeg_fd_, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0)
    ESP_LOGW(TAG, "Could not set JPEG quality to %d: %s", this->jpeg_quality_, strerror(errno));

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

  // Give the encoder its output buffer and start the CAPTURE queue before the
  // OUTPUT one, matching esp_video's own M2M example (queue_all on capture,
  // then STREAMON capture, then STREAMON output). The encoder needs somewhere
  // to write before it is handed anything to encode.
  if (!this->queue_jpeg_capture_buffer_())
    return false;

  int jtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int otype = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if (ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &jtype) < 0 || ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &otype) < 0) {
    ESP_LOGE(TAG, "JPEG STREAMON failed: %s", strerror(errno));
    return false;
  }
  return true;
}

bool ESPVideoCamera::queue_jpeg_capture_buffer_() {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = 0;
  if (ioctl(this->jpeg_fd_, VIDIOC_QBUF, &buf) < 0) {
    ESP_LOGE(TAG, "JPEG CAPTURE QBUF failed: %s", strerror(errno));
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
  this->idle_since_ms_ = 0;
  // Any pending re-open is re-armed by handle_device_gone_() after this call;
  // a clean stop must not leave one behind.
  this->capture_retry_pending_ = false;
}

void ESPVideoCamera::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP-Video Camera:");
  ESP_LOGCONFIG(TAG, "  Name: %s", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Source: %s (%s)", this->device_.c_str(), this->resolved_device_.c_str());
  ESP_LOGCONFIG(TAG, "  Resolution: %s", this->resolution_.c_str());
  if (this->is_hw_jpeg_)
    ESP_LOGCONFIG(TAG, "  JPEG quality: %d", this->jpeg_quality_);
  ESP_LOGCONFIG(TAG, "  Max framerate: %.1f fps", this->max_framerate_);

  // Which sensor drivers esp_cam_sensor actually built in. esp_video_init()
  // probes exactly these over SCCB, so when auto-detection comes up empty this
  // line says whether the sensor you have is even represented in the firmware.
  std::string drivers;
#ifdef CONFIG_CAMERA_SC202CS
  drivers += " SC202CS(0x36)";
#endif
#ifdef CONFIG_CAMERA_OV5647
  drivers += " OV5647(0x36)";
#endif
#ifdef CONFIG_CAMERA_SC2336
  drivers += " SC2336(0x30)";
#endif
  ESP_LOGCONFIG(TAG, "  MIPI-CSI drivers:%s", drivers.empty() ? " none" : drivers.c_str());

  if (this->is_failed())
    ESP_LOGCONFIG(TAG, "  State: FAILED");
}

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4
