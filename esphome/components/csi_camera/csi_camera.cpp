#include "csi_camera.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32

static const char *const TAG = "csi_camera";

// Configuration constants for CSI Camera
#define CSI_CAMERA_H_RES 640
#define CSI_CAMERA_V_RES 480
#define CSI_MIPI_CSI_LANE_BITRATE_MBPS 400
#define CSI_ISP_CLOCK_HZ 80000000  // 80MHz
#define CSI_STREAMING_STACK_SIZE 8192
#define CSI_FRAME_QUEUE_LENGTH 2

namespace esphome {
namespace csi_camera {

CsiCamera::~CsiCamera() {
#ifdef HAS_ESP32_P4_CAMERA
  this->deinit_camera_();
#endif
}

void CsiCamera::setup() {
#ifdef HAS_ESP32_P4_CAMERA
  ESP_LOGCONFIG(TAG, "Setting up CSI Camera with ESP32-P4 MIPI-CSI...");

  ESP_LOGI(TAG, "Step 1: Creating synchronization objects");

  // Creation of synchronization objects
  this->frame_ready_semaphore_ = xSemaphoreCreateBinary();
  if (!this->frame_ready_semaphore_) {
    ESP_LOGE(TAG, "Failed to create frame ready semaphore");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Frame semaphore created successfully");

  this->frame_queue_ = xQueueCreate(CSI_FRAME_QUEUE_LENGTH, sizeof(FrameData));
  if (!this->frame_queue_) {
    ESP_LOGE(TAG, "Failed to create frame queue");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Frame queue created successfully");

  // Configuration of the external clock pin
  if (this->external_clock_pin_ > 0) {
    ESP_LOGD(TAG, "Configuring external clock on GPIO%u at %u Hz", 
             this->external_clock_pin_, this->external_clock_frequency_);
  }

  ESP_LOGI(TAG, "Step 2: Initializing camera");

  if (!this->init_camera_()) {
    ESP_LOGE(TAG, "Failed to initialize camera - setup marked as failed");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "CSI Camera '%s' setup completed successfully", this->name_.c_str());
#else
  ESP_LOGE(TAG, "ESP32-P4 MIPI-CSI API not available - CSI Camera component disabled");
  this->mark_failed();
#endif
}

void CsiCamera::dump_config() {
  ESP_LOGCONFIG(TAG, "CSI Camera '%s':", this->name_.c_str());

  // Debug available macros
  ESP_LOGCONFIG(TAG, "  Platform Debug:");
#ifdef USE_ESP32
  ESP_LOGCONFIG(TAG, "    USE_ESP32: YES");
#else
  ESP_LOGCONFIG(TAG, "    USE_ESP32: NO");
#endif

#ifdef CONFIG_IDF_TARGET_ESP32P4
  ESP_LOGCONFIG(TAG, "    CONFIG_IDF_TARGET_ESP32P4: YES");
#else
  ESP_LOGCONFIG(TAG, "    CONFIG_IDF_TARGET_ESP32P4: NO");
#endif

#ifdef HAS_ESP32_P4_CAMERA
  ESP_LOGCONFIG(TAG, "    HAS_ESP32_P4_CAMERA: YES");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", CSI_CAMERA_H_RES, CSI_CAMERA_V_RES);
  if (this->external_clock_pin_ > 0) {
    ESP_LOGCONFIG(TAG, "  External Clock Pin: GPIO%u", this->external_clock_pin_);
    ESP_LOGCONFIG(TAG, "  External Clock Frequency: %u Hz", this->external_clock_frequency_);
  }
  ESP_LOGCONFIG(TAG, "  Frame Buffer Size: %zu bytes", this->frame_buffer_size_);
  ESP_LOGCONFIG(TAG, "  Streaming Support: Available");
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", this->address_);

  if (this->reset_pin_) {
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
  }
#else
  ESP_LOGCONFIG(TAG, "    HAS_ESP32_P4_CAMERA: NO");
  ESP_LOGCONFIG(TAG, "  Status: ESP32-P4 MIPI-CSI API not available");
#endif

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Setup Failed");
  }
}

float CsiCamera::get_setup_priority() const {
  return setup_priority::HARDWARE - 1.0f;
}

#ifdef HAS_ESP32_P4_CAMERA
bool CsiCamera::init_ldo_() {
  if (this->ldo_initialized_) {
    ESP_LOGI(TAG, "LDO already initialized, skipping");
    return true;
  }

  ESP_LOGI(TAG, "Initializing MIPI LDO regulator");

  // More flexible configuration for the LDO
  esp_ldo_channel_config_t ldo_mipi_phy_config = {
    .chan_id = 3,  // LDO channel ID for MIPI
    .voltage_mv = 2500,  // 2.5V for MIPI PHY
  };

  esp_err_t ret = esp_ldo_acquire_channel(&ldo_mipi_phy_config, &this->ldo_mipi_phy_);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to acquire MIPI LDO channel: %s - trying to continue anyway", esp_err_to_name(ret));
    // Do not fail completely, some boards may not have a dedicated MIPI LDO
    this->ldo_mipi_phy_ = nullptr;
  } else {
    ESP_LOGI(TAG, "MIPI LDO regulator acquired successfully");
  }

  this->ldo_initialized_ = true;
  return true;  // Return true even if LDO fails
}

bool CsiCamera::init_sensor_() {
  if (this->sensor_initialized_) {
    ESP_LOGI(TAG, "Sensor already initialized, skipping");
    return true;
  }

  ESP_LOGI(TAG, "Attempting to initialize camera sensor at I2C address 0x%02X", this->address_);

  // Simple I2C communication test
  uint8_t test_data;
  if (!this->read_byte(0x00, &test_data)) {
    ESP_LOGW(TAG, "Could not read from sensor at address 0x%02X - sensor might not be connected", this->address_);
    // For now, do not fail if the sensor does not respond
    // return false;
  } else {
    ESP_LOGI(TAG, "Sensor responded to I2C communication (reg 0x00 = 0x%02X)", test_data);
  }

  // Basic sensor configuration (adapt as needed for your model)
  // For now, mark as initialized even without complete configuration
  this->sensor_initialized_ = true;
  ESP_LOGI(TAG, "Camera sensor marked as initialized");
  return true;
}

bool CsiCamera::init_camera_() {
  if (this->camera_initialized_) {
    ESP_LOGI(TAG, "Camera already initialized, skipping");
    return true;
  }

  ESP_LOGI(TAG, "Starting camera initialization for '%s'", this->name_.c_str());

  // Step 1: MIPI LDO Initialization
  ESP_LOGI(TAG, "Step 2.1: Initializing MIPI LDO");
  if (!this->init_ldo_()) {
    ESP_LOGE(TAG, "Failed to initialize MIPI LDO");
    return false;
  }
  ESP_LOGI(TAG, "MIPI LDO initialized successfully");

  // Step 2: Camera reset if pin available
  if (this->reset_pin_) {
    ESP_LOGI(TAG, "Step 2.2: Executing camera reset sequence");
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    delayMicroseconds(10000);  // 10ms
    this->reset_pin_->digital_write(true);
    delayMicroseconds(10000);  // 10ms
    ESP_LOGI(TAG, "Camera reset completed");
  } else {
    ESP_LOGI(TAG, "Step 2.2: No reset pin configured, skipping reset");
  }

  // Step 3: I2C sensor initialization
  ESP_LOGI(TAG, "Step 2.3: Initializing camera sensor");
  if (!this->init_sensor_()) {
    ESP_LOGE(TAG, "Failed to initialize camera sensor");
    return false;
  }
  ESP_LOGI(TAG, "Camera sensor initialized successfully");

  // Step 4: Frame buffer allocation
  ESP_LOGI(TAG, "Step 2.4: Allocating frame buffer");
  this->frame_buffer_size_ = CSI_CAMERA_H_RES * CSI_CAMERA_V_RES * 2; // RGB565 = 2 bytes per pixel
  ESP_LOGI(TAG, "Calculated frame buffer size: %zu bytes", this->frame_buffer_size_);

  this->frame_buffer_ = heap_caps_malloc(this->frame_buffer_size_, MALLOC_CAP_SPIRAM);
  if (!this->frame_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate frame buffer (%zu bytes) - trying regular RAM", this->frame_buffer_size_);
    // Try using regular RAM if SPIRAM fails
    this->frame_buffer_ = heap_caps_malloc(this->frame_buffer_size_, MALLOC_CAP_DMA);
    if (!this->frame_buffer_) {
      ESP_LOGE(TAG, "Failed to allocate frame buffer in regular RAM also");
      return false;
    }
  }

  ESP_LOGI(TAG, "Frame buffer allocated: %p, size: %zu bytes", this->frame_buffer_, this->frame_buffer_size_);

  // Step 5: CSI controller configuration
  ESP_LOGI(TAG, "Step 2.5: Configuring CSI controller");
  esp_cam_ctlr_csi_config_t csi_config = {};
  csi_config.ctlr_id = 0;
  csi_config.h_res = CSI_CAMERA_H_RES;
  csi_config.v_res = CSI_CAMERA_V_RES;
  csi_config.lane_bit_rate_mbps = CSI_MIPI_CSI_LANE_BITRATE_MBPS;
  csi_config.input_data_color_type = CAM_CTLR_COLOR_RAW8;
  csi_config.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  csi_config.data_lane_num = 2;
  csi_config.byte_swap_en = false;
  csi_config.queue_items = 1;

  esp_err_t ret = esp_cam_new_csi_ctlr(&csi_config, &this->cam_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "CSI controller init failed: %s", esp_err_to_name(ret));
    return false;
  }
  ESP_LOGI(TAG, "CSI controller created successfully");

  // Step 6: Callback configuration
  ESP_LOGI(TAG, "Step 2.6: Registering camera callbacks");
  esp_cam_ctlr_evt_cbs_t cbs = {
    .on_get_new_trans = CsiCamera::camera_get_new_vb_callback,
    .on_trans_finished = CsiCamera::camera_get_finished_trans_callback,
  };

  ret = esp_cam_ctlr_register_event_callbacks(this->cam_handle_, &cbs, this);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register camera callbacks: %s", esp_err_to_name(ret));
    return false;
  }
  ESP_LOGI(TAG, "Camera callbacks registered successfully");

  // Step 7: Camera controller activation
  ESP_LOGI(TAG, "Step 2.7: Enabling camera controller");
  ret = esp_cam_ctlr_enable(this->cam_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable camera controller: %s", esp_err_to_name(ret));
    return false;
  }
  ESP_LOGI(TAG, "Camera controller enabled successfully");

  // Step 8: ISP configuration
  ESP_LOGI(TAG, "Step 2.8: Configuring ISP processor");
  esp_isp_processor_cfg_t isp_config = {};
  isp_config.clk_hz = CSI_ISP_CLOCK_HZ;
  isp_config.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
  isp_config.input_data_color_type = ISP_COLOR_RAW8;
  isp_config.output_data_color_type = ISP_COLOR_RGB565;
  isp_config.has_line_start_packet = false;
  isp_config.has_line_end_packet = false;
  isp_config.h_res = CSI_CAMERA_H_RES;
  isp_config.v_res = CSI_CAMERA_V_RES;

  ret = esp_isp_new_processor(&isp_config, &this->isp_proc_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ISP processor init failed: %s", esp_err_to_name(ret));
    return false;
  }
  ESP_LOGI(TAG, "ISP processor created successfully");

  ret = esp_isp_enable(this->isp_proc_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable ISP processor: %s", esp_err_to_name(ret));
    return false;
  }
  ESP_LOGI(TAG, "ISP processor enabled successfully");

  // Step 9: Frame buffer initialization
  ESP_LOGI(TAG, "Step 2.9: Initializing frame buffer");
  memset(this->frame_buffer_, 0xFF, this->frame_buffer_size_);
  esp_cache_msync(this->frame_buffer_, this->frame_buffer_size_, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  ESP_LOGI(TAG, "Frame buffer initialized");

  // Step 10: Start the camera
  ESP_LOGI(TAG, "Step 2.10: Starting camera controller");
  ret = esp_cam_ctlr_start(this->cam_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start camera controller: %s", esp_err_to_name(ret));
    return false;
  }

  this->camera_initialized_ = true;
  ESP_LOGI(TAG, "Camera '%s' initialized successfully - all steps completed", this->name_.c_str());
  return true;
}

void CsiCamera::deinit_camera_() {
  if (this->streaming_active_) {
    this->stop_streaming();
  }

  if (this->camera_initialized_) {
    ESP_LOGD(TAG, "Deinitializing camera '%s'...", this->name_.c_str());

    if (this->cam_handle_) {
      esp_cam_ctlr_stop(this->cam_handle_);
      esp_cam_ctlr_disable(this->cam_handle_);
      esp_cam_ctlr_del(this->cam_handle_);
      this->cam_handle_ = nullptr;
    }

    if (this->isp_proc_) {
      esp_isp_disable(this->isp_proc_);
      esp_isp_del_processor(this->isp_proc_);
      this->isp_proc_ = nullptr;
    }

    if (this->frame_buffer_) {
      heap_caps_free(this->frame_buffer_);
      this->frame_buffer_ = nullptr;
    }

    if (this->ldo_mipi_phy_) {
      esp_ldo_release_channel(this->ldo_mipi_phy_);
      this->ldo_mipi_phy_ = nullptr;
    }

    this->camera_initialized_ = false;
    this->sensor_initialized_ = false;
    this->ldo_initialized_ = false;
    ESP_LOGD(TAG, "Camera '%s' deinitialized", this->name_.c_str());
  }

  // Cleanup synchronization objects
  if (this->frame_ready_semaphore_) {
    vSemaphoreDelete(this->frame_ready_semaphore_);
    this->frame_ready_semaphore_ = nullptr;
  }

  if (this->frame_queue_) {
    vQueueDelete(this->frame_queue_);
    this->frame_queue_ = nullptr;
  }
}

bool CsiCamera::camera_get_new_vb_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data) {
  CsiCamera *camera = static_cast<CsiCamera*>(user_data);
  trans->buffer = camera->frame_buffer_;
  trans->buflen = camera->frame_buffer_size_;
  return false;
}

bool CsiCamera::camera_get_finished_trans_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data) {
  CsiCamera *camera = static_cast<CsiCamera*>(user_data);

  if (camera->streaming_active_) {
    // Signal that a new frame is available
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(camera->frame_ready_semaphore_, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }

  return false;
}

bool CsiCamera::take_snapshot() {
  if (!this->camera_initialized_) {
    ESP_LOGE(TAG, "Camera '%s' not initialized", this->name_.c_str());
    return false;
  }

  ESP_LOGD(TAG, "Taking snapshot with camera '%s'", this->name_.c_str());

  esp_cam_ctlr_trans_t trans = {
    .buffer = this->frame_buffer_,
    .buflen = this->frame_buffer_size_,
  };

  esp_err_t ret = esp_cam_ctlr_receive(this->cam_handle_, &trans, 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Camera '%s' capture failed: %s", this->name_.c_str(), esp_err_to_name(ret));
    return false;
  }

  ESP_LOGD(TAG, "Camera '%s' snapshot taken, size: %zu bytes", this->name_.c_str(), trans.buflen);

  // Synchronize cache
  esp_cache_msync(this->frame_buffer_, this->frame_buffer_size_, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

  // Call callbacks
  this->on_frame_callbacks_.call(static_cast<uint8_t*>(this->frame_buffer_), this->frame_buffer_size_);

  return true;
}

bool CsiCamera::start_streaming() {
  if (!this->camera_initialized_) {
    ESP_LOGE(TAG, "Camera '%s' not initialized for streaming", this->name_.c_str());
    return false;
  }

  if (this->streaming_active_) {
    ESP_LOGW(TAG, "Camera '%s' streaming already active", this->name_.c_str());
    return true;
  }

  ESP_LOGD(TAG, "Starting streaming for camera '%s'", this->name_.c_str());

  this->streaming_should_stop_ = false;
  this->streaming_active_ = true;

  // Creation of the streaming task
  BaseType_t result = xTaskCreate(
    CsiCamera::streaming_task,
    "csi_streaming",
    CSI_STREAMING_STACK_SIZE,
    this,
    5,  // High priority for streaming
    &this->streaming_task_handle_
  );

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create streaming task");
    this->streaming_active_ = false;
    return false;
  }

  ESP_LOGI(TAG, "Camera '%s' streaming started", this->name_.c_str());
  return true;
}

bool CsiCamera::stop_streaming() {
  if (!this->streaming_active_) {
    return true;
  }

  ESP_LOGD(TAG, "Stopping camera '%s' streaming...", this->name_.c_str());

  this->streaming_should_stop_ = true;

  // Wait for task to stop
  if (this->streaming_task_handle_) {
    // Signal to unblock the task if waiting
    xSemaphoreGive(this->frame_ready_semaphore_);

    // Wait for the task to end
    uint32_t timeout = 0;
    while (this->streaming_active_ && timeout < 50) {  // Max 5 seconds
      vTaskDelay(100 / portTICK_PERIOD_MS);
      timeout++;
    }

    if (this->streaming_active_) {
      ESP_LOGW(TAG, "Force stopping streaming task");
      vTaskDelete(this->streaming_task_handle_);
      this->streaming_active_ = false;
    }

    this->streaming_task_handle_ = nullptr;
  }

  ESP_LOGI(TAG, "Camera '%s' streaming stopped", this->name_.c_str());
  return true;
}

void CsiCamera::streaming_task(void *parameter) {
  CsiCamera *camera = static_cast<CsiCamera*>(parameter);
  camera->streaming_loop_();
}

void CsiCamera::streaming_loop_() {
  ESP_LOGD(TAG, "Streaming loop started for camera '%s'", this->name_.c_str());

  esp_cam_ctlr_trans_t trans = {
    .buffer = this->frame_buffer_,
    .buflen = this->frame_buffer_size_,
  };

  while (!this->streaming_should_stop_) {
    // Capture a new frame
    esp_err_t ret = esp_cam_ctlr_receive(this->cam_handle_, &trans, 100 / portTICK_PERIOD_MS);

    if (ret == ESP_OK) {
      // Synchronize cache
      esp_cache_msync(this->frame_buffer_, this->frame_buffer_size_, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

      // Call callbacks
      this->on_frame_callbacks_.call(static_cast<uint8_t*>(this->frame_buffer_), this->frame_buffer_size_);

    } else if (ret != ESP_ERR_TIMEOUT) {
      ESP_LOGW(TAG, "Frame capture failed: %s", esp_err_to_name(ret));
      vTaskDelay(10 / portTICK_PERIOD_MS);  // Short pause on error
    }

    // Small pause to avoid overloading CPU
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }

  this->streaming_active_ = false;
  ESP_LOGD(TAG, "Streaming loop ended for camera '%s'", this->name_.c_str());
  vTaskDelete(nullptr);  // Delete current task
}
#endif

}  // namespace csi_camera
}  // namespace esphome

#endif  // USE_ESP32