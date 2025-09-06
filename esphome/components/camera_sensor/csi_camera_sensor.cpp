#ifdef USE_CSI_CAMERA_SENSOR

#include "csi_camera_sensor.h"

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
    return true;

  trans->buflen = this->frame_buffer_size_;
  return true;
}

bool IRAM_ATTR CSICameraSensor::finished_transaction(esp_cam_ctlr_trans_t *trans) {
  if (xQueueSendFromISR(this->produced_, &trans->buffer, NULL) != pdPASS)
    xQueueSendFromISR(this->consumed_, &trans->buffer, NULL);

  return false;
}

CSICameraSensor::CSICameraSensor(camera::CameraImageSpec *spec) : i2c_adapter_(this) {
  this->image_spec_ = spec;
  this->buffer_.data_buffer_ = NULL;
  this->format_ = NULL;
}

void CSICameraSensor::set_pins(int xclk, int pwdn, int reset) {
  this->reset_pin_ = reset;
  this->pwdn_pin_ = pwdn;
  this->xclk_pin_ = xclk;
}

camera::SensorError CSICameraSensor::capture_pixels() {
  if (uxQueueMessagesWaiting(this->produced_) > 0)
    return camera::SENSOR_ERROR_SUCCESS;

  return camera::SENSOR_ERROR_RETRY;
}

camera::Buffer *CSICameraSensor::get_image_buffer() {
  if (this->buffer_.data_buffer_ != NULL)
    assert(xQueueSendToBack(this->consumed_, &this->buffer_.data_buffer_, 0) == pdPASS);

  assert(xQueueReceive(this->produced_, &this->buffer_.data_buffer_, 0) == pdPASS);
  this->buffer_.data_length_ = this->frame_buffer_size_;
  return &this->buffer_;
}

bool CSICameraSensor::camera_sensor_setup() {
  this->produced_ = xQueueCreate(this->framebuffers_, sizeof(void *));
  this->consumed_ = xQueueCreate(this->framebuffers_, sizeof(void *));

  esp_ldo_channel_config_t ldo_channel_config = {
      .chan_id = 3,
      .voltage_mv = 2500,
  };

  if (esp_ldo_acquire_channel(&ldo_channel_config, &this->ldo_mipi_phy_) != ESP_OK) {
    ESP_LOGE(TAG, "esp_ldo_acquire_channel failed.");
    return false;
  }

  for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start;
       p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
    if (p->port != ESP_CAM_SENSOR_MIPI_CSI)
      continue;

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
    if (this->image_spec_->width == sensor_format[i].width && this->image_spec_->height == sensor_format[i].height) {
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
  this->frame_buffer_size_ = this->image_spec_->bytes_per_image();
  size_t frame_buffer_alignment = 64;
  for (int i = 0; i < this->framebuffers_; ++i) {
    void *frame_buffer = heap_caps_aligned_calloc(frame_buffer_alignment, 1, this->frame_buffer_size_,
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
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
    isp_config.bayer_order = COLOR_RAW_ELEMENT_ORDER_BGGR;
    isp_config.intr_priority = 0;
    // isp_config.flags.bypass_isp = 0;

    if (esp_isp_new_processor(&isp_config, &this->isp_proc_handle_) != ESP_OK) {
      ESP_LOGE(TAG, "esp_isp_new_processor failed.");
      return false;
    }

    if (esp_isp_enable(this->isp_proc_handle_) != ESP_OK) {
      ESP_LOGE(TAG, "esp_isp_enable failed.");
      return false;
    }
  }

  start_stream_();

  return true;
}

void CSICameraSensor::camera_sensor_dump_config() {
  ESP_LOGCONFIG(TAG,
                "CSI Camera Sensor: %s\n"
                "  %s\n"
                "  MIPI CSI:\n"
                "    Clock: %d MHz\n"
                "    Lanes: %d\n"
                "    Line Sync: %s\n"
                "  ISP: %s\n",
                this->sensor_->name, this->format_->name, this->format_->mipi_info.mipi_clk / 1000000,
                this->format_->mipi_info.lane_num, YESNO(this->format_->mipi_info.line_sync_en),
                YESNO(this->format_->isp_info));
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

}  // namespace esphome::camera_sensor

#endif
