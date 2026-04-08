#include "esphome/core/defines.h"
#if defined(USE_CSI_CAMERA) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "csi_camera.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <esp_cam_ctlr.h>
#include <esp_cam_ctlr_csi.h>

#include <driver/isp.h>
#include <driver/isp_ccm.h>
#include <driver/isp_color.h>
#include <driver/isp_demosaic.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <cstdio>

namespace esphome::csi_camera {

static const char *const TAG = "csi_camera";
static constexpr uint32_t SETUP_LDO_DELAY_MS = 200;
static constexpr uint32_t HERTZ_PER_MEGAHERTZ = 1000000;
static constexpr uint32_t ISP_PROCESSOR_CLOCK_LOW_HZ = 160 * HERTZ_PER_MEGAHERTZ;
static constexpr uint32_t ISP_PROCESSOR_CLOCK_HIGH_HZ = 240 * HERTZ_PER_MEGAHERTZ;
static constexpr uint32_t ISP_PROCESSOR_LOW_CLOCK_INPUT_MAX_HZ = 80 * HERTZ_PER_MEGAHERTZ;
static constexpr uint8_t DEFAULT_MIPI_LANE_COUNT = 2;
static constexpr uint8_t YUV420_BYTES_NUMERATOR = 3;
static constexpr uint8_t YUV420_BYTES_DENOMINATOR = 2;
static constexpr size_t FRAME_BUFFER_ALIGNMENT_BYTES = 64;
static constexpr uint8_t CSI_CONTROLLER_ID = 0;
static constexpr uint8_t CSI_QUEUE_ITEMS = 1;
static constexpr uint32_t CAPTURE_TASK_STACK_BYTES = 8192;
static constexpr UBaseType_t CAPTURE_TASK_PRIORITY = 5;
static constexpr BaseType_t CAPTURE_TASK_CORE = 1;
static constexpr uint32_t CAPTURE_WAIT_MS = 1000;
static constexpr uint32_t INITIAL_FRAME_LOG_COUNT = 5;
static constexpr uint32_t FRAME_RATE_LOG_INTERVAL = 300;
static constexpr uint8_t FRAME_RATE_LOG_DECIMAL_SCALE = 10;
static constexpr uint32_t COLOR_FIXED_POINT_SCALE = 128;
static constexpr uint32_t COLOR_MAX_INTEGER = 1;
static constexpr uint32_t COLOR_MAX_DECIMAL = 127;
static constexpr uint32_t DEMOSAIC_GRAD_RATIO_INTEGER = 2;
static constexpr uint32_t DEMOSAIC_GRAD_RATIO_DECIMAL = 5;

bool CsiVideoFrame::allocate(size_t capacity, bool *dma_capable) {
  if (capacity == 0 || this->data_.load(std::memory_order_acquire) != nullptr) {
    return false;
  }

  bool is_dma_capable = true;
  void *data = heap_caps_aligned_alloc(FRAME_BUFFER_ALIGNMENT_BYTES, capacity,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (data == nullptr) {
    is_dma_capable = false;
    data = heap_caps_aligned_alloc(FRAME_BUFFER_ALIGNMENT_BYTES, capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (data == nullptr) {
    return false;
  }

  this->capacity_ = capacity;
  this->acquired_.store(false, std::memory_order_release);
  this->release_pending_.store(false, std::memory_order_release);
  this->data_.store(static_cast<uint8_t *>(data), std::memory_order_release);
  if (dma_capable != nullptr) {
    *dma_capable = is_dma_capable;
  }
  return true;
}

bool CsiVideoFrame::try_acquire() {
  if (this->data_.load(std::memory_order_acquire) == nullptr) {
    return false;
  }
  bool expected = false;
  return this->acquired_.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
}

void CsiVideoFrame::release_unpublished() { this->acquired_.store(false, std::memory_order_release); }

camera_video::FrameRef<camera_video::CameraVideoFrame> CsiVideoFrame::commit(uint64_t timestamp_us) {
  this->timestamp_us_ = timestamp_us;
  return camera_video::FrameRef<camera_video::CameraVideoFrame>(this);
}

void CsiVideoFrame::release_storage() {
  this->release_pending_.store(true, std::memory_order_release);
  if (!this->has_references()) {
    this->free_storage_();
  }
}

void CsiVideoFrame::on_last_reference() {
  if (this->release_pending_.load(std::memory_order_acquire)) {
    this->free_storage_();
  } else {
    this->acquired_.store(false, std::memory_order_release);
  }
}

void CsiVideoFrame::free_storage_() {
  uint8_t *data = this->data_.exchange(nullptr, std::memory_order_acq_rel);
  if (data != nullptr) {
    heap_caps_free(data);
    this->capacity_ = 0;
    this->timestamp_us_ = 0;
    this->acquired_.store(false, std::memory_order_release);
  }
}

void CsiCamera::set_night_mode(bool enabled) {
  this->sensor_config_.night_mode = enabled;
  this->sensor_.set_night_mode(enabled);
}

struct IspColorFixedPoint {
  uint32_t integer;
  uint32_t decimal;
};

static IspColorFixedPoint make_isp_color_fixed_point(float value) {
  const uint32_t integer = clamp_at_most(static_cast<uint32_t>(value), COLOR_MAX_INTEGER);
  const float fractional = value - static_cast<float>(integer);
  const uint32_t decimal =
      clamp_at_most(static_cast<uint32_t>(fractional * COLOR_FIXED_POINT_SCALE), COLOR_MAX_DECIMAL);
  return {integer, decimal};
}

// ISR DMA callbacks

bool IRAM_ATTR CsiCamera::dma_start(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *transaction, void *user_data) {
  (void) handle;
  return static_cast<CsiCamera *>(user_data)->dma_start_cb_(transaction);
}
bool IRAM_ATTR CsiCamera::dma_complete(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *transaction,
                                       void *user_data) {
  (void) handle;
  return static_cast<CsiCamera *>(user_data)->dma_complete_cb_(transaction);
}

bool CsiCamera::dma_start_cb_(esp_cam_ctlr_trans_t *trans) {
  for (size_t index = 0; index < static_cast<size_t>(this->frame_buffer_count_); index++) {
    CsiVideoFrame &frame = this->frame_slots_[index];
    if (frame.try_acquire()) {
      trans->buffer = frame.get_data_buffer();
      trans->buflen = frame.get_data_length();
      return false;
    }
  }

  // Leave the transaction empty so the CSI driver captures into its internal backup buffer.
  this->dropped_frames_.fetch_add(1, std::memory_order_relaxed);
  return false;
}

bool CsiCamera::dma_complete_cb_(esp_cam_ctlr_trans_t *trans) {
  CsiVideoFrame *frame = nullptr;
  for (size_t index = 0; index < static_cast<size_t>(this->frame_buffer_count_); index++) {
    if (this->frame_slots_[index].get_data_buffer() == trans->buffer) {
      frame = &this->frame_slots_[index];
      break;
    }
  }
  if (frame == nullptr) {
    return false;
  }

  CsiVideoFrame *replaced = this->ready_frame_.exchange(frame, std::memory_order_acq_rel);
  if (replaced != nullptr) {
    replaced->release_unpublished();
    this->dropped_frames_.fetch_add(1, std::memory_order_relaxed);
  }

  BaseType_t higher_priority_task_woken = pdFALSE;
  const TaskHandle_t task_handle = this->capture_task_handle_.load(std::memory_order_relaxed);
  if (task_handle != nullptr) {
    vTaskNotifyGiveFromISR(task_handle, &higher_priority_task_woken);
  }
  return higher_priority_task_woken == pdTRUE;
}

// Setup

void CsiCamera::setup() {
  const CsiFormat &format = this->sensor_config_.format;
  ESP_LOGI(TAG, "Initializing %s CSI camera %" PRIu16 "x%" PRIu16 "@%" PRIu16 "fps %s", this->sensor_.name(),
           format.width, format.height, format.fps, csi_raw_format_to_string(format.raw_format));
  if (this->power_down_pin_ >= 0) {
    gpio_set_direction(static_cast<gpio_num_t>(this->power_down_pin_), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(this->power_down_pin_), 0);
  }
  this->set_timeout(SETUP_LDO_DELAY_MS, [this]() { this->setup_power_and_sensor_(); });
}

bool CsiCamera::setup_sensor_(CsiSensorSetup &setup) {
  this->sensor_config_.sccb_adapter = &this->i2c_adapter_;
  this->sensor_config_.i2c_address = this->get_i2c_address();
  this->sensor_config_.power_down_pin = this->power_down_pin_;
  if (!this->sensor_.setup(this->sensor_config_, &setup)) {
    return false;
  }
  return true;
}

bool CsiCamera::allocate_frame_buffers_() {
  if (this->frame_buffer_count_ <= 0 || static_cast<size_t>(this->frame_buffer_count_) > MAX_FRAME_BUFFERS) {
    ESP_LOGE(TAG, "invalid frame_buffer_count=%d", this->frame_buffer_count_);
    return false;
  }

  const size_t frame_stride =
      static_cast<size_t>(this->sensor_setup_.format.width) * YUV420_BYTES_NUMERATOR / YUV420_BYTES_DENOMINATOR;
  const size_t frame_buffer_size = frame_stride * this->sensor_setup_.format.height;
  this->video_frame_spec_ = {this->sensor_setup_.format.width,
                             this->sensor_setup_.format.height,
                             this->sensor_setup_.format.fps,
                             camera_video::VideoPixelFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY,
                             frame_stride,
                             frame_buffer_size};

  if (static_cast<size_t>(this->frame_buffer_count_) < MAX_FRAME_BUFFERS) {
    ESP_LOGW(TAG,
             "frame_buffer_count=%d leaves no spare buffer while a downstream consumer retains one frame; "
             "use %zu for low-latency streaming",
             this->frame_buffer_count_, MAX_FRAME_BUFFERS);
  }

  bool all_buffers_dma_capable = true;
  for (size_t index = 0; index < static_cast<size_t>(this->frame_buffer_count_); index++) {
    bool dma_capable = false;
    if (!this->frame_slots_[index].allocate(frame_buffer_size, &dma_capable)) {
      ESP_LOGE(TAG, "Frame buffer allocation failed (%d x %zu B)", this->frame_buffer_count_, frame_buffer_size);
      this->release_frame_buffers_();
      return false;
    }
    all_buffers_dma_capable &= dma_capable;
  }

  ESP_LOGI(TAG, "Frame buffers: %d × %zu B SPIRAM%s", this->frame_buffer_count_, frame_buffer_size,
           all_buffers_dma_capable ? " DMA" : "");
  if (!all_buffers_dma_capable) {
    ESP_LOGW(TAG, "CSI frame buffers are not MALLOC_CAP_DMA; downstream consumers may require an input copy");
  }
  return true;
}

void CsiCamera::release_frame_buffers_() {
  CsiVideoFrame *ready = this->ready_frame_.exchange(nullptr, std::memory_order_acq_rel);
  if (ready != nullptr) {
    ready->release_unpublished();
  }
  for (auto &frame : this->frame_slots_) {
    frame.release_storage();
  }
  this->video_frame_spec_ = {};
}

bool CsiCamera::configure_demosaic_(CsiBayerOrder bayer_order) {
  esp_isp_demosaic_config_t demosaic_config = {};
  demosaic_config.grad_ratio.integer = DEMOSAIC_GRAD_RATIO_INTEGER;
  demosaic_config.grad_ratio.decimal = DEMOSAIC_GRAD_RATIO_DECIMAL;
  demosaic_config.padding_mode = ISP_DEMOSAIC_EDGE_PADDING_MODE_SRND_DATA;

  esp_err_t error = esp_isp_demosaic_configure(this->isp_proc_, &demosaic_config);
  if (error == ESP_OK) {
    error = esp_isp_demosaic_enable(this->isp_proc_);
  }
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "ISP demosaic enable failed: %s", esp_err_to_name(error));
    return false;
  }
  ESP_LOGI(TAG, "ISP demosaic enabled, bayer_order=%s", csi_bayer_order_to_string(bayer_order));
  return true;
}

void CsiCamera::configure_ccm_() {
  if (!this->ccm_.has_value())
    return;
  esp_isp_ccm_config_t config = {};
  for (size_t row = 0; row < 3; row++) {
    for (size_t column = 0; column < 3; column++)
      config.matrix[row][column] = (*this->ccm_)[row * 3 + column];
  }
  config.saturation = true;
  config.flags.update_once_configured = true;
  esp_err_t error = esp_isp_ccm_configure(this->isp_proc_, &config);
  if (error == ESP_OK)
    error = esp_isp_ccm_enable(this->isp_proc_);
  if (error != ESP_OK)
    ESP_LOGW(TAG, "ISP CCM enable failed: %s", esp_err_to_name(error));
}

void CsiCamera::configure_color_() {
  const IspColorFixedPoint contrast = make_isp_color_fixed_point(this->contrast_);
  const IspColorFixedPoint saturation = make_isp_color_fixed_point(this->saturation_);

  esp_isp_color_config_t color_config = {};
  color_config.color_contrast.integer = contrast.integer;
  color_config.color_contrast.decimal = contrast.decimal;
  color_config.color_saturation.integer = saturation.integer;
  color_config.color_saturation.decimal = saturation.decimal;
  color_config.color_hue = this->hue_;
  color_config.color_brightness = this->brightness_;
  color_config.flags.update_once_configured = true;

  esp_err_t error = esp_isp_color_configure(this->isp_proc_, &color_config);
  if (error == ESP_OK) {
    error = esp_isp_color_enable(this->isp_proc_);
  }
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "ISP color controller enable failed: %s", esp_err_to_name(error));
    return;
  }
  ESP_LOGI(TAG, "ISP color enabled: hue=%" PRIu32 " brightness=%d contrast=%.2f saturation=%.2f", this->hue_,
           this->brightness_, this->contrast_, this->saturation_);
}

bool CsiCamera::configure_isp_(const CsiSensorSetup &setup, CsiBayerOrder bayer_order) {
  const uint32_t isp_clock_hz = setup.pixel_clock_hz <= ISP_PROCESSOR_LOW_CLOCK_INPUT_MAX_HZ
                                    ? ISP_PROCESSOR_CLOCK_LOW_HZ
                                    : ISP_PROCESSOR_CLOCK_HIGH_HZ;
  ESP_LOGI(TAG, "ISP clock: %" PRIu32 " MHz for sensor pixel clock %" PRIu32 " Hz", isp_clock_hz / HERTZ_PER_MEGAHERTZ,
           setup.pixel_clock_hz);

  esp_isp_processor_cfg_t isp_config = {};
  isp_config.clk_hz = isp_clock_hz;
  isp_config.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
  isp_config.input_data_color_type =
      setup.format.raw_format == CsiRawFormat::CSI_RAW_FORMAT_RAW8 ? ISP_COLOR_RAW8 : ISP_COLOR_RAW10;
  isp_config.output_data_color_type = ISP_COLOR_YUV420;
  switch (bayer_order) {
    case CsiBayerOrder::CSI_BAYER_ORDER_RGGB:
      isp_config.bayer_order = COLOR_RAW_ELEMENT_ORDER_RGGB;
      break;
    case CsiBayerOrder::CSI_BAYER_ORDER_GRBG:
      isp_config.bayer_order = COLOR_RAW_ELEMENT_ORDER_GRBG;
      break;
    case CsiBayerOrder::CSI_BAYER_ORDER_GBRG:
      isp_config.bayer_order = COLOR_RAW_ELEMENT_ORDER_GBRG;
      break;
    case CsiBayerOrder::CSI_BAYER_ORDER_BGGR:
      isp_config.bayer_order = COLOR_RAW_ELEMENT_ORDER_BGGR;
      break;
  }
  isp_config.has_line_start_packet = setup.uses_line_sync;
  isp_config.has_line_end_packet = setup.uses_line_sync;
  isp_config.h_res = setup.format.width;
  isp_config.v_res = setup.format.height;

  esp_err_t error = esp_isp_new_processor(&isp_config, &this->isp_proc_);
  if (error == ESP_OK) {
    error = esp_isp_enable(this->isp_proc_);
    if (error == ESP_OK) {
      this->isp_enabled_ = true;
    }
  }
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "ISP setup failed: %s", esp_err_to_name(error));
    return false;
  }
  if (!this->configure_demosaic_(bayer_order)) {
    return false;
  }
  this->configure_ccm_();
  this->configure_color_();
  return true;
}

bool CsiCamera::configure_csi_controller_(const CsiSensorSetup &setup) {
  esp_cam_ctlr_csi_config_t csi_cfg = {};
  csi_cfg.ctlr_id = CSI_CONTROLLER_ID;
  csi_cfg.clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT;
  csi_cfg.h_res = setup.format.width;
  csi_cfg.v_res = setup.format.height;
  csi_cfg.data_lane_num = setup.lane_count;
  csi_cfg.lane_bit_rate_mbps = setup.lane_bit_rate_mbps;
  csi_cfg.input_data_color_type =
      setup.format.raw_format == CsiRawFormat::CSI_RAW_FORMAT_RAW8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
  csi_cfg.output_data_color_type = CAM_CTLR_COLOR_YUV420;
  csi_cfg.queue_items = CSI_QUEUE_ITEMS;
  csi_cfg.byte_swap_en = false;
  csi_cfg.bk_buffer_dis = false;

  if (esp_cam_new_csi_ctlr(&csi_cfg, &this->cam_handle_) != ESP_OK) {
    ESP_LOGE(TAG, "CSI controller creation failed");
    return false;
  }

  esp_cam_ctlr_evt_cbs_t callbacks = {};
  callbacks.on_get_new_trans = CsiCamera::dma_start;
  callbacks.on_trans_finished = CsiCamera::dma_complete;
  esp_err_t controller_err = esp_cam_ctlr_register_event_callbacks(this->cam_handle_, &callbacks, this);
  if (controller_err == ESP_OK) {
    controller_err = esp_cam_ctlr_enable(this->cam_handle_);
    if (controller_err == ESP_OK) {
      this->controller_enabled_ = true;
    }
  }
  if (controller_err == ESP_OK) {
    controller_err = esp_cam_ctlr_start(this->cam_handle_);
    if (controller_err == ESP_OK) {
      this->controller_started_ = true;
    }
  }
  if (controller_err != ESP_OK) {
    ESP_LOGE(TAG, "CSI controller start failed: %s", esp_err_to_name(controller_err));
    return false;
  }
  ESP_LOGI(TAG, "CSI controller started: queue_items=%u frame_buffer_count=%d", CSI_QUEUE_ITEMS,
           this->frame_buffer_count_);
  return true;
}

void CsiCamera::fail_setup_(const char *message) {
  ESP_LOGE(TAG, "%s", message);
  this->request_capture_task_stop_();
  this->cleanup_hardware_();
  this->status_set_error();
  this->mark_failed();
}

void CsiCamera::setup_power_and_sensor_() {
  if (this->power_supply_ != nullptr && this->power_supply_->is_failed()) {
    this->fail_setup_("configured camera power supply failed");
    return;
  }
  ESP_LOGI(TAG, "Camera power: %s", this->power_supply_ != nullptr ? "esp_ldo" : "external/board");

  if (!this->setup_sensor_(this->sensor_setup_)) {
    this->fail_setup_("camera sensor setup failed");
    return;
  }

  const bool mirror_changed =
      this->sensor_setup_.orientation_state_valid &&
      this->sensor_setup_.default_horizontal_mirror != this->sensor_setup_.final_horizontal_mirror;
  const bool flip_changed = this->sensor_setup_.orientation_state_valid &&
                            this->sensor_setup_.default_vertical_flip != this->sensor_setup_.final_vertical_flip;
  this->effective_bayer_order_ =
      this->bayer_order_auto_
          ? csi_bayer_order_after_orientation(this->sensor_setup_.bayer_order, mirror_changed, flip_changed)
          : this->bayer_order_;
  if (this->sensor_setup_.orientation_state_valid) {
    ESP_LOGI(TAG, "Bayer order: sensor=%s requested=%s effective=%s orientation=mirror:%s->%s flip:%s->%s",
             csi_bayer_order_to_string(this->sensor_setup_.bayer_order), this->bayer_order_auto_ ? "auto" : "manual",
             csi_bayer_order_to_string(this->effective_bayer_order_),
             ONOFF(this->sensor_setup_.default_horizontal_mirror), ONOFF(this->sensor_setup_.final_horizontal_mirror),
             ONOFF(this->sensor_setup_.default_vertical_flip), ONOFF(this->sensor_setup_.final_vertical_flip));
  } else {
    ESP_LOGI(TAG, "Bayer order: sensor=%s requested=%s effective=%s orientation=unknown",
             csi_bayer_order_to_string(this->sensor_setup_.bayer_order), this->bayer_order_auto_ ? "auto" : "manual",
             csi_bayer_order_to_string(this->effective_bayer_order_));
  }

  this->set_timeout("csi_setup_buffers", 0, [this]() { this->setup_buffers_(); });
}

void CsiCamera::setup_buffers_() {
  if (!this->allocate_frame_buffers_()) {
    this->fail_setup_("camera frame-buffer setup failed");
    return;
  }
  this->set_timeout("csi_setup_isp", 0, [this]() { this->setup_isp_(); });
}

void CsiCamera::setup_isp_() {
  if (!this->configure_isp_(this->sensor_setup_, this->effective_bayer_order_)) {
    this->fail_setup_("camera ISP setup failed");
    return;
  }
  this->set_timeout("csi_setup_controller", 0, [this]() { this->start_pipeline_(); });
}

void CsiCamera::start_pipeline_() {
  if (!this->configure_csi_controller_(this->sensor_setup_)) {
    this->fail_setup_("camera CSI controller setup failed");
    return;
  }
  if (!this->sensor_.start_stream()) {
    this->fail_setup_("camera sensor stream start failed");
    return;
  }

  this->capture_task_stop_.store(false, std::memory_order_release);
  this->capture_task_exited_.store(false, std::memory_order_release);
  TaskHandle_t capture_task_handle = nullptr;
  // StaticTask does not expose core affinity; CSI capture is pinned to the second P4 core.
  if (xTaskCreatePinnedToCore(CsiCamera::capture_task_entry, "csi_cap", CAPTURE_TASK_STACK_BYTES, this,
                              CAPTURE_TASK_PRIORITY, &capture_task_handle, CAPTURE_TASK_CORE) != pdPASS) {
    this->capture_task_exited_.store(true, std::memory_order_release);
    this->fail_setup_("capture task creation failed");
    return;
  }
  this->capture_task_handle_.store(capture_task_handle, std::memory_order_release);

  this->video_source_ready_.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "CSI camera ready %" PRIu16 "x%" PRIu16 "@%" PRIu16 "fps yuv420", this->sensor_setup_.format.width,
           this->sensor_setup_.format.height, this->sensor_setup_.format.fps);
}

void CsiCamera::request_capture_task_stop_() {
  this->video_source_ready_.store(false, std::memory_order_release);
  this->capture_task_stop_.store(true, std::memory_order_release);
  this->wake_capture_task_();
}

void CsiCamera::wake_capture_task_() {
  const TaskHandle_t task_handle = this->capture_task_handle_.load(std::memory_order_acquire);
  if (task_handle != nullptr) {
    xTaskNotifyGive(task_handle);
  }
}

void CsiCamera::cleanup_hardware_() {
  this->video_source_ready_.store(false, std::memory_order_release);
  this->sensor_.stop_stream();
  if (this->cam_handle_ != nullptr) {
    if (this->controller_started_) {
      (void) esp_cam_ctlr_stop(this->cam_handle_);
      this->controller_started_ = false;
    }
    if (this->controller_enabled_) {
      (void) esp_cam_ctlr_disable(this->cam_handle_);
      this->controller_enabled_ = false;
    }
    (void) esp_cam_ctlr_del(this->cam_handle_);
    this->cam_handle_ = nullptr;
  }
  if (this->isp_proc_ != nullptr) {
    if (this->isp_enabled_) {
      (void) esp_isp_disable(this->isp_proc_);
      this->isp_enabled_ = false;
    }
    (void) esp_isp_del_processor(this->isp_proc_);
    this->isp_proc_ = nullptr;
  }
  this->sensor_.reset();
  this->release_frame_buffers_();
}

void CsiCamera::on_shutdown() {
  if (this->shutdown_started_.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  if (this->cam_handle_ != nullptr && this->controller_started_) {
    (void) esp_cam_ctlr_stop(this->cam_handle_);
    this->controller_started_ = false;
  }
  this->request_capture_task_stop_();
}

bool CsiCamera::teardown() {
  const TaskHandle_t task_handle = this->capture_task_handle_.load(std::memory_order_acquire);
  if (task_handle != nullptr && !this->capture_task_exited_.load(std::memory_order_acquire)) {
    return false;
  }
  this->capture_task_handle_.store(nullptr, std::memory_order_release);
  this->cleanup_hardware_();
  return true;
}

void CsiCamera::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32-P4 CSI Camera:");
  ESP_LOGCONFIG(TAG, "  sensor: %s", this->sensor_.name());
  ESP_LOGCONFIG(TAG, "  sensor format: %" PRIu16 "x%" PRIu16 "@%" PRIu16 "fps %s", this->sensor_config_.format.width,
                this->sensor_config_.format.height, this->sensor_config_.format.fps,
                csi_raw_format_to_string(this->sensor_config_.format.raw_format));
  ESP_LOGCONFIG(TAG, "  output: YUV420");
  ESP_LOGCONFIG(TAG, "  power supply: %s", this->power_supply_ != nullptr ? "esp_ldo" : "external/board");
  ESP_LOGCONFIG(TAG, "  frame buffers: %d", this->frame_buffer_count_);
  ESP_LOGCONFIG(TAG, "  color: hue=%" PRIu32 " brightness=%d contrast=%.2f saturation=%.2f", this->hue_,
                this->brightness_, this->contrast_, this->saturation_);
  ESP_LOGCONFIG(TAG, "  CCM: %s test_pattern=%s night_mode=%s", YESNO(this->ccm_.has_value()),
                ONOFF(this->sensor_config_.test_pattern),
                this->sensor_config_.night_mode.has_value() ? ONOFF(*this->sensor_config_.night_mode) : "unchanged");
  ESP_LOGCONFIG(
      TAG, "  orientation: horizontal_mirror=%s vertical_flip=%s",
      this->sensor_config_.horizontal_mirror.has_value() ? ONOFF(*this->sensor_config_.horizontal_mirror) : "unchanged",
      this->sensor_config_.vertical_flip.has_value() ? ONOFF(*this->sensor_config_.vertical_flip) : "unchanged");
  ESP_LOGCONFIG(TAG, "  bayer order: %s (%s)", csi_bayer_order_to_string(this->effective_bayer_order_),
                this->bayer_order_auto_ ? "auto" : "manual");
}

bool CsiCamera::start_stream(camera_video::CameraVideoSourceListener *listener) {
  if (listener == nullptr || this->is_failed()) {
    return false;
  }
  LockGuard lock(this->listeners_mutex_);
  if (std::find(this->listeners_.begin(), this->listeners_.end(), listener) != this->listeners_.end()) {
    return true;
  }
  if (this->listeners_.size() >= MAX_CAMERA_LISTENERS) {
    ESP_LOGE(TAG, "Maximum raw-video listener count reached");
    return false;
  }
  this->listeners_.push_back(listener);
  return true;
}

void CsiCamera::stop_stream(camera_video::CameraVideoSourceListener *listener) {
  if (listener == nullptr) {
    return;
  }
  LockGuard lock(this->listeners_mutex_);
  StaticVector<camera_video::CameraVideoSourceListener *, MAX_CAMERA_LISTENERS> retained;
  for (auto *registered : this->listeners_) {
    if (registered != listener) {
      retained.push_back(registered);
    }
  }
  this->listeners_.assign(retained.begin(), retained.end());
}

void CsiCamera::capture_task_entry(void *arg) {
  auto *self = static_cast<CsiCamera *>(arg);
  self->capture_task_handle_.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
  self->capture_task_body_();
  self->capture_task_exited_.store(true, std::memory_order_release);
  self->enable_loop_soon_any_context();
  vTaskDelete(nullptr);
}

void CsiCamera::capture_task_body_() {
  uint32_t frame_count = 0;
  uint32_t rate_start_us = 0;
  while (!this->capture_task_stop_.load(std::memory_order_acquire)) {
    CsiVideoFrame *frame_slot = this->ready_frame_.exchange(nullptr, std::memory_order_acq_rel);
    if (frame_slot == nullptr) {
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CAPTURE_WAIT_MS)) == 0 &&
          !this->capture_task_stop_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "No frame in %" PRIu32 "ms", CAPTURE_WAIT_MS);
      }
      continue;
    }
    if (this->capture_task_stop_.load(std::memory_order_acquire)) {
      frame_slot->release_unpublished();
      break;
    }

    frame_count++;
    const uint32_t now_us = micros();
    if (rate_start_us == 0) {
      rate_start_us = now_us;
    }
    if (frame_count == 1) {
      ESP_LOGI(TAG, "Frame #%" PRIu32 " buf=%p", frame_count, frame_slot->get_data_buffer());
    } else if (frame_count <= INITIAL_FRAME_LOG_COUNT) {
      ESP_LOGD(TAG, "Frame #%" PRIu32 " buf=%p", frame_count, frame_slot->get_data_buffer());
    } else if (frame_count % FRAME_RATE_LOG_INTERVAL == 0) {
      uint32_t source_fps_tenths = 0;
      const uint32_t elapsed_us = now_us - rate_start_us;
      if (elapsed_us != 0) {
        source_fps_tenths = static_cast<uint32_t>(
            (static_cast<uint64_t>(FRAME_RATE_LOG_INTERVAL) * FRAME_RATE_LOG_DECIMAL_SCALE * HERTZ_PER_MEGAHERTZ +
             elapsed_us / 2U) /
            elapsed_us);
      }
      ESP_LOGD(TAG, "Frame #%" PRIu32 " buf=%p source=%" PRIu32 ".%" PRIu32 "fps dropped=%" PRIu32, frame_count,
               frame_slot->get_data_buffer(), source_fps_tenths / FRAME_RATE_LOG_DECIMAL_SCALE,
               source_fps_tenths % FRAME_RATE_LOG_DECIMAL_SCALE, this->dropped_frames_.load(std::memory_order_relaxed));
      rate_start_us = now_us;
    }

    StaticVector<camera_video::CameraVideoSourceListener *, MAX_CAMERA_LISTENERS> listeners;
    {
      LockGuard lock(this->listeners_mutex_);
      listeners.assign(this->listeners_.begin(), this->listeners_.end());
    }
    if (listeners.empty()) {
      frame_slot->release_unpublished();
      continue;
    }

    auto frame = frame_slot->commit(static_cast<uint64_t>(esp_timer_get_time()));
    for (auto *listener : listeners) {
      listener->on_video_frame(frame);
    }
    this->delivered_frames_.fetch_add(1, std::memory_order_relaxed);
  }

  CsiVideoFrame *ready = this->ready_frame_.exchange(nullptr, std::memory_order_acq_rel);
  if (ready != nullptr) {
    ready->release_unpublished();
  }
  ESP_LOGI(TAG, "Capture task stopped frames=%" PRIu32 " delivered=%" PRIu32 " dropped=%" PRIu32, frame_count,
           this->delivered_frames_.load(std::memory_order_relaxed),
           this->dropped_frames_.load(std::memory_order_relaxed));
}

}  // namespace esphome::csi_camera

#endif
