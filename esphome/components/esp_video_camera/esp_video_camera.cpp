#include "esp_video_camera.h"

// This component is ESP32-P4 silicon (MIPI-CSI, ISP, hardware JPEG) and builds
// only against esp_video's V4L2 headers, so it compiles on that variant alone.
#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/core/application.h"  // App.feed_wdt()
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

// Explicitly: an #if on an undefined CONFIG_ESP_VIDEO_* macro is silently false.
#include <sdkconfig.h>

#include "esp_idf_version.h"   // ESP_IDF_VERSION, for usb_host_config_t::peripheral_map
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
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "soc/soc_caps.h"       // SOC_LEDC_CHANNEL_NUM
#include "driver/i2c_master.h"  // i2c_master_bus_handle_t, which the CSI config carries with or without USE_I2C
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#endif
}

namespace esphome::esp_video_camera {

ESPHOME_LOG_TAG(TAG, "esp_video_camera");

// How long setup() waits for esp_video_init() on core 0 before giving up.
static constexpr uint32_t INIT_TIMEOUT_MS = 10000;
// How much of that wait may pass between two watchdog feeds.
static constexpr uint32_t INIT_POLL_SLICE_MS = 200;

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
// Settling time between the host port's 5 V rail coming up and the USB Host
// Library install; the same delay Espressif's board support code leaves.
static constexpr uint32_t VBUS_SETTLE_MS = 100;
#endif

// Frame buffers are rounded up to this many bytes so that consecutive frames of
// slightly different sizes reuse the same heap block (see deliver_frame_).
static constexpr size_t FRAME_ALLOC_GRANULARITY = 4096;

// Upper bound on how long a JPEG encode may hold the main loop; only a wedged
// encoder gets near it.
static constexpr uint32_t JPEG_DQBUF_TIMEOUT_MS = 100;

// The capture device is polled, never waited on, so the sensor's pace does not
// stall the main loop. esp_video treats a zero timeout as a plain queue poll.
static constexpr uint32_t CAPTURE_DQBUF_POLL_MS = 0;

// How long the pipeline keeps running after the last consumer went away. Every
// STREAMON rebuilds the ISP processor and re-converges AE/AWB.
static constexpr uint32_t CAPTURE_IDLE_TIMEOUT_MS = 5000;

// How often deliver_frame_ reports the resolution and frame rate.
static constexpr uint32_t STATS_INTERVAL_MS = 10000;

// Owns the configs esp_video_init() reads, on the heap so they outlive an
// attempt the component has stopped waiting on. Last party to let go frees it.
struct VideoInitContext {
  esp_video_init_csi_config_t csi_config{};
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  esp_video_init_usb_uvc_config_t uvc_config{};
#endif
  esp_video_init_config_t video_config{};
  SemaphoreHandle_t done{nullptr};
  esp_err_t result{ESP_FAIL};
  std::atomic<int> refs{2};

  ~VideoInitContext() {
    if (this->done != nullptr)
      vSemaphoreDelete(this->done);
  }
  void release() {
    if (this->refs.fetch_sub(1) == 1)
      delete this;
  }
};

namespace {

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
// Pump USB Host Library events when we installed it ourselves; when it is
// shared with another component, its existing owner pumps them.
void usb_host_lib_daemon_task(void *param) {
  bool complained = false;
  while (true) {
    uint32_t event_flags;
    const esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (err == ESP_OK) {
      complained = false;
      if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        usb_host_device_free_all();
      continue;
    }
    // Nothing enumerates while this is failing, so it must not be silent. Once
    // per run of failures, because a broken pump fails every time round.
    if (!complained) {
      complained = true;
      ESP_LOGE(TAG, "USB host event pump failed: %s; devices will not enumerate", esp_err_to_name(err));
    }
  }
}

// How many devices the USB Host Library has enumerated, or -1 if it is not
// running. Only used to explain why a UVC device did not show up.
int count_usb_devices() {
  uint8_t addr_list[16];
  int num_devices = 0;
  if (usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devices) != ESP_OK)
    return -1;
  return num_devices;
}
#else
int count_usb_devices() { return -1; }
#endif

// The top LEDC timer and channel: the `ledc` output component allocates from
// the bottom up, so a clash needs a board using every channel.
constexpr ledc_timer_t XCLK_LEDC_TIMER = (ledc_timer_t) (LEDC_TIMER_MAX - 1);
constexpr ledc_channel_t XCLK_LEDC_CHANNEL = (ledc_channel_t) (SOC_LEDC_CHANNEL_NUM - 1);

// esp_video_init() does not start XCLK, so a board without its own clock must
// generate it before init or the sensor stays silent on I2C.
esp_err_t init_xclk_ledc(gpio_num_t gpio_num, uint32_t freq_hz) {
  ledc_timer_config_t timer_conf = {};
  timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
  timer_conf.timer_num = XCLK_LEDC_TIMER;
  timer_conf.duty_resolution = LEDC_TIMER_1_BIT;
  timer_conf.freq_hz = freq_hz;
  timer_conf.clk_cfg = LEDC_AUTO_CLK;
  esp_err_t ret = ledc_timer_config(&timer_conf);
  if (ret != ESP_OK)
    return ret;

  ledc_channel_config_t ch_conf = {};
  ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
  ch_conf.channel = XCLK_LEDC_CHANNEL;
  ch_conf.timer_sel = XCLK_LEDC_TIMER;
  ch_conf.intr_type = LEDC_INTR_DISABLE;
  ch_conf.gpio_num = gpio_num;
  ch_conf.duty = 1;  // 50 % duty cycle
  ch_conf.hpoint = 0;
  return ledc_channel_config(&ch_conf);
}

// What a failed esp_video ioctl means. esp_video flattens its own error codes
// onto errno (esp_video_vfs.c), so strerror() alone is misleading.
const char *esp_video_errno_means(int err) {
  switch (err) {
    case EINVAL:
      return "a size, geometry or format the driver will not accept";
    case EBUSY:
      return "already streaming, or the last run left the hardware claimed";
    case ENOMEM:
      return "not enough memory for the buffers";
    case ESRCH:
      return "the driver does not implement this";
    case ENODEV:
      return "no free CSI controller or ISP processor";
    default:
      return "the driver failed without classifying why";
  }
}

// Render a V4L2 fourcc for logging, e.g. V4L2_PIX_FMT_MJPEG -> "MJPG".
const char *fourcc_to_string(uint32_t fourcc, char (&out)[5]) {
  for (int i = 0; i < 4; i++) {
    char c = (char) ((fourcc >> (8 * i)) & 0xFF);
    out[i] = (c >= 0x20 && c < 0x7F) ? c : '?';
  }
  out[4] = '\0';
  return out;
}

// The device is gone for good (a USB-UVC camera unplugged). EIO is excluded:
// it is also reported for transient frame errors.
bool errno_means_device_gone(int err) { return err == ENODEV || err == ENXIO; }

// A DQBUF that came back empty. esp_video reports a timed-out queue receive as
// EPERM, which on the polled capture device just means "no frame yet".
bool errno_means_no_frame(int err) { return err == EAGAIN || err == EPERM; }

// Bound how long VIDIOC_DQBUF may block. esp_video_vfs_open() ignores
// O_NONBLOCK and the default wait is portMAX_DELAY, so failing this is fatal.
bool set_dqbuf_timeout(int fd, uint32_t timeout_ms, const char *what) {
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) < 0) {
    ESP_LOGE(TAG, "Could not bound the %s DQBUF wait (%s); refusing to stream with an unbounded one", what,
             strerror(errno));
    return false;
  }
  return true;
}

}  // namespace

bool ESPVideoCameraImage::was_requested_by(camera::CameraRequester requester) const {
  return (this->requesters_ & (1 << requester)) != 0;
}

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

void ESPVideoCamera::setup() {
  if (!this->init_pipeline_()) {
    // A USB camera that has not enumerated fails esp_video_init(), which unwinds
    // cleanly, so retry: that is what makes plugging the camera in later work.
    if (this->is_uvc_()) {
      ESP_LOGW(TAG, "Camera pipeline is not up (no USB camera?); will retry every %u s",
               (unsigned) (PIPELINE_RETRY_INTERVAL_MS / 1000));
      this->pipeline_retry_at_ms_ = millis() + PIPELINE_RETRY_INTERVAL_MS;
      return;
    }
    // Nothing polls a still-running attempt once the component fails; the task
    // frees the context when esp_video_init() returns.
    if (this->init_ctx_ != nullptr) {
      this->init_ctx_->release();
      this->init_ctx_ = nullptr;
    }
    this->mark_failed(LOG_STR("Camera pipeline init failed"));
    return;
  }

  // Never probe a USB camera by opening it: that runs esp_video's
  // uvc_video_init(), which blocks for CONFIG_USB_UVC_INIT_TIMEOUT_MS, and
  // loop() retries on a timer regardless.
  if (this->is_uvc_()) {
    // None at all this early is normal; it stays at none when the host port
    // carries no data.
    ESP_LOGI(TAG, "Waiting for the USB camera on %s to enumerate (%d USB device(s) so far)", this->device_,
             count_usb_devices());
    this->arm_capture_retry_();
    return;
  }

  // The encoder is always present, but it is fed by /dev/video0, which only
  // exists once a sensor answered on the SCCB bus.
  const char *probe = this->is_hw_jpeg_() ? ESP_VIDEO_MIPI_CSI_DEVICE_NAME : this->device_;
  int probe_fd = open(probe, O_RDWR | O_NONBLOCK);
  if (probe_fd < 0) {
    if (this->is_hw_jpeg_()) {
      ESP_LOGE(TAG,
               "%s is unavailable. Check the sensor wiring and that it answers on the configured I2C bus; the "
               "drivers built into this firmware are listed in the config dump.",
               probe);
      this->mark_failed(LOG_STR("No MIPI-CSI sensor detected"));
    } else {
      ESP_LOGE(TAG, "V4L2 device '%s' unavailable (errno=%d: %s)", probe, errno, strerror(errno));
      this->mark_failed(LOG_STR("V4L2 device unavailable"));
    }
    return;
  }
  close(probe_fd);

  ESP_LOGI(TAG, "Camera ready on %s", this->device_);
}

bool ESPVideoCamera::start_pipeline_init_() {
  if (this->init_ctx_ != nullptr)
    return true;  // one is already under way
  // A USB camera is not on any I2C bus, so only the MIPI-CSI path needs one.
  const bool uvc_only = this->is_uvc_();
  i2c_master_bus_handle_t i2c_handle = nullptr;
  if (!uvc_only) {
#ifdef USE_I2C
    if (this->i2c_bus_ == nullptr) {
      ESP_LOGE(TAG, "No I2C bus set");
      return false;
    }
    const esp_err_t err = i2c_master_get_bus_handle((i2c_port_num_t) this->i2c_bus_->get_port(), &i2c_handle);
    if (err != ESP_OK || i2c_handle == nullptr) {
      ESP_LOGE(TAG, "No ESP-IDF I2C bus on port %d: %s", (int) this->i2c_bus_->get_port(), esp_err_to_name(err));
      return false;
    }
#else
    // Unreachable in practice: the i2c_id: check rules this out at config time.
    ESP_LOGE(TAG, "A MIPI-CSI sensor is probed over I2C, and this firmware was built without I2C");
    return false;
#endif  // USE_I2C
  }

  if (!uvc_only && this->xclk_pin_ >= 0) {
    const esp_err_t err = init_xclk_ledc((gpio_num_t) this->xclk_pin_, this->xclk_freq_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Could not generate the sensor clock on GPIO%d at %u Hz: %s", (int) this->xclk_pin_,
               (unsigned) this->xclk_freq_, esp_err_to_name(err));
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  if (uvc_only) {
    // A switched 5 V rail (usually an IO expander pin) comes up just before this
    // runs; let it settle, once, or the attached device never enumerates.
    if (!this->usb_host_installed_)
      delay(VBUS_SETTLE_MS);

    // The USB Host Library installs once per system, so own it here rather than
    // letting esp_video abort when another component already installed it.
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // Zero picks the High-Speed controller; a board wired to the Full-Speed one
    // enumerates nothing until this names it.
    host_config.peripheral_map = this->usb_peripheral_map_;
#endif
    // Only on the first attempt; a retry already has the library.
    if (!this->usb_host_installed_) {
      esp_err_t host_ret = usb_host_install(&host_config);
      if (host_ret == ESP_OK) {
        this->usb_host_installed_ = true;
        this->usb_host_owned_ = true;
        ESP_LOGI(TAG, "USB Host installed (peripheral map 0x%X)", (unsigned) this->usb_peripheral_map_);
      } else if (host_ret == ESP_ERR_INVALID_STATE) {
        // Whoever installed it owns the event pump too. If they are not draining
        // it, nothing will ever enumerate and this line is the only clue.
        this->usb_host_installed_ = true;
        ESP_LOGW(TAG, "USB Host already installed by another component; sharing it for UVC");
      } else {
        // Without the USB Host library the UVC device can never enumerate, so
        // there is nothing to gain from continuing into esp_video_init().
        ESP_LOGE(TAG, "usb_host_install() failed: %s", esp_err_to_name(host_ret));
        return false;
      }
    }
    // Tracked apart from the install so a pump that failed to start is retried.
    if (this->usb_host_owned_ && !this->usb_pump_started_) {
      // Priority 10, as in Espressif's board support code: it has to outrank
      // the work it feeds.
      if (xTaskCreatePinnedToCore(usb_host_lib_daemon_task, "usb_lib", 4096, nullptr, 10, nullptr, tskNO_AFFINITY) !=
          pdPASS) {
        ESP_LOGE(TAG, "Could not start the USB host event pump; no USB camera will enumerate");
        return false;
      }
      this->usb_pump_started_ = true;
    }
  }
#endif

  // Heap-allocated so the configs stay valid for the init task even if the wait
  // below times out (see VideoInitContext).
  auto *ctx = new VideoInitContext();
  ctx->done = xSemaphoreCreateBinary();
  if (ctx->done == nullptr) {
    delete ctx;
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
  if (uvc_only) {
    uvc_config.uvc.uvc_dev_num = 1;
    uvc_config.uvc.task_stack = 4096;
    uvc_config.uvc.task_priority = 5;
    uvc_config.uvc.task_affinity = -1;
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
    delete ctx;
    return false;
  }

  // With UVC, esp_video_init() itself waits up to CONFIG_USB_UVC_INIT_TIMEOUT_MS
  // for the camera to enumerate.
  uint32_t init_timeout_ms = INIT_TIMEOUT_MS;
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  if (this->is_uvc_())
    init_timeout_ms += CONFIG_USB_UVC_INIT_TIMEOUT_MS;
#endif
  this->init_ctx_ = ctx;
  this->init_deadline_ms_ = millis() + init_timeout_ms;
  this->init_overrun_logged_ = false;
  return true;
}

int ESPVideoCamera::poll_pipeline_init_(uint32_t wait_ms) {
  auto *ctx = this->init_ctx_;
  if (ctx == nullptr)
    return this->pipeline_ready_ ? 1 : 0;

  if (xSemaphoreTake(ctx->done, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
    const bool ok = ctx->result == ESP_OK;
    if (!ok) {
      ESP_LOGE(TAG, "esp_video_init() failed: %s", esp_err_to_name(ctx->result));
    }
    ctx->release();
    this->init_ctx_ = nullptr;
    this->pipeline_ready_ = ok;
    return ok ? 1 : 0;
  }

  // Worth saying, not acting on: starting another attempt over one that still
  // holds the USB stack would be worse than waiting.
  if (!this->init_overrun_logged_ && (int32_t) (millis() - this->init_deadline_ms_) >= 0) {
    this->init_overrun_logged_ = true;
    ESP_LOGE(TAG, "esp_video_init() has not returned yet; still waiting for it");
  }
  return -1;
}

bool ESPVideoCamera::init_pipeline_() {
  if (!this->start_pipeline_init_())
    return false;
  // In slices, feeding the watchdog between them: nothing else feeds it during
  // setup(), and the whole allowance is longer than the watchdog timeout.
  int state;
  do {
    // Before the slice, not after, so the settle delay and the USB host install
    // that start_pipeline_init_() just did are covered too.
    App.feed_wdt();
    state = this->poll_pipeline_init_(INIT_POLL_SLICE_MS);
  } while (state < 0 && (int32_t) (millis() - this->init_deadline_ms_) < 0);
  return state > 0;
}

void ESPVideoCamera::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  const bool wanted = this->requesters_() != 0;

  // Only reachable with UVC; esp_video_init() runs on its own task and is never
  // waited for here.
  if (!this->pipeline_ready_) {
    if (this->init_ctx_ != nullptr) {
      if (this->poll_pipeline_init_(0) < 0)
        return;  // still going; look again next iteration
      if (this->pipeline_ready_) {
        ESP_LOGI(TAG, "Camera ready on %s", this->device_);
      } else {
        this->pipeline_retry_at_ms_ = now + PIPELINE_RETRY_INTERVAL_MS;
      }
      return;
    }
    if (!wanted || (int32_t) (now - this->pipeline_retry_at_ms_) < 0)
      return;
    this->pipeline_retry_at_ms_ = now + PIPELINE_RETRY_INTERVAL_MS;
    // A failed start has logged why, and the retry above is armed either way.
    (void) this->start_pipeline_init_();
    return;
  }

  if (!this->streaming_) {
    if (!wanted)
      return;
    // Retry on a timer, both for a device that vanished and for a start that just
    // failed -- otherwise a sensor that cannot open is retried every iteration.
    if (this->capture_retry_pending_ && (int32_t) (now - this->capture_retry_at_ms_) < 0)
      return;
    this->capture_retry_pending_ = false;
    if (!this->start_capture_())
      this->arm_capture_retry_();
    return;
  }

  if (this->is_hw_jpeg_()) {
    this->loop_jpeg_pipeline_(now);
  } else {
    this->loop_direct_capture_(now);
  }

  // A source that accepted STREAMON but never delivers is otherwise silent,
  // since an empty queue looks like "no frame yet". Only checked while someone
  // is watching: deliver_frame_() counts nothing during the idle grace period.
  if (!wanted) {
    this->stats_since_ms_ = now;
  } else if (!this->warned_no_frames_ && this->stats_frames_ == 0 &&
             (now - this->stats_since_ms_) >= NO_FRAME_WARNING_MS) {
    this->warned_no_frames_ = true;
    ESP_LOGW(TAG,
             "Streaming from %s for %us without a single frame; restarting the capture. esp_video reports several "
             "unrelated failures as EPERM, which is indistinguishable from \"no frame yet\", so a device that has "
             "really gone can look like this.",
             this->device_, (unsigned) (NO_FRAME_WARNING_MS / 1000));
    this->stop_capture_();
    this->arm_capture_retry_();
    return;
  }

  // Keep the buffers in flight while idle (deliver_frame_() drops the frames),
  // and only tear the pipeline down once nobody has come back for a while.
  if (wanted) {
    this->idle_since_ms_ = 0;
  } else if (this->idle_since_ms_ == 0) {
    // Zero means "not idle", so a genuine zero is stored as 1.
    this->idle_since_ms_ = now == 0 ? 1 : now;
  } else if ((now - this->idle_since_ms_) >= CAPTURE_IDLE_TIMEOUT_MS) {
    this->stop_capture_();
  }
}

void ESPVideoCamera::arm_capture_retry_() {
  this->capture_retry_pending_ = true;
  this->capture_retry_at_ms_ = millis() + this->capture_retry_interval_ms_();
}

bool ESPVideoCamera::handle_device_gone_(int err) {
  if (!errno_means_device_gone(err))
    return false;
  ESP_LOGW(TAG, "Capture device '%s' disappeared (%s); will retry", this->device_, strerror(err));
  this->stop_capture_();
  this->arm_capture_retry_();
  return true;
}

bool ESPVideoCamera::dequeue_capture_(v4l2_buffer &buf) {
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->capture_fd_, VIDIOC_DQBUF, &buf) < 0) {
    const int err = errno;
    if (!errno_means_no_frame(err) && !this->handle_device_gone_(err)) {
      ESP_LOGW(TAG, "Capture DQBUF failed: %s", strerror(err));
    }
    return false;
  }
  // A buffer this component never mapped still has to go back to the driver;
  // say so once, or it looks like a camera that sends nothing.
  if (buf.index >= (uint32_t) this->num_capture_buffers_ && !this->warned_stray_buffer_) {
    this->warned_stray_buffer_ = true;
    ESP_LOGW(TAG, "The driver returned buffer %u; only %d are mapped", (unsigned) buf.index,
             this->num_capture_buffers_);
  }
  return true;
}

void ESPVideoCamera::deliver_frame_(const uint8_t *data, size_t length, uint32_t now) {
  if (length == 0)
    return;
  // Nobody is watching: the pipeline is only running out its idle grace period.
  auto requesters = this->requesters_();
  if (requesters == 0)
    return;
  this->last_frame_ms_ = now;

  // Round up so consecutive frames of slightly different sizes land in the same
  // heap size class instead of fragmenting it.
  size_t alloc_size = (length + FRAME_ALLOC_GRANULARITY - 1) & ~(size_t) (FRAME_ALLOC_GRANULARITY - 1);
  // PSRAM first, internal RAM as a fallback.
  auto copy = RAMAllocator<uint8_t>().make_unique_array_for_overwrite(alloc_size);
  if (copy == nullptr) {
    ESP_LOGW(TAG, "Failed to allocate %u bytes (frame dropped)", (unsigned) alloc_size);
    return;
  }
  memcpy(copy.get(), data, length);
  // Claim pending single-image requests only now that a frame exists for them;
  // exchange() leaves one that arrives mid-copy set for the next frame.
  requesters = (uint8_t) (this->single_requesters_.exchange(0) | this->stream_requesters_);
  this->current_image_ = std::make_shared<ESPVideoCameraImage>(std::move(copy), length, requesters);
  for (auto *listener : this->listeners_)
    listener->on_camera_image(this->current_image_);

  this->stats_frames_++;
  this->stats_bytes_ += length;
  uint32_t elapsed = now - this->stats_since_ms_;
  if (elapsed >= STATS_INTERVAL_MS) {
    ESP_LOGD(TAG, "%ux%u @ %.1f fps, %u B/frame", (unsigned) this->capture_width_, (unsigned) this->capture_height_,
             this->stats_frames_ * 1000.0f / elapsed, (unsigned) (this->stats_bytes_ / this->stats_frames_));
    this->stats_since_ms_ = now;
    this->stats_frames_ = 0;
    this->stats_bytes_ = 0;
  }
}

void ESPVideoCamera::loop_direct_capture_(uint32_t now) {
  // The device already delivers JPEG/MJPEG frames; one MMAP capture queue.
  struct v4l2_buffer buf;
  if (!this->dequeue_capture_(buf))
    return;
  if (buf.index < (uint32_t) this->num_capture_buffers_ && this->frame_due_(now))
    this->deliver_frame_((const uint8_t *) this->capture_buffers_[buf.index].start, buf.bytesused, now);
  if (ioctl(this->capture_fd_, VIDIOC_QBUF, &buf) < 0) {
    ESP_LOGW(TAG, "Capture QBUF failed: %s", strerror(errno));
  }
}

void ESPVideoCamera::loop_jpeg_pipeline_(uint32_t now) {
  // Dequeue one RGB565 frame from the sensor/ISP device (non-blocking).
  struct v4l2_buffer cap_buf;
  if (!this->dequeue_capture_(cap_buf))
    return;

  // Only encode frames that are going somewhere, or the encoder runs at the
  // sensor's rate whatever max_framerate says.
  const bool wanted = this->requesters_() != 0;
  const bool due = this->frame_due_(now);

  bool encoder_broken = false;
  if (wanted && due && cap_buf.index < (uint32_t) this->num_capture_buffers_ && cap_buf.bytesused > 0) {
    // Queue OUTPUT, dequeue CAPTURE, then reclaim OUTPUT (Espressif's
    // examples/m2m order); waiting on OUTPUT first deadlocks.
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
      // esp_video reports EINVAL for several distinct conditions without saying
      // which, so dump the buffer once per capture to name the cause.
      if (!this->logged_qbuf_failure_) {
        this->logged_qbuf_failure_ = true;
        // Keep the pointer as a pointer for esp_ptr_external_ram(); the integer
        // is only for printing its value and alignment.
        const void *start = this->capture_buffers_[cap_buf.index].start;
        auto ptr = (uintptr_t) start;
        ESP_LOGW(TAG, "  buffer %u: ptr=0x%08X psram=%s align32=%u align64=%u len=%u used=%u", (unsigned) cap_buf.index,
                 (unsigned) ptr, esp_ptr_external_ram(start) ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("NO"),
                 (unsigned) (ptr % 32), (unsigned) (ptr % 64), (unsigned) this->capture_buffers_[cap_buf.index].length,
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
          // esp_video refuses to queue an element it still holds, and this path always
          // uses OUTPUT index 0, so a missed reclaim would wedge every later frame.
          ESP_LOGW(TAG, "JPEG encoder OUTPUT DQBUF failed: %s", strerror(errno));
          encoder_broken = true;
        }

        if (jpeg_buf.bytesused > 0)
          this->deliver_frame_((const uint8_t *) this->jpeg_out_buffer_.start, jpeg_buf.bytesused, now);

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

  // Settle the encoder first: a broken one may still read this buffer through
  // its USERPTR while CSI DMA writes into it. The reset's STREAMOFF reclaims it.
  bool capture_alive = true;
  if (encoder_broken && !this->reset_jpeg_encoder_()) {
    ESP_LOGE(TAG, "JPEG encoder is unrecoverable; stopping capture");
    this->stop_capture_();
    capture_alive = false;
    // stop_capture_() clears any pending retry; without one loop() would rebuild
    // the whole pipeline on its very next iteration.
    this->arm_capture_retry_();
  }

  // Return the raw frame to the sensor/ISP device, unless the teardown above
  // already closed it.
  if (capture_alive && ioctl(this->capture_fd_, VIDIOC_QBUF, &cap_buf) < 0) {
    ESP_LOGW(TAG, "capture QBUF failed: %s", strerror(errno));
  }
}

bool ESPVideoCamera::reset_jpeg_encoder_() {
  // STREAMOFF is the only way to get back a buffer stuck in the driver. It also
  // un-queues the encoder's output buffer, so hand that back before restarting.
  this->set_jpeg_streaming_(false);
  if (!this->queue_jpeg_capture_buffer_())
    return false;
  if (!this->set_jpeg_streaming_(true)) {
    ESP_LOGE(TAG, "JPEG encoder STREAMON failed on reset: %s", strerror(errno));
    return false;
  }
  ESP_LOGW(TAG, "JPEG encoder queues were reset after an error");
  return true;
}

camera::CameraImageReader *ESPVideoCamera::create_image_reader() { return new ESPVideoCameraImageReader(); }

// These run in the caller's task and only touch the atomic requester masks;
// loop() owns the pipeline.
void ESPVideoCamera::request_image(camera::CameraRequester requester) { this->single_requesters_ |= (1U << requester); }

void ESPVideoCamera::start_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_)
    listener->on_stream_start();
  this->stream_requesters_ |= (1U << requester);
}

void ESPVideoCamera::stop_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_)
    listener->on_stream_stop();
  this->stream_requesters_ &= ~(1U << requester);
}

bool ESPVideoCamera::configure_capture_format_(uint32_t pixelformat) {
  const uint32_t width = this->width_;
  const uint32_t height = this->height_;
  const bool force_res = width != 0;

  // A MIPI-CSI sensor's size is a build-time Kconfig choice and V4L2 rejects any
  // other; USB-UVC devices resize at runtime. On the hardware-JPEG path
  // capture_fd_ is the MIPI-CSI node, not device_.
  const bool device_can_resize = !this->is_hw_jpeg_() && strcmp(this->device_, ESP_VIDEO_MIPI_CSI_DEVICE_NAME) != 0;

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
  if (ioctl(this->capture_fd_, VIDIOC_S_FMT, &fmt) < 0) {
    // A MIPI sensor keeps streaming its own size, so only warn. On a USB camera
    // S_FMT picks the stream format, and STREAMON fails without it.
    if (this->is_uvc_()) {
      ESP_LOGE(TAG, "VIDIOC_S_FMT failed on the USB camera (%s); it never agreed a stream format", strerror(errno));
      return false;
    }
    ESP_LOGW(TAG, "VIDIOC_S_FMT (best-effort resolution) failed: %s", strerror(errno));
  }

  // A device that cannot honour the request keeps its own format silently.
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
  char negotiated_str[5];
  ESP_LOGI(TAG, "Capture resolution: %ux%u (%s)", (unsigned) this->capture_width_, (unsigned) this->capture_height_,
           fourcc_to_string(negotiated, negotiated_str));

  // The pixel format is not negotiable: a fallback would be streamed as a corrupt
  // image rather than reported. JPEG and MJPEG are the same payload here.
  const bool format_ok = (negotiated == pixelformat) ||
                         (pixelformat == V4L2_PIX_FMT_MJPEG && negotiated == V4L2_PIX_FMT_JPEG) ||
                         (pixelformat == V4L2_PIX_FMT_JPEG && negotiated == V4L2_PIX_FMT_MJPEG);
  if (!format_ok) {
    char wanted_str[5];
    ESP_LOGE(TAG, "Device '%s' does not support pixel format %s (negotiated %s instead)", this->device_,
             fourcc_to_string(pixelformat, wanted_str), negotiated_str);
    return false;
  }

  // A resolution the device cannot do is not fatal — the pipeline runs at the
  // negotiated size — but it is never what the user asked for, so say so.
  if (force_res && (this->capture_width_ != width || this->capture_height_ != height)) {
    if (device_can_resize) {
      ESP_LOGW(TAG, "Requested resolution %ux%u is not supported by '%s'; streaming %ux%u instead", (unsigned) width,
               (unsigned) height, this->device_, (unsigned) this->capture_width_, (unsigned) this->capture_height_);
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
  // REQBUFS may grant fewer buffers than asked for, including none, without
  // reporting an error.
  if (this->num_capture_buffers_ == 0) {
    ESP_LOGE(TAG, "The device granted no capture buffers; there is nothing to stream into");
    return false;
  }
  return true;
}

bool ESPVideoCamera::start_capture_() {
  if (this->streaming_)
    return true;
  if (this->is_failed())
    return false;

  bool ok = this->is_hw_jpeg_() ? this->start_jpeg_pipeline_() : this->start_direct_capture_();
  if (!ok) {
    this->stop_capture_();
    return false;
  }
  this->streaming_ = true;
  this->last_frame_ms_ = 0;
  this->idle_since_ms_ = 0;
  this->stats_since_ms_ = millis();  // the open above can block
  this->stats_frames_ = 0;
  this->stats_bytes_ = 0;
  this->logged_qbuf_failure_ = false;
  this->warned_stray_buffer_ = false;
  this->warned_no_frames_ = false;
  return true;
}

bool ESPVideoCamera::open_capture_(const char *path, uint32_t pixelformat) {
  this->capture_fd_ = open(path, O_RDWR | O_NONBLOCK);
  if (this->capture_fd_ < 0) {
    // Not an error for a USB camera that has not enumerated yet. No USB devices
    // at all points at power or cabling; one that is there offers no UVC stream.
    if (this->is_uvc_()) {
      ESP_LOGW(TAG, "No USB camera on %s yet (%s); %d USB device(s) on the bus", path, strerror(errno),
               count_usb_devices());
    } else {
      ESP_LOGE(TAG, "open(%s) failed: %s", path, strerror(errno));
    }
    return false;
  }
  return set_dqbuf_timeout(this->capture_fd_, CAPTURE_DQBUF_POLL_MS, "capture") &&
         this->configure_capture_format_(pixelformat) && this->setup_capture_buffers_();
}

bool ESPVideoCamera::start_direct_capture_() {
  // JPEG, not MJPEG: esp_video's UVC driver maps a camera's MJPEG stream onto
  // V4L2_PIX_FMT_JPEG and rejects V4L2_PIX_FMT_MJPEG outright.
  if (!this->open_capture_(this->device_, V4L2_PIX_FMT_JPEG))
    return false;
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->capture_fd_, VIDIOC_STREAMON, &type) < 0) {
    ESP_LOGE(TAG, "Could not start '%s' at %ux%u: %s (%s)", this->device_, (unsigned) this->capture_width_,
             (unsigned) this->capture_height_, strerror(errno), esp_video_errno_means(errno));
    return false;
  }
  return true;
}

bool ESPVideoCamera::start_jpeg_pipeline_() {
  // Stage 1: sensor/ISP capture device producing RGB565 frames.
  if (!this->open_capture_(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, V4L2_PIX_FMT_RGB565))
    return false;
  int ctype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->capture_fd_, VIDIOC_STREAMON, &ctype) < 0) {
    // The CSI and ISP drivers log the check that failed just before this.
    ESP_LOGE(TAG, "Could not start the sensor at %ux%u: %s (%s). The csi/ISP error just above says which check failed.",
             (unsigned) this->capture_width_, (unsigned) this->capture_height_, strerror(errno),
             esp_video_errno_means(errno));
    return false;
  }

  // Stage 2: JPEG hardware encoder (M2M). Blocking so the per-frame DQBUFs wait
  // for the (fast) hardware encode instead of busy-looping on EAGAIN.
  this->jpeg_fd_ = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
  if (this->jpeg_fd_ < 0) {
    ESP_LOGE(TAG, "open(%s) failed: %s", ESP_VIDEO_JPEG_DEVICE_NAME, strerror(errno));
    return false;
  }

  // ...but bound that wait; only a wedged encoder hits it, and the DQBUF error
  // path then resets its queues.
  if (!set_dqbuf_timeout(this->jpeg_fd_, JPEG_DQBUF_TIMEOUT_MS, "JPEG encoder"))
    return false;

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
  // The encoder validates width/height on CAPTURE too: a zeroed v4l2_format is
  // rejected with EINVAL.
  fmt.fmt.pix.width = this->capture_width_;
  fmt.fmt.pix.height = this->capture_height_;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  if (ioctl(this->jpeg_fd_, VIDIOC_S_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "JPEG CAPTURE S_FMT failed: %s", strerror(errno));
    return false;
  }

  // Quality goes through VIDIOC_S_EXT_CTRLS: esp_video's dispatcher has no
  // VIDIOC_S_CTRL case at all, so the plain form silently keeps the default 80.
  struct v4l2_ext_control ext_ctrl;
  memset(&ext_ctrl, 0, sizeof(ext_ctrl));
  ext_ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  ext_ctrl.value = this->jpeg_quality_;

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.ctrl_class = V4L2_CID_JPEG_CLASS;
  ext_ctrls.count = 1;
  ext_ctrls.controls = &ext_ctrl;
  if (ioctl(this->jpeg_fd_, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
    ESP_LOGW(TAG, "Could not set JPEG quality to %d: %s", this->jpeg_quality_, strerror(errno));
  }

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

  // CAPTURE queue before OUTPUT, as in esp_video's M2M example: the encoder
  // needs somewhere to write before it is handed anything to encode.
  if (!this->queue_jpeg_capture_buffer_())
    return false;

  if (!this->set_jpeg_streaming_(true)) {
    ESP_LOGE(TAG, "JPEG STREAMON failed: %s (%s)", strerror(errno), esp_video_errno_means(errno));
    return false;
  }
  return true;
}

bool ESPVideoCamera::set_jpeg_streaming_(bool on) {
  int otype = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  int jtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!on) {
    ioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &otype);
    ioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &jtype);
    return true;
  }
  // CAPTURE before OUTPUT: the encoder needs somewhere to write first.
  return ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &jtype) >= 0 && ioctl(this->jpeg_fd_, VIDIOC_STREAMON, &otype) >= 0;
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
    this->set_jpeg_streaming_(false);
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
  // A pending snapshot left set would keep the pipeline wanted for ever; the
  // requester asks again if it still wants one.
  this->single_requesters_ = 0;
  // Free the last frame's PSRAM; listeners hold their own shared_ptr.
  this->current_image_.reset();
  // Callers that want a retry arm it after this.
  this->capture_retry_pending_ = false;
}

void ESPVideoCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP-Video Camera:\n"
                "  Name: %s\n"
                "  Device: %s\n"
                "  Max framerate: %.1f fps",
                LOG_STR_ARG(this->get_log_name()), this->device_, 1000.0f / this->min_interval_ms_);
  if (this->width_ != 0) {
    ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGCONFIG(TAG, "  Resolution: auto");
  }
  if (this->is_hw_jpeg_()) {
    ESP_LOGCONFIG(TAG, "  JPEG quality: %d", this->jpeg_quality_);
  }

  // Neither of the next two means anything for a USB camera: it has no sensor
  // clock to generate and is not probed over SCCB.
  if (!this->is_uvc_()) {
    if (this->xclk_pin_ >= 0) {
      ESP_LOGCONFIG(TAG, "  XCLK: GPIO%d at %u Hz", this->xclk_pin_, (unsigned) this->xclk_freq_);
    } else {
      ESP_LOGCONFIG(TAG, "  XCLK: left to the board");
    }
    // The drivers esp_video_init() probes over SCCB, to tell whether the
    // sensor on the board is in the firmware at all.
    ESP_LOGCONFIG(TAG, "  MIPI-CSI drivers:");
#ifdef CONFIG_CAMERA_SC202CS
    ESP_LOGCONFIG(TAG, "    SC202CS (0x36)");
#endif
#ifdef CONFIG_CAMERA_OV5647
    ESP_LOGCONFIG(TAG, "    OV5647 (0x36)");
#endif
#ifdef CONFIG_CAMERA_SC2336
    ESP_LOGCONFIG(TAG, "    SC2336 (0x30)");
#endif
  }
}

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4
