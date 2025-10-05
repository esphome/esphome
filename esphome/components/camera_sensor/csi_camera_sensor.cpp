#ifdef USE_CSI_CAMERA_SENSOR

#include "csi_camera_sensor.h"

#include "driver/isp.h"
#include "esp_cache.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"

namespace esphome::camera_sensor {

static const char *const TAG = "camera_sensor";

bool IRAM_ATTR CSICameraSensor::get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans,
                                              void *user_data) {
  return reinterpret_cast<CSICameraSensor *>(user_data)->init_transaction(trans);
}

bool IRAM_ATTR CSICameraSensor::trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans,
                                               void *user_data) {
  return reinterpret_cast<CSICameraSensor *>(user_data)->finished_transaction(trans);
}

bool IRAM_ATTR CSICameraSensor::init_transaction(esp_cam_ctlr_trans_t *trans) {
  if (xQueueReceiveFromISR(this->consumed_, &trans->buffer, NULL) != pdPASS)
    return false;

  trans->buflen = this->frame_buffer_size_;
  return true;
}

bool IRAM_ATTR CSICameraSensor::finished_transaction(esp_cam_ctlr_trans_t *trans) {
  if (xQueueSendFromISR(this->produced_, &trans->buffer, NULL) != pdPASS)
    xQueueSendFromISR(this->consumed_, &trans->buffer, NULL);

  return true;
}

CSICameraSensor::CSICameraSensor(uint16_t width, uint16_t height, camera::PixelFormat pixel_format)
    : i2c_adapter_(this), image_spec_(width, height, pixel_format) {
  this->format_ = NULL;
}

void CSICameraSensor::number_brightness(float value) {
  this->brightness_ = static_cast<int8_t>(value);
  this->color_configure_(true);
}

void CSICameraSensor::number_contrast(float value) {
  this->contrast_ = static_cast<uint8_t>(value);
  this->color_configure_(true);
}

void CSICameraSensor::number_exposure(float value) {
  this->exposure_ = static_cast<uint8_t>(value);
  this->exposure_configure();
}

void CSICameraSensor::number_filter(float value) {
  this->filter_ = static_cast<uint8_t>(value);
  this->filter_configure_(true);
}

void CSICameraSensor::number_hue(float value) {
  this->hue_ = static_cast<uint16_t>(value);
  this->color_configure_(true);
}

void CSICameraSensor::number_saturation(float value) {
  this->saturation_ = static_cast<uint8_t>(value);
  this->color_configure_(true);
}

void CSICameraSensor::set_pins(int xclk, int pwdn, int reset) {
  this->reset_pin_ = reset;
  this->pwdn_pin_ = pwdn;
  this->xclk_pin_ = xclk;
}

bool CSICameraSensor::configure() {
  this->produced_ = xQueueCreate(this->buffers_, sizeof(void *));
  this->consumed_ = xQueueCreate(this->buffers_, sizeof(void *));

  esp_ldo_channel_config_t ldo_channel_config = {
      .chan_id = 3,
      .voltage_mv = 2500,
  };

  if (esp_ldo_acquire_channel(&ldo_channel_config, &this->ldo_mipi_phy_) != ESP_OK) {
    ESP_LOGE(TAG, "esp_ldo_acquire_channel failed.");
    return false;
  }

  bool probe = get_i2c_address() == 0x00;
  for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start;
       p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
    if (p->port != ESP_CAM_SENSOR_MIPI_CSI)
      continue;
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

  if (this->sensor_ == NULL) {
    ESP_LOGE(TAG, "No CSI camera sensor found (missing CONFIC_CAMERA_* option?).");
    return false;
  }

  // Set sensor format
  esp_cam_sensor_format_array_t sensor_format_array = {0};
  esp_cam_sensor_query_format(this->sensor_, &sensor_format_array);
  const esp_cam_sensor_format_t *sensor_format = sensor_format_array.format_array;
  for (int i = 0; i < sensor_format_array.count; i++) {
    if (this->image_spec_.width == sensor_format[i].width && this->image_spec_.height == sensor_format[i].height) {
      this->format_ = (esp_cam_sensor_format_t *) &sensor_format[i];
      break;
    }
  }

  if (this->format_ == NULL) {
    ESP_LOGE(TAG, "Invalid resolution. Options:");
    for (int i = 0; i < sensor_format_array.count; i++) {
      ESP_LOGE(TAG, "  %s", sensor_format[i].name);
    }
    return false;
  }

  if (esp_cam_sensor_set_format(this->sensor_, this->format_) != ESP_OK) {
    ESP_LOGE(TAG, "Format set fail");
    return false;
  }

  int flip_y = this->flip_y_ ? 1 : 0;
  if (esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_VFLIP, &flip_y, sizeof(flip_y)) != ESP_OK)
    ESP_LOGE(TAG, "ESP_CAM_SENSOR_VFLIP failed.");

  int flip_x = this->flip_x_ ? 1 : 0;
  if (esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_HMIRROR, &flip_x, sizeof(flip_x)) != ESP_OK)
    ESP_LOGE(TAG, "ESP_CAM_SENSOR_HMIRROR failed.");

  // CSI setup
  esp_cam_ctlr_csi_config_t csi_config = {};
  csi_config.ctlr_id = 0;
  csi_config.clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT;
  csi_config.h_res = this->format_->height;
  csi_config.v_res = this->format_->width;
  csi_config.data_lane_num = this->format_->mipi_info.lane_num;
  csi_config.lane_bit_rate_mbps = this->format_->mipi_info.mipi_clk / 1000000;
  if (this->format_->isp_info) {
    csi_config.input_data_color_type = CAM_CTLR_COLOR_RAW8;
    csi_config.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  } else {
    csi_config.input_data_color_type = CAM_CTLR_COLOR_RAW8;
    csi_config.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  }
  csi_config.queue_items = 1;
  csi_config.byte_swap_en = this->byte_swap_;
  csi_config.bk_buffer_dis = false;

  if (esp_cam_new_csi_ctlr(&csi_config, &this->cam_ctrl_handle_) != ESP_OK) {
    ESP_LOGE(TAG, "esp_cam_new_csi_ctlr failed");
    return false;
  }

  this->cam_ctlr_evt_cbs_.on_get_new_trans = get_new_trans;
  this->cam_ctlr_evt_cbs_.on_trans_finished = trans_finished;
  if (esp_cam_ctlr_register_event_callbacks(this->cam_ctrl_handle_, &this->cam_ctlr_evt_cbs_, this) != ESP_OK) {
    ESP_LOGE(TAG, "esp_cam_ctlr_register_event_callbacks failed.");
    return false;
  }

  // Init framebuffers
  this->pool_.init(this->buffers_, [] { return new CSICameraSensorBuffer(); });
  this->frame_buffer_size_ = this->image_spec_.bytes_per_image();
  size_t frame_buffer_alignment = 64;
  for (int i = 0; i < this->buffers_; ++i) {
    void *frame_buffer = heap_caps_aligned_calloc(frame_buffer_alignment, 1, this->frame_buffer_size_,
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
    if (!frame_buffer)
      return false;

    if (xQueueSendToBack(this->consumed_, &frame_buffer, 0) != pdPASS) {
      ESP_LOGE(TAG, "Buffer allocation failed. %i", i);
      return false;
    }
  }

  if (esp_cam_ctlr_enable(this->cam_ctrl_handle_) != ESP_OK) {
    ESP_LOGE(TAG, "esp_cam_ctlr_enable failed.");
    return false;
  }

  if (esp_cam_ctlr_start(this->cam_ctrl_handle_) != ESP_OK) {
    ESP_LOGE(TAG, "esp_cam_ctlr_start FAILED");
    return false;
  }

  // Start Image Signal Processor
  if (this->format_->isp_info) {
    esp_isp_processor_cfg_t isp_config = {};
    isp_config.clk_src = ISP_CLK_SRC_DEFAULT;
    isp_config.clk_hz = 80000000;
    isp_config.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
    isp_config.input_data_color_type = ISP_COLOR_RAW8;
    isp_config.output_data_color_type = ISP_COLOR_RGB565;
    isp_config.yuv_range = ISP_COLOR_RANGE_LIMIT;
    isp_config.yuv_std = ISP_YUV_CONV_STD_BT601;
    isp_config.has_line_start_packet = this->format_->mipi_info.line_sync_en;
    isp_config.has_line_end_packet = this->format_->mipi_info.line_sync_en;
    isp_config.h_res = this->format_->width;
    isp_config.v_res = this->format_->height;
    isp_config.bayer_order = bayer_to_raw_(this->format_->isp_info->isp_v1_info.bayer_type);
    isp_config.intr_priority = 1;
    // isp_config.flags.bypass_isp = 0;

    if (esp_isp_new_processor(&isp_config, &this->isp_proc_handle_) != ESP_OK) {
      ESP_LOGE(TAG, "esp_isp_new_processor failed.");
      return false;
    }

    this->filter_configure_(true);
    this->color_configure_(true);

    if (esp_isp_enable(this->isp_proc_handle_) != ESP_OK) {
      ESP_LOGE(TAG, "esp_isp_enable failed.");
      return false;
    }
  }

  this->exposure_configure();

  int flip = this->flip_y_ ? 1 : 0;
  if (esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_VFLIP, &flip, sizeof(flip)) != ESP_OK) {
    ESP_LOGE(TAG, "esp_cam_sensor_set_para_value  ESP_CAM_SENSOR_VFLIP failed.");
  }

  int mirror = this->flip_x_ ? 1 : 0;
  if (esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_HMIRROR, &mirror, sizeof(mirror)) != ESP_OK) {
    ESP_LOGE(TAG, "esp_cam_sensor_set_para_value  ESP_CAM_SENSOR_HMIRROR failed.");
  }

  start_stream_();

#ifdef USE_NUMBER
  if (this->brightness_number_)
    this->brightness_number_->publish_state(this->brightness_);
  if (this->contrast_number_)
    this->contrast_number_->publish_state(this->contrast_);
  if (this->exposure_number_)
    this->exposure_number_->publish_state(this->exposure_);
  if (this->filter_number_)
    this->filter_number_->publish_state(this->filter_);
  if (this->hue_number_)
    this->hue_number_->publish_state(this->hue_);
  if (this->saturation_number_)
    this->saturation_number_->publish_state(this->saturation_);
#endif

  return true;
}

camera::Buffer *CSICameraSensor::acquire_frame_buffer() {
  if (uxQueueMessagesWaiting(this->produced_) <= 0)
    return nullptr;

  CSICameraSensorBuffer *buffer = this->pool_.acquire();
  if (!buffer)
    return nullptr;

  assert(xQueueReceive(this->produced_, &buffer->data_buffer_, 0) == pdPASS);
  if (esp_cache_msync(buffer->data_buffer_, this->frame_buffer_size_, ESP_CACHE_MSYNC_FLAG_DIR_M2C) != ESP_OK)
    ESP_LOGW(TAG, "ESP_CACHE_MSYNC_FLAG_DIR_M2C failed.");

  buffer->data_length_ = this->frame_buffer_size_;
  return buffer;
}

void CSICameraSensor::return_frame_buffer(camera::Buffer *buffer) {
  CSICameraSensorBuffer *b = reinterpret_cast<CSICameraSensorBuffer *>(buffer);
  assert(xQueueSendToBack(this->consumed_, &b->data_buffer_, 0) == pdPASS);
  this->pool_.release(b);
}

void CSICameraSensor::log_config() {
  ESP_LOGCONFIG(TAG,
                "Camera Sensor: %s\n"
                "  %s\n"
                "  Flip X: %s, Y: %s\n"
                "  Buffers: %u\n"
                "  MIPI-CSI:\n"
                "    Clock: %d MHz\n"
                "    Lanes: %d\n"
                "    Line Sync: %s\n"
                "  ISP: %s\n",
                this->sensor_->name, this->format_->name, YESNO(this->flip_x_), YESNO(this->flip_y_), this->buffers_,
                this->format_->mipi_info.mipi_clk / 1000000, this->format_->mipi_info.lane_num,
                YESNO(this->format_->mipi_info.line_sync_en), YESNO(this->format_->isp_info));
  LOG_I2C_DEVICE(this);
}

void CSICameraSensor::start_stream_() {
  int enable = 1;
  if (esp_cam_sensor_ioctl(this->sensor_, ESP_CAM_SENSOR_IOC_S_STREAM, &enable) != ESP_OK)
    ESP_LOGE(TAG, "ESP_CAM_SENSOR_IOC_S_STREAM start failed.");
}

void CSICameraSensor::stop_stream_() {
  int disable = 0;
  if (esp_cam_sensor_ioctl(this->sensor_, ESP_CAM_SENSOR_IOC_S_STREAM, &disable) != ESP_OK)
    ESP_LOGE(TAG, "ESP_CAM_SENSOR_IOC_S_STREAM stop failed.");
}

color_raw_element_order_t CSICameraSensor::bayer_to_raw_(esp_cam_sensor_bayer_pattern_t pattern) {
  static const color_raw_element_order_t table[4][4] = {
      {COLOR_RAW_ELEMENT_ORDER_RGGB, COLOR_RAW_ELEMENT_ORDER_GRBG, COLOR_RAW_ELEMENT_ORDER_GBRG,
       COLOR_RAW_ELEMENT_ORDER_BGGR},
      {COLOR_RAW_ELEMENT_ORDER_GRBG, COLOR_RAW_ELEMENT_ORDER_RGGB, COLOR_RAW_ELEMENT_ORDER_BGGR,
       COLOR_RAW_ELEMENT_ORDER_GBRG},
      {COLOR_RAW_ELEMENT_ORDER_GBRG, COLOR_RAW_ELEMENT_ORDER_BGGR, COLOR_RAW_ELEMENT_ORDER_RGGB,
       COLOR_RAW_ELEMENT_ORDER_GRBG},
      {COLOR_RAW_ELEMENT_ORDER_BGGR, COLOR_RAW_ELEMENT_ORDER_GBRG, COLOR_RAW_ELEMENT_ORDER_GRBG,
       COLOR_RAW_ELEMENT_ORDER_RGGB},
  };

  if (pattern > 4) {
    ESP_LOGE(TAG, "Bayer mapping is not up to date.");
    return COLOR_RAW_ELEMENT_ORDER_BGGR;
  }

  int variation = (this->flip_x_ ? 0 : 1) | (this->flip_y_ ? 2 : 0);
  return table[pattern][variation];
}

void CSICameraSensor::color_configure_(bool enable) {
  esp_isp_color_config_t config = {
      .color_contrast =
          {
              .decimal = this->contrast_ < 128 ? this->contrast_ : static_cast<uint32_t>(0),
              .integer = this->contrast_ >= 128 ? 1 : static_cast<uint32_t>(0),
          },
      .color_saturation =
          {
              .decimal = this->saturation_ < 128 ? this->saturation_ : static_cast<uint32_t>(0),
              .integer = this->saturation_ >= 128 ? 1 : static_cast<uint32_t>(0),
          },
      .color_hue = this->hue_,
      .color_brightness = this->brightness_,
  };

  if (esp_isp_color_configure(this->isp_proc_handle_, &config) != ESP_OK)
    ESP_LOGE(TAG, "esp_isp_color_configure failed.");

  esp_isp_color_enable(this->isp_proc_handle_);
}

void CSICameraSensor::exposure_configure() {
  int exposure = this->exposure_;
  if (esp_cam_sensor_set_para_value(this->sensor_, ESP_CAM_SENSOR_EXPOSURE_VAL, &exposure, sizeof(exposure)) != ESP_OK)
    ESP_LOGE(TAG, "ESP_CAM_SENSOR_EXPOSURE_VAL failed.");
}

void CSICameraSensor::filter_configure_(bool enable) {
  esp_isp_bf_config_t config = {
      .bf_template =
          {
              {1, 2, 1},
              {2, 4, 2},
              {1, 2, 1},
          },
      .denoising_level = this->filter_,
  };

  esp_isp_bf_disable(this->isp_proc_handle_);
  if (esp_isp_bf_configure(this->isp_proc_handle_, &config) != ESP_OK)
    ESP_LOGE(TAG, "esp_isp_bf_configure failed.");

  if (esp_isp_bf_enable(this->isp_proc_handle_) != ESP_OK)
    ESP_LOGE(TAG, "esp_isp_bf_enable failed.");
}

}  // namespace esphome::camera_sensor

#endif
