#if USE_ESP32_VARIANT_ESP32P4 && USE_CSI_CAMERA_SENSOR

#include "csi_camera_sensor.h"

#include "esp_cache.h"
#include "esp_cam_sensor_detect.h"

namespace esphome::camera_sensor {

static const char *const TAG = "camera_sensor";

CSICameraSensor::CSICameraSensor(std::string mode, camera::PixelFormat pixel_format) : i2c_adapter_(this) {
  this->image_spec_.format = pixel_format;
  this->mode_ = mode;
  this->format_ = NULL;
}

void CSICameraSensor::set_pins(int xclk, int pwdn, int reset) {
  this->reset_pin_ = reset;
  this->pwdn_pin_ = pwdn;
  this->xclk_pin_ = xclk;
}

bool CSICameraSensor::configure() {
  this->produced_ = xQueueCreate(this->buffers_, sizeof(void *));
  this->consumed_ = xQueueCreate(this->buffers_, sizeof(void *));

  if (this->xclk_pin_ > 0) {
    esp_cam_sensor_xclk_config_t xclk_config = {.esp_clock_router_cfg = {
                                                    .xclk_pin = gpio_num_t(this->xclk_pin_),
                                                    .xclk_freq_hz = this->frequency_,
                                                }};
    if (esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &this->xclk_handle_) != ESP_OK) {
      ESP_LOGE(TAG, "External clock pin or frequency invalid.");
      return false;
    }

    if (esp_cam_sensor_xclk_start(this->xclk_handle_, &xclk_config) != ESP_OK) {
      ESP_LOGE(TAG, "External clock pin does not supported to output clock signal.");
      return false;
    }
  }

  bool probe = get_i2c_address() == 0x00;
  bool found_sensor = false;
  bool found_mipi_sensor = false;

  for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start;
       p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
    found_sensor = true;
    if (p->port != ESP_CAM_SENSOR_MIPI_CSI)
      continue;

    found_mipi_sensor = true;
    if (!probe && p->sccb_addr != get_i2c_address())
      continue;

    set_i2c_address(p->sccb_addr);
    esp_cam_sensor_config_t sensor_config = {.sccb_handle = &this->i2c_adapter_,
                                             .reset_pin = gpio_num_t(this->reset_pin_),
                                             .pwdn_pin = gpio_num_t(this->pwdn_pin_),
                                             .xclk_pin = gpio_num_t(this->xclk_pin_),
                                             .sensor_port = ESP_CAM_SENSOR_MIPI_CSI};

    this->sensor_ = (*(p->detect))(&sensor_config);
    if (this->sensor_)
      break;
  }

  if (!this->sensor_) {
    if (!found_sensor)
      ESP_LOGE(TAG, "No driver compiled (missing CONFIC_CAMERA_* option)!");
    else if (!found_mipi_sensor)
      ESP_LOGE(TAG, "No MIPI driver found! Compile different driver (CONFIC_CAMERA_* option).");
    else
      ESP_LOGE(TAG, "No MIPI camera detected on I2C bus! Verify GPIO settings.");

    return false;
  }

  // Configure camera sensor
  esp_cam_sensor_format_array_t sensor_format_array = {0};
  esp_cam_sensor_query_format(this->sensor_, &sensor_format_array);
  const esp_cam_sensor_format_t *sensor_format = sensor_format_array.format_array;
  for (int i = 0; i < sensor_format_array.count; i++) {
    if (this->mode_ == sensor_format[i].name) {
      this->format_ = (esp_cam_sensor_format_t *) &sensor_format[i];
      break;
    }
  }

  if (!this->format_) {
    ESP_LOGE(TAG, "Invalid mode %s! Valid modes:", this->mode_.c_str());
    for (int i = 0; i < sensor_format_array.count; i++) {
      ESP_LOGE(TAG, "  %s", sensor_format[i].name);
    }

    return false;
  }

  if (!this->format_->isp_info) {
    ESP_LOGE(TAG, "ISP passthrough not implemented yet.");
    return false;
  }

  this->image_spec_.width = this->format_->width;
  this->image_spec_.height = this->format_->height;
  this->frame_buffer_size_ = this->image_spec_.bytes_per_image();

  if (esp_cam_sensor_set_format(this->sensor_, this->format_) != ESP_OK)
    return false;

  // OV2710 has a bug when writing the x-flip register. Only apply camera settings when explicitly configured.
  int flip_y = this->flip_y_ ? 1 : 0;
  if (this->set_flip_y_ &&
      esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_VFLIP, &flip_y, sizeof(flip_y)) != ESP_OK)
    ESP_LOGE(TAG, "%s has no flip Y support.", this->sensor_->name);

  // Most sensors are factory x-flipped. The user flip flag must therefore be inverted (factory_flip_x_).
  int flip_x = this->flip_x_ ? (this->factory_flip_x_ ? 0 : 1) : (this->factory_flip_x_ ? 1 : 0);
  if (this->set_flip_x_ &&
      esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_HMIRROR, &flip_x, sizeof(flip_x)) != ESP_OK)
    ESP_LOGE(TAG, "%s has no flip x support.", this->sensor_->name);

  int test_pattern = this->test_pattern_ ? 1 : 0;
  if (this->set_test_pattern_ &&
      esp_cam_sensor_ioctl(this->sensor_, ESP_CAM_SENSOR_IOC_S_TEST_PATTERN, &test_pattern_) != ESP_OK)
    ESP_LOGE(TAG, "%s has no test pattern support.", this->sensor_->name);

  if (this->set_gain_ &&
      esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_GAIN, &this->gain_, sizeof(this->gain_)) != ESP_OK)
    ESP_LOGE(TAG, "%s has no sensor gain support.", this->sensor_->name);

  if (this->set_exposure_ && esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_EXPOSURE_VAL, &this->exposure_,
                                                           sizeof(this->exposure_)) != ESP_OK)
    ESP_LOGE(TAG, "%s has no exposure support.", this->sensor_->name);

  // Configure CSI
  esp_cam_ctlr_csi_config_t csi_config = {};
  csi_config.ctlr_id = 0;
  csi_config.clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT;
  csi_config.h_res = this->format_->width;
  csi_config.v_res = this->format_->height;
  csi_config.data_lane_num = this->format_->mipi_info.lane_num;
  csi_config.lane_bit_rate_mbps = this->format_->mipi_info.mipi_clk / 1000000;
  csi_config.input_data_color_type = this->to_internal_(this->format_->format);
  csi_config.output_data_color_type = this->to_internal_(this->image_spec_.format);
  csi_config.queue_items = 1;
  csi_config.byte_swap_en = this->byte_swap_;
  csi_config.bk_buffer_dis = false;

  esp_err_t err = esp_cam_new_csi_ctlr(&csi_config, &this->cam_ctrl_handle_);
  if (err == ESP_ERR_NO_MEM)
    ESP_LOGE(TAG, "Not enough memory. Is PSRAM enabled ?");

  if (err != ESP_OK)
    return false;

  // Setup DMA callbacks
  this->cam_ctlr_evt_cbs_.on_get_new_trans = dma_start_callback;
  this->cam_ctlr_evt_cbs_.on_trans_finished = dma_complete_callback;
  if (esp_cam_ctlr_register_event_callbacks(this->cam_ctrl_handle_, &this->cam_ctlr_evt_cbs_, this) != ESP_OK)
    return false;

  // Init framebuffers
  this->pool_.init(this->buffers_, [] { return new InternalBuffer(); });
  this->allocator_.set_caps(MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_8BIT);
  for (int i = 0; i < this->buffers_; ++i) {
    void *frame_buffer = this->allocator_.allocate(this->frame_buffer_size_);
    if (!frame_buffer) {
      ESP_LOGE(TAG, "Not enough memory to allocate frame buffers.");
      return false;
    }

    if (xQueueSendToBack(this->consumed_, &frame_buffer, 0) != pdPASS)
      return false;
  }

  if (esp_cam_ctlr_enable(this->cam_ctrl_handle_) != ESP_OK)
    return false;

  if (esp_cam_ctlr_start(this->cam_ctrl_handle_) != ESP_OK)
    return false;

  if (!this->isp_->configure_isp(this->format_, this->image_spec_.format, this->flip_x_, this->flip_y_)) {
    ESP_LOGE(TAG, "ISP configuration failed.");
    return false;
  }

  int enable = 1;
  if (esp_cam_sensor_ioctl(this->sensor_, ESP_CAM_SENSOR_IOC_S_STREAM, &enable) != ESP_OK) {
    ESP_LOGE(TAG, "ESP_CAM_SENSOR_IOC_S_STREAM start failed.");
    return false;
  }

  return true;
}

camera::Buffer *CSICameraSensor::acquire_frame_buffer() {
  if (uxQueueMessagesWaiting(this->produced_) <= 0)
    return nullptr;

  InternalBuffer *buffer = this->pool_.acquire();
  if (!buffer)
    return nullptr;

  if (xQueueReceive(this->produced_, &buffer->data_buffer_, 0) != pdPASS)
    ESP_LOGE(TAG, "xQueueReceive failed.");

  // Cache needs to be invalidated to avoid overlay and other artifacts.
  if (esp_cache_msync(buffer->data_buffer_, this->frame_buffer_size_, ESP_CACHE_MSYNC_FLAG_DIR_M2C) != ESP_OK)
    ESP_LOGW(TAG, "ESP_CACHE_MSYNC_FLAG_DIR_M2C failed.");

  buffer->data_length_ = this->frame_buffer_size_;
  return buffer;
}

void CSICameraSensor::return_frame_buffer(camera::Buffer *buffer) {
  InternalBuffer *b = reinterpret_cast<InternalBuffer *>(buffer);
  if (xQueueSendToBack(this->consumed_, &b->data_buffer_, 0) != pdPASS)
    ESP_LOGE(TAG, "xQueueSendToBack failed.");

  this->pool_.release(b);
}

void CSICameraSensor::log_config() {
  ESP_LOGCONFIG(TAG,
                "Camera Sensor: %s\n"
                "  %s\n"
                "  %s\n"
                "  Flip X: %s, Y: %s, Factory X: %s\n"
                "  Buffers: %u\n"
                "  Test Pattern: %s\n"
                "  MIPI-CSI:\n"
                "    Clock: %d MHz\n"
                "    Lanes: %d\n"
                "    Line Sync: %s\n",
                this->sensor_->name, this->format_->name, to_string(this->image_spec_.format), YESNO(this->flip_x_),
                YESNO(this->flip_y_), YESNO(this->factory_flip_x_), this->buffers_, YESNO(this->test_pattern_),
                this->format_->mipi_info.mipi_clk / 1000000, this->format_->mipi_info.lane_num,
                YESNO(this->format_->mipi_info.line_sync_en));
  LOG_I2C_DEVICE(this);
  this->isp_->log_config();
}

bool IRAM_ATTR CSICameraSensor::dma_start_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans,
                                                   void *user_data) {
  return reinterpret_cast<CSICameraSensor *>(user_data)->dma_start_(trans);
}

bool IRAM_ATTR CSICameraSensor::dma_complete_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans,
                                                      void *user_data) {
  return reinterpret_cast<CSICameraSensor *>(user_data)->dma_complete_(trans);
}

bool IRAM_ATTR CSICameraSensor::dma_start_(esp_cam_ctlr_trans_t *trans) {
  if (xQueueReceiveFromISR(this->consumed_, &trans->buffer, NULL) != pdPASS) {
    // Replace the oldest image for multi buffer mode.
    if (uxQueueMessagesWaiting(this->produced_) <= 1 ||
        xQueueReceiveFromISR(this->produced_, &trans->buffer, NULL) != pdPASS)
      return false;
  }

  trans->buflen = this->frame_buffer_size_;
  return true;
}

bool IRAM_ATTR CSICameraSensor::dma_complete_(esp_cam_ctlr_trans_t *trans) {
  if (xQueueSendFromISR(this->produced_, &trans->buffer, NULL) != pdPASS)
    xQueueSendFromISR(this->consumed_, &trans->buffer, NULL);

  return true;
}

cam_ctlr_color_t CSICameraSensor::to_internal_(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_GRAYSCALE:
      return CAM_CTLR_COLOR_GRAY8;
    case camera::PIXEL_FORMAT_RGB565:
      return CAM_CTLR_COLOR_RGB565;
    case camera::PIXEL_FORMAT_BGR888:
      return CAM_CTLR_COLOR_RGB888;
  }

  return CAM_CTLR_COLOR_GRAY8;
}

cam_ctlr_color_t CSICameraSensor::to_internal_(esp_cam_sensor_output_format_t format) {
  switch (format) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
      return CAM_CTLR_COLOR_RAW8;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
      return CAM_CTLR_COLOR_RAW10;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
      return CAM_CTLR_COLOR_RAW12;
  }

  return CAM_CTLR_COLOR_RAW8;
}

}  // namespace esphome::camera_sensor

#endif
