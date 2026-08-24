#include "esp_video_camera.h"

// This component is ESP32-P4 silicon (MIPI-CSI, ISP, hardware JPEG) and builds
// only against esp_video's V4L2 headers, so it compiles on that variant alone.
#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "i2c_helper.h"
#include "esphome/core/application.h"  // App.feed_wdt()
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

// Explicitly, not by luck: every CONFIG_ESP_VIDEO_* test below decides whether
// a whole block of code exists, and an #if on an undefined macro is false
// without a word of complaint.
#include <sdkconfig.h>

#include "esp_heap_caps.h"
#include "esp_idf_version.h"   // ESP_IDF_VERSION, for usb_host_config_t::peripheral_map
#include "esp_memory_utils.h"  // esp_ptr_external_ram()

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
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

static const char *const TAG = "esp_video_camera";

// How long setup() waits for esp_video_init() on core 0 before giving up.
static constexpr uint32_t INIT_TIMEOUT_MS = 10000;
// How much of that wait may pass between two watchdog feeds. Comfortably under
// any sane CONFIG_ESP_TASK_WDT_TIMEOUT_S (5 s by default in ESPHome), and long
// enough that the polling costs nothing next to the init it is waiting on.
static constexpr uint32_t INIT_POLL_SLICE_MS = 200;

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
// Settling time between the host port's 5 V rail coming up and the USB Host
// Library being installed. Matches the delay Espressif's own board support code
// leaves for the port's inrush. Only the USB path has a rail to wait on, so a
// build without it must not carry this at all.
static constexpr uint32_t VBUS_SETTLE_MS = 100;
#endif

// Frame buffers are rounded up to this many bytes so that consecutive frames of
// slightly different sizes reuse the same heap block (see deliver_frame_).
static constexpr size_t FRAME_ALLOC_GRANULARITY = 4096;

// Upper bound on how long a JPEG encode may hold the main loop (see
// start_jpeg_pipeline_). Generous next to a real 720p encode, small enough that
// a wedged encoder cannot trip the task watchdog.
static constexpr uint32_t JPEG_DQBUF_TIMEOUT_MS = 100;

// The capture device is polled, never waited on: loop() runs in the main task,
// and the sensor's pace would otherwise cost every other component a full
// frame period. esp_video treats a zero timeout as a plain queue poll.
static constexpr uint32_t CAPTURE_DQBUF_POLL_MS = 0;

// How long the pipeline keeps running after the last consumer went away.
// Every STREAMOFF on the MIPI-CSI device destroys the ISP processor and every
// STREAMON builds a new one and re-converges AE/AWB, so idling briefly avoids
// cycling all of that on a browser reconnect or a burst of snapshots.
static constexpr uint32_t CAPTURE_IDLE_TIMEOUT_MS = 5000;

// How often deliver_frame_ reports the resolution and frame rate it is running
// at. This is the component's only recurring log line.
static constexpr uint32_t STATS_INTERVAL_MS = 10000;

// ===========================================================================
// Pipeline init helpers (run esp_video_init on core 0, optional LEDC XCLK)
// ===========================================================================
// Owns the configs esp_video_init() reads, on the heap so they outlive an
// attempt the component has stopped waiting on. Last party to let go destroys
// it. Declared in the header, which is why this is not in the anonymous
// namespace below.
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
  while (true) {
    uint32_t event_flags;
    if (usb_host_lib_handle_events(portMAX_DELAY, &event_flags) == ESP_OK) {
      if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        usb_host_device_free_all();
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

// Which LEDC timer and channel the sensor clock runs on: the top of the range.
// ESPHome's `ledc` output component hands out channels from the bottom up and
// derives its timer from the channel, so claiming channel 0 took the one it
// gives the first light in the configuration. The two components share no
// allocator and are not worth coupling over a clock pin, so start at the far
// end, where a clash needs a board using every channel LEDC has.
constexpr ledc_timer_t XCLK_LEDC_TIMER = (ledc_timer_t) (LEDC_TIMER_MAX - 1);
constexpr ledc_channel_t XCLK_LEDC_CHANNEL = (ledc_channel_t) (SOC_LEDC_CHANNEL_NUM - 1);

// Generate the sensor XCLK with LEDC. For MIPI-CSI sensors esp_video_init() does
// not start XCLK, so non-M5Stack boards must do it before init or the sensor
// stays silent on I2C.
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

// Parse "WIDTHxHEIGHT", the only form the Python schema emits. False for "auto".
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

// The device is gone for good (a USB-UVC camera unplugged). EIO is excluded:
// it is also reported for transient frame errors.
bool errno_means_device_gone(int err) { return err == ENODEV || err == ENXIO; }

// A DQBUF that came back empty rather than broken. esp_video maps a timed-out
// queue receive to ESP_FAIL, which esp_err_to_errno() turns into EPERM; on the
// polled capture device that is simply "no frame yet".
bool errno_means_no_frame(int err) { return err == EAGAIN || err == EPERM; }

// Bound how long VIDIOC_DQBUF may block. O_NONBLOCK does nothing here:
// esp_video_vfs_open() ignores its flags and the default wait is portMAX_DELAY,
// which in the main loop means the task watchdog fires.
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
  // Raising esp32 -> framework -> advanced -> log_level to DEBUG overflows
  // esp_video's 4 KB ISP task stack on its per-frame stats dump. Nothing here
  // can stop that: ESPHome builds with CONFIG_LOG_TAG_LEVEL_IMPL_NONE, so
  // esp_log_level_set() is compiled out and every tag follows the one level.

  // Resolve the device alias to a concrete /dev/videoN path. This runs before
  // init_pipeline_() because is_uvc_device_() reads the resolved path, and the
  // pipeline is built differently for a USB camera.
  const std::string &d = this->device_;
  this->is_hw_jpeg_ = false;
  if (d.empty() || d == "jpeg" || d == ESP_VIDEO_JPEG_DEVICE_NAME) {
    this->resolved_device_ = ESP_VIDEO_JPEG_DEVICE_NAME;  // /dev/video10
    this->is_hw_jpeg_ = true;
  } else if (d.starts_with("uvc")) {
    // "uvc" -> /dev/video40, "uvcN" -> /dev/video4N (N validated as a digit).
    const char *index = (d.size() == 4) ? (d.c_str() + 3) : "0";
    this->resolved_device_ = std::string(ESP_VIDEO_USB_UVC_NAME_PREFIX) + index;
  } else {
    this->resolved_device_ = d;
  }

  if (!this->init_pipeline_()) {
    // A USB camera that has not enumerated makes esp_video_init() fail as a
    // whole, and it unwinds everything it had created, so calling it again
    // later starts from a clean slate. Retry instead of failing the component
    // for good: that is also what makes plugging the camera in after boot work.
    if (this->enable_uvc_) {
      ESP_LOGW(TAG, "Camera pipeline is not up (no USB camera?); will retry every %u s",
               (unsigned) (PIPELINE_RETRY_INTERVAL_MS / 1000));
      this->pipeline_retry_at_ms_ = millis() + PIPELINE_RETRY_INTERVAL_MS;
      return;
    }
    // Failing the component stops loop(), so nothing will ever poll an attempt
    // that is still running. Let go of it here; the task frees the context when
    // esp_video_init() eventually returns.
    if (this->init_ctx_ != nullptr) {
      this->init_ctx_->release();
      this->init_ctx_ = nullptr;
    }
    this->mark_failed();
    return;
  }

  // The encoder is always present, but it is fed by /dev/video0, which only
  // exists once a sensor answered on the SCCB bus. Without this check the
  // component reports itself ready and then never delivers a frame.
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
    // A USB camera enumerates on its own schedule and is routinely still absent
    // here, a second or two after esp_video_init(). Failing the component would
    // make that permanent, so leave it to loop(), which already retries a
    // missing device on a timer. A MIPI sensor is either detected by now or
    // never will be.
    if (this->is_uvc_device_()) {
      // The device count is the useful half of this: none at all this early is
      // normal, but it stays at none when the host port is not carrying data.
      ESP_LOGI(TAG, "%s is not there yet; waiting for the USB camera to enumerate (%d USB device(s) so far)",
               this->resolved_device_.c_str(), count_usb_devices());
      this->capture_retry_pending_ = true;
      this->capture_retry_at_ms_ = millis() + this->capture_retry_interval_ms_();
      return;
    }
    ESP_LOGE(TAG, "V4L2 device '%s' unavailable (errno=%d: %s)", this->resolved_device_.c_str(), errno,
             strerror(errno));
    this->mark_failed();
    return;
  }
  close(test_fd);

  ESP_LOGI(TAG, "Camera ready on %s (source: %s)", this->resolved_device_.c_str(), this->device_.c_str());
}

bool ESPVideoCamera::is_uvc_device_() const {
  return this->device_.starts_with("uvc") || this->resolved_device_.starts_with(ESP_VIDEO_USB_UVC_NAME_PREFIX);
}

bool ESPVideoCamera::start_pipeline_init_() {
  if (this->init_ctx_ != nullptr)
    return true;  // one is already under way
  // A USB camera is not on any I2C bus, so only the MIPI-CSI path needs one.
  const bool uvc_only = this->is_uvc_device_();
  i2c_master_bus_handle_t i2c_handle = nullptr;
  if (!uvc_only) {
#ifdef USE_I2C
    if (this->i2c_bus_ == nullptr) {
      ESP_LOGE(TAG, "No I2C bus set");
      return false;
    }
    i2c_handle = get_i2c_bus_handle(this->i2c_bus_);
    if (i2c_handle == nullptr) {
      ESP_LOGE(TAG, "Could not obtain the ESP-IDF I2C bus handle");
      return false;
    }
#else
    // Unreachable in practice: the i2c_id: check rules this out at config time.
    ESP_LOGE(TAG, "A MIPI-CSI sensor is probed over I2C, and this firmware was built without I2C");
    return false;
#endif  // USE_I2C
  }

  // esp_video_init() only probes for a MIPI sensor when config->csi is set, so
  // leave it NULL for a USB-only board.
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

    // On boards where the host port's 5 V is switched -- an IO expander pin on
    // most of them -- that switch is a component of its own, and ESPHome runs
    // its setup() only microseconds before this one. Let the rail settle before
    // the host library starts driving bus resets, or the device attached at
    // boot is reset while its own supply is still ramping and never enumerates.
    // Espressif's board support code and the M5Stack Tab5 USB host example both
    // wait here for the same reason.
    // Once: a retry runs with the rail already up, and has nothing to wait for.
    if (!this->usb_host_started_)
      delay(VBUS_SETTLE_MS);

    // The USB Host Library installs once per system, so own it here rather than
    // letting esp_video abort when another component already installed it.
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // The ESP32-P4 has two USB controllers and a board wires its host connector
    // to one of them. Zero means "the default", which on a High-Speed capable
    // target is the High-Speed one -- so a board whose host port hangs off the
    // Full-Speed controller enumerates nothing until this names it instead.
    host_config.peripheral_map = this->usb_peripheral_map_;
#endif
    // Only on the first attempt: a retry already has the library and the daemon
    // task this component started, and asking again would earn a warning about
    // "another component" that would in fact be this one.
    if (!this->usb_host_started_) {
      esp_err_t host_ret = usb_host_install(&host_config);
      if (host_ret == ESP_OK) {
        // Priority 10, the same as Espressif's own board support code: nothing
        // enumerates unless this task keeps draining the library's events, so it
        // has to outrank the work it is feeding.
        xTaskCreatePinnedToCore(usb_host_lib_daemon_task, "usb_lib", 4096, nullptr, 10, nullptr, tskNO_AFFINITY);
        ESP_LOGI(TAG, "USB Host installed (peripheral map 0x%X)", (unsigned) this->usb_peripheral_map_);
      } else if (host_ret == ESP_ERR_INVALID_STATE) {
        // Whoever installed it owns the event pump too. If they are not draining
        // it, nothing will ever enumerate and this line is the only clue.
        ESP_LOGW(TAG, "USB Host already installed by another component; sharing it for UVC");
      } else {
        // Without the USB Host library the UVC device can never enumerate, so
        // there is nothing to gain from continuing into esp_video_init().
        ESP_LOGE(TAG, "usb_host_install() failed: %s", esp_err_to_name(host_ret));
        ctx->refs.store(1);  // no task was started
        ctx->release();
        return false;
      }
      this->usb_host_started_ = true;
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

  // With UVC enabled, esp_video_init() spends up to CONFIG_USB_UVC_INIT_TIMEOUT_MS
  // (10 s by default) waiting for the USB camera to enumerate before it returns.
  // Allowing only INIT_TIMEOUT_MS would expire in the same instant and fail the
  // component for a camera that was about to come up.
  uint32_t init_timeout_ms = INIT_TIMEOUT_MS;
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
  if (this->enable_uvc_)
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
    if (!ok)
      ESP_LOGE(TAG, "esp_video_init() failed: %s", esp_err_to_name(ctx->result));
    ctx->release();
    this->init_ctx_ = nullptr;
    this->pipeline_ready_ = ok;
    return ok ? 1 : 0;
  }

  // Overrunning the deadline is worth saying, but not worth acting on: the task
  // still holds the USB stack, and the only thing that could be done about it --
  // starting another -- would be worse than waiting. It is still running.
  if (!this->init_overrun_logged_ && (int32_t) (millis() - this->init_deadline_ms_) >= 0) {
    this->init_overrun_logged_ = true;
    ESP_LOGE(TAG, "esp_video_init() has not returned yet; still waiting for it");
  }
  return -1;
}

bool ESPVideoCamera::init_pipeline_() {
  if (!this->start_pipeline_init_())
    return false;
  // In slices, feeding the watchdog between them. This runs from setup(), and
  // the loop task is subscribed to the task watchdog with panic on: nothing
  // feeds it between one component's setup() and the next. Waiting out the
  // whole allowance in one call would reboot the device at the watchdog
  // timeout -- and reboot it on exactly the slow init this code was written to
  // survive, so the overrun message and the UVC retry below could never run.
  int state;
  do {
    // Before the slice, not after, so the settle delay and the USB host install
    // that start_pipeline_init_() just did are covered too.
    App.feed_wdt();
    state = this->poll_pipeline_init_(INIT_POLL_SLICE_MS);
  } while (state < 0 && (int32_t) (millis() - this->init_deadline_ms_) < 0);
  return state > 0;
}

// ===========================================================================
// ESPVideoCamera — streaming / capture
// ===========================================================================
void ESPVideoCamera::loop() {
  const bool wanted = (this->stream_requesters_ != 0) || (this->single_requesters_ != 0);

  // Only reachable with UVC: setup() fails the component outright otherwise.
  // Retried on demand and on a long timer, and never waited for here:
  // esp_video_init() runs on its own task and spends tens of seconds inside the
  // USB stack when no camera is plugged in, which is not time the main loop has
  // to give.
  if (!this->pipeline_ready_) {
    if (this->init_ctx_ != nullptr) {
      if (this->poll_pipeline_init_(0) < 0)
        return;  // still going; look again next iteration
      if (this->pipeline_ready_) {
        ESP_LOGI(TAG, "Camera ready on %s (source: %s)", this->resolved_device_.c_str(), this->device_.c_str());
      } else {
        this->pipeline_retry_at_ms_ = millis() + PIPELINE_RETRY_INTERVAL_MS;
      }
      return;
    }
    if (!wanted || (int32_t) (millis() - this->pipeline_retry_at_ms_) < 0)
      return;
    this->pipeline_retry_at_ms_ = millis() + PIPELINE_RETRY_INTERVAL_MS;
    this->start_pipeline_init_();
    return;
  }

  if (!this->streaming_) {
    if (!wanted)
      return;
    // Retry on a timer, both for a device that vanished and for a start that just
    // failed -- otherwise a sensor that cannot open is retried every iteration.
    if (this->capture_retry_pending_ && (int32_t) (millis() - this->capture_retry_at_ms_) < 0)
      return;
    this->capture_retry_pending_ = false;
    if (!this->start_capture_()) {
      this->capture_retry_pending_ = true;
      this->capture_retry_at_ms_ = millis() + this->capture_retry_interval_ms_();
    }
    return;
  }

  if (this->is_hw_jpeg_) {
    this->loop_jpeg_pipeline_();
  } else {
    this->loop_direct_capture_();
  }

  // STREAMON succeeding only means the source accepted the request, not that it
  // is sending anything. An empty queue is indistinguishable from "no frame
  // yet" one poll at a time, so a source that never delivers is otherwise
  // completely silent -- the only symptom is a consumer timing out somewhere
  // else entirely.
  //
  // Only while someone is watching: deliver_frame_() counts nothing during the
  // idle grace period, so this warned about a perfectly healthy camera every
  // time the last viewer went away.
  if (!wanted) {
    this->stats_since_ms_ = millis();
  } else if (!this->warned_no_frames_ && this->stats_frames_ == 0 &&
             (millis() - this->stats_since_ms_) >= NO_FRAME_WARNING_MS) {
    this->warned_no_frames_ = true;
    ESP_LOGW(TAG, "Streaming from %s for %us without a single frame; the source accepted the format but sends nothing",
             this->resolved_device_.c_str(), (unsigned) (NO_FRAME_WARNING_MS / 1000));
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
  this->capture_retry_at_ms_ = millis() + this->capture_retry_interval_ms_();
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

  // Round up so consecutive frames of slightly different sizes land in the same
  // heap size class instead of fragmenting it.
  size_t alloc_size = (length + FRAME_ALLOC_GRANULARITY - 1) & ~(size_t) (FRAME_ALLOC_GRANULARITY - 1);
  uint8_t *copy = (uint8_t *) heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (copy == nullptr)
    copy = (uint8_t *) heap_caps_malloc(alloc_size, MALLOC_CAP_8BIT);
  if (copy == nullptr) {
    ESP_LOGW(TAG, "Failed to allocate %u bytes (frame dropped)", (unsigned) alloc_size);
    return;
  }
  memcpy(copy, data, length);
  // Claim pending single-image requests only now that a frame exists for them;
  // exchange() leaves one that arrives mid-copy set for the next frame.
  requesters = (uint8_t) (this->single_requesters_.exchange(0) | this->stream_requesters_);
  this->current_image_ = std::make_shared<ESPVideoCameraImage>(copy, length, requesters);
  for (auto *listener : this->listeners_)
    listener->on_camera_image(this->current_image_);

  // Throughput, on an interval. Logging a line per frame from here would cost
  // more than the capture it describes, and the numbers only mean anything
  // averaged anyway.
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

void ESPVideoCamera::loop_direct_capture_() {
  // The device already delivers JPEG/MJPEG frames; one MMAP capture queue.
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->capture_fd_, VIDIOC_DQBUF, &buf) < 0) {
    int err = errno;
    if (!errno_means_no_frame(err) && !this->handle_device_gone_(err))
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
    if (!errno_means_no_frame(err) && !this->handle_device_gone_(err))
      ESP_LOGW(TAG, "capture DQBUF failed: %s", strerror(err));
    return;
  }

  // Only encode frames that are going somewhere. The sensor sets the pace, so
  // encoding every frame and dropping most of them in deliver_frame_ would run
  // the encoder at the sensor's rate whatever max_framerate says. Skipping
  // leaves the encoder as STREAMON left it.
  const bool wanted = (this->stream_requesters_ != 0) || (this->single_requesters_ != 0);
  const bool due = this->min_interval_ms_ == 0 || (millis() - this->last_frame_ms_) >= this->min_interval_ms_;

  bool encoder_broken = false;
  if (wanted && due && cap_buf.index < (uint32_t) this->num_capture_buffers_ && cap_buf.bytesused > 0) {
    // M2M encode in the order of Espressif's examples/m2m: queue the raw frame on
    // OUTPUT, dequeue the encoded frame from CAPTURE, only then reclaim OUTPUT.
    // The encoder releases the input as part of completing the output, so waiting
    // on OUTPUT first deadlocks.
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
                 (unsigned) ptr, esp_ptr_external_ram(start) ? "yes" : "NO", (unsigned) (ptr % 32),
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
          // esp_video refuses to queue an element it still holds, and this path always
          // uses OUTPUT index 0, so a missed reclaim would wedge every later frame.
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
    // stop_capture_() clears any pending retry, and loop() would otherwise
    // start the whole pipeline again on its very next iteration -- two opens,
    // an S_FMT, a REQBUFS, three mmaps and a STREAMON, torn down again the
    // moment the encoder wedges on the first frame. Space the attempts out.
    this->capture_retry_pending_ = true;
    this->capture_retry_at_ms_ = millis() + this->capture_retry_interval_ms_();
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

// The three below deliberately do not touch the pipeline: they run in the
// caller's task while loop() owns the fds, the mapped buffers and the stream
// state. All they share with it is the requester masks, which are atomic, and
// loop() picks the change up on its next iteration.
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
  uint32_t width = 0, height = 0;
  bool force_res = parse_resolution(this->resolution_, width, height);

  // A MIPI-CSI sensor cannot be resized through V4L2: common_video_set_format()
  // rejects any size but the sensor's current one, and ENUM_FRAMESIZES reports
  // only that one. The size is a build-time Kconfig choice driven by
  // `resolution:` (see __init__.py). USB-UVC devices do resize at runtime.
  //
  // This asks about capture_fd_, and on the hardware-JPEG path that is the
  // MIPI-CSI node while resolved_device_ names the encoder -- so the name alone
  // said a sensor could be resized.
  const bool device_can_resize = !this->is_hw_jpeg_ && this->resolved_device_ != ESP_VIDEO_MIPI_CSI_DEVICE_NAME;

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
    // A MIPI sensor rejects any size but its own and keeps streaming the one it
    // has, so there this is only worth a warning. A USB camera is different:
    // S_FMT is what picks its stream format and frame interval, and without it
    // STREAMON has nothing to negotiate with -- "Could not find requested frame
    // format". Reading a plausible format back afterwards does not mean the
    // device accepted anything, so do not let that paper over it.
    if (this->is_uvc_device_()) {
      ESP_LOGE(TAG, "VIDIOC_S_FMT failed on the USB camera (%s); it never agreed a stream format", strerror(errno));
      return false;
    }
    ESP_LOGW(TAG, "VIDIOC_S_FMT (best-effort resolution) failed: %s", strerror(errno));
  }

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

  // The pixel format is not negotiable: a fallback would be streamed as a corrupt
  // image rather than reported. JPEG and MJPEG are the same payload here.
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
  this->stats_since_ms_ = millis();
  this->stats_frames_ = 0;
  this->stats_bytes_ = 0;
  this->logged_qbuf_failure_ = false;
  this->warned_no_frames_ = false;
  return true;
}

bool ESPVideoCamera::start_direct_capture_() {
  this->capture_fd_ = open(this->resolved_device_.c_str(), O_RDWR | O_NONBLOCK);
  if (this->capture_fd_ < 0) {
    // Not an error for a USB camera: opening the node is what runs esp_video's
    // uvc_video_init(), which fails until a camera has enumerated. Say how many
    // USB devices the host can see, because that splits the two causes apart:
    // none at all means power or cabling (on many boards the host port's 5 V is
    // behind a GPIO), while a device that is there but not usable means it did
    // not offer a UVC streaming interface.
    if (this->is_uvc_device_()) {
      ESP_LOGW(TAG, "No USB camera on %s yet (%s); %d USB device(s) on the bus", this->resolved_device_.c_str(),
               strerror(errno), count_usb_devices());
    } else {
      ESP_LOGE(TAG, "open(%s) failed: %s", this->resolved_device_.c_str(), strerror(errno));
    }
    return false;
  }
  set_dqbuf_timeout(this->capture_fd_, CAPTURE_DQBUF_POLL_MS, "capture");
  // JPEG, not MJPEG: esp_video's UVC driver maps a camera's MJPEG stream onto
  // V4L2_PIX_FMT_JPEG and rejects V4L2_PIX_FMT_MJPEG outright, so asking for
  // MJPEG loses the S_FMT and leaves the stream unnegotiated. The payload is
  // the same either way.
  if (!this->configure_capture_format_(V4L2_PIX_FMT_JPEG))
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
  set_dqbuf_timeout(this->capture_fd_, CAPTURE_DQBUF_POLL_MS, "capture");
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

  // CAPTURE queue before OUTPUT, as in esp_video's M2M example: the encoder
  // needs somewhere to write before it is handed anything to encode.
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
  // Let go of the last frame. Listeners hold their own shared_ptr, so this
  // frees nothing early -- but without it the final JPEG of a capture stays in
  // PSRAM for the life of the device, which at 1080p is a few hundred kilobytes
  // held for a camera nobody is watching.
  this->current_image_.reset();
  // Any pending re-open is re-armed by handle_device_gone_() after this call;
  // a clean stop must not leave one behind.
  this->capture_retry_pending_ = false;
}

void ESPVideoCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP-Video Camera:\n"
                "  Name: %s\n"
                "  Source: %s (%s)\n"
                "  Resolution: %s\n"
                "  Max framerate: %.1f fps",
                this->get_name().c_str(), this->device_.c_str(), this->resolved_device_.c_str(),
                this->resolution_.c_str(), this->max_framerate_);
  if (this->is_hw_jpeg_)
    ESP_LOGCONFIG(TAG, "  JPEG quality: %d", this->jpeg_quality_);

  // Neither of the next two means anything for a USB camera: it has no sensor
  // clock to generate and is not probed over SCCB.
  if (!this->is_uvc_device_()) {
    // The XCLK settings are board-specific and are the first thing to have got
    // wrong on a sensor that never answers.
    char generated[40];
    const char *xclk = "left to the board";
    if (this->enable_xclk_init_) {
      snprintf(generated, sizeof(generated), "GPIO%d at %u Hz", (int) this->xclk_pin_, (unsigned) this->xclk_freq_);
      xclk = generated;
    }
    // Which sensor drivers esp_cam_sensor actually built in. esp_video_init()
    // probes exactly these over SCCB, so when auto-detection comes up empty
    // this says whether the sensor you are holding is even in the firmware.
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
    ESP_LOGCONFIG(TAG,
                  "  XCLK: %s\n"
                  "  MIPI-CSI drivers:%s",
                  xclk, drivers.empty() ? " none" : drivers.c_str());
  }

  if (this->is_failed())
    ESP_LOGCONFIG(TAG, "  State: FAILED");
}

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4
