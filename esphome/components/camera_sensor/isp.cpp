#if USE_ESP32_VARIANT_ESP32P4 && USE_CSI_CAMERA_SENSOR

#include "isp.h"

namespace esphome::camera_sensor {

static const char *const TAG = "camera_sensor";

void ISP::number_brightness(float value) {
  this->brightness_ = static_cast<int8_t>(value);
  this->configure_color_();
}

void ISP::number_contrast(float value) {
  this->contrast_ = static_cast<uint8_t>(value);
  this->configure_color_();
}

void ISP::number_filter(float value) {
  this->filter_ = static_cast<uint8_t>(value);
  this->configure_filter_();
}

void ISP::number_hue(float value) {
  this->hue_ = static_cast<uint16_t>(value);
  this->configure_color_();
}

void ISP::number_saturation(float value) {
  this->saturation_ = static_cast<uint8_t>(value);
  this->configure_color_();
}

void ISP::number_red(float value) {
  this->red_ = value;
  this->configure_color_correction_();
}

void ISP::number_green(float value) {
  this->green_ = value;
  this->configure_color_correction_();
}

void ISP::number_blue(float value) {
  this->blue_ = value;
  this->configure_color_correction_();
}

bool ISP::configure_isp(esp_cam_sensor_format_t *format, camera::PixelFormat pixel_format, bool flip_x, bool flip_y) {
  // Setup bayer pattern
  this->bayer_pattern_reported_ = this->to_internal_(format->isp_info->isp_v1_info.bayer_type);

  // Configure Image Signal Processor
  esp_isp_processor_cfg_t isp_config = {};
  isp_config.clk_src = ISP_CLK_SRC_DEFAULT;
  isp_config.clk_hz = format->isp_info->isp_v1_info.pclk;
  isp_config.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
  isp_config.input_data_color_type = this->to_internal_(format->format);
  isp_config.output_data_color_type = this->to_internal_(pixel_format);
  isp_config.yuv_range = ISP_COLOR_RANGE_FULL;
  isp_config.yuv_std = ISP_YUV_CONV_STD_BT709;
  isp_config.has_line_start_packet = format->mipi_info.line_sync_en;
  isp_config.has_line_end_packet = format->mipi_info.line_sync_en;
  isp_config.h_res = format->width;
  isp_config.v_res = format->height;
  isp_config.bayer_order = bayer_to_isp_(flip_x, flip_y);

  if (esp_isp_new_processor(&isp_config, &this->isp_proc_handle_) != ESP_OK)
    return false;

  this->configure_filter_();
  this->configure_color_();
  this->configure_color_correction_();

  if (esp_isp_enable(this->isp_proc_handle_) != ESP_OK)
    return false;

#ifdef USE_NUMBER
  if (this->brightness_number_)
    this->brightness_number_->publish_state(this->brightness_);
  if (this->contrast_number_)
    this->contrast_number_->publish_state(this->contrast_);
  if (this->filter_number_)
    this->filter_number_->publish_state(this->filter_);
  if (this->hue_number_)
    this->hue_number_->publish_state(this->hue_);
  if (this->saturation_number_)
    this->saturation_number_->publish_state(this->saturation_);
  if (this->red_number_)
    this->red_number_->publish_state(this->red_);
  if (this->green_number_)
    this->green_number_->publish_state(this->green_);
  if (this->blue_number_)
    this->blue_number_->publish_state(this->blue_);
#endif

  return true;
}

void ISP::log_config() {
  ESP_LOGCONFIG(TAG,
                "ISP:\n"
                "  Bayer Pattern:\n"
                "    Reported: %s\n"
                "    In Use: %s\n",
                to_string(this->bayer_pattern_reported_), to_string(this->bayer_pattern_));
}

void ISP::configure_color_() {
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

void ISP::configure_filter_() {
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

void ISP::configure_color_correction_() {
  esp_isp_ccm_config_t config = {.matrix =
                                     {
                                         {this->red_, 0.0f, 0.0f},
                                         {0.0f, this->green_, 0.0f},
                                         {0.0f, 0.0f, this->blue_},
                                     },
                                 .saturation = true};

  esp_isp_ccm_disable(this->isp_proc_handle_);
  if (esp_isp_ccm_configure(this->isp_proc_handle_, &config) != ESP_OK)
    ESP_LOGE(TAG, "esp_isp_ccm_configure failed.");

  if (esp_isp_ccm_enable(this->isp_proc_handle_) != ESP_OK)
    ESP_LOGE(TAG, "esp_isp_ccm_enable failed.");
}

isp_color_t ISP::to_internal_(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_RGB565:
      return ISP_COLOR_RGB565;
    case camera::PIXEL_FORMAT_BGR888:
      return ISP_COLOR_RGB888;
  }

  return ISP_COLOR_RGB565;
}

isp_color_t ISP::to_internal_(esp_cam_sensor_output_format_t format) {
  switch (format) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
      return ISP_COLOR_RAW8;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
      return ISP_COLOR_RAW10;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
      return ISP_COLOR_RAW12;
  }

  return ISP_COLOR_RAW8;
}

BayerPattern ISP::to_internal_(esp_cam_sensor_bayer_pattern_t pattern) {
  switch (pattern) {
    case ESP_CAM_SENSOR_BAYER_RGGB:
      return RGGB;
    case ESP_CAM_SENSOR_BAYER_GRBG:
      return GRBG;
    case ESP_CAM_SENSOR_BAYER_GBRG:
      return GBRG;
    case ESP_CAM_SENSOR_BAYER_BGGR:
      return BGGR;
  }

  return AUTO;
}

color_raw_element_order_t ISP::bayer_to_isp_(bool flip_x, bool flip_y) {
  BayerPattern pattern = this->bayer_pattern_reported_;
  BayerPattern flip_y_pattern[] = {AUTO, GBRG, BGGR, RGGB, GRBG};
  BayerPattern flip_x_pattern[] = {AUTO, GRBG, RGGB, BGGR, GBRG};
  if (flip_y)
    pattern = flip_y_pattern[pattern];

  if (flip_x)
    pattern = flip_x_pattern[pattern];

  if (this->bayer_pattern_ != AUTO)
    pattern = this->bayer_pattern_;

  switch (pattern) {
    case RGGB:
      return COLOR_RAW_ELEMENT_ORDER_RGGB;
    case GRBG:
      return COLOR_RAW_ELEMENT_ORDER_GRBG;
    case GBRG:
      return COLOR_RAW_ELEMENT_ORDER_GBRG;
    case BGGR:
      return COLOR_RAW_ELEMENT_ORDER_BGGR;
    case AUTO:
      break;
  }

  return COLOR_RAW_ELEMENT_ORDER_RGGB;
}

}  // namespace esphome::camera_sensor

#endif
