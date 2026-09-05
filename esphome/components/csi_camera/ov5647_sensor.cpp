#include "esphome/core/defines.h"
#if defined(USE_CSI_CAMERA) && defined(USE_ESP32) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "ov5647_sensor.h"
#include "esphome/core/log.h"

#include <driver/gpio.h>
#include <esp_cam_sensor.h>
#include <esp_cam_sensor_detect.h>
#include <esp_err.h>

#include <cinttypes>

namespace esphome::csi_camera {

static const char *const TAG = "csi_camera";
static constexpr uint32_t HERTZ_PER_MEGAHERTZ = 1000000;
static constexpr uint8_t SENSOR_STREAM_DISABLE = 0;
static constexpr uint8_t SENSOR_STREAM_ENABLE = 1;
static constexpr uint32_t OV5647_MODE_SELECT_REG = 0x0100;
static constexpr uint32_t OV5647_MODE_SELECT_STREAMING_MASK = 1U;
static constexpr uint32_t OV5647_AEC_CTRL00_REG = 0x3A00;
static constexpr uint32_t OV5647_AEC_CTRL00_NIGHT_MODE_MASK = 1U << 2U;
static constexpr uint32_t OV5647_TIMING_TC_REG20 = 0x3820;
static constexpr uint32_t OV5647_TIMING_TC_REG21 = 0x3821;
static constexpr uint32_t OV5647_ORIENTATION_MASK = 1U << 1U;

static CsiBayerOrder csi_bayer_order_from_sensor_isp(esp_cam_sensor_bayer_pattern_t pattern) {
  switch (pattern) {
    case ESP_CAM_SENSOR_BAYER_RGGB:
      return CsiBayerOrder::CSI_BAYER_ORDER_RGGB;
    case ESP_CAM_SENSOR_BAYER_GRBG:
      return CsiBayerOrder::CSI_BAYER_ORDER_GRBG;
    case ESP_CAM_SENSOR_BAYER_GBRG:
      return CsiBayerOrder::CSI_BAYER_ORDER_GBRG;
    case ESP_CAM_SENSOR_BAYER_BGGR:
      return CsiBayerOrder::CSI_BAYER_ORDER_BGGR;
    case ESP_CAM_SENSOR_BAYER_MONO:
      break;
  }
  return CsiBayerOrder::CSI_BAYER_ORDER_GBRG;
}

template<typename T>
static void apply_sensor_parameter(esp_cam_sensor_device_t *sensor, uint32_t id, const char *name,
                                   const std::optional<T> &configured_value) {
  if (!configured_value.has_value()) {
    return;
  }
  int value = static_cast<int>(*configured_value);
  const esp_err_t error = esp_cam_sensor_set_para_value(sensor, id, &value, sizeof(value));
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "Sensor control %s failed: %s", name, esp_err_to_name(error));
  }
}

Ov5647Sensor::~Ov5647Sensor() { this->reset(); }

bool Ov5647Sensor::setup(const Ov5647SensorConfig &config, CsiSensorSetup *setup) {
  if (config.sccb_adapter == nullptr || setup == nullptr) {
    return false;
  }
  this->reset();
  this->night_mode_ = config.night_mode;
  if (!Ov5647Sensor::supports_format(config.format)) {
    ESP_LOGE(TAG, "OV5647 does not support requested CSI format %" PRIu16 "x%" PRIu16 "@%" PRIu16 "fps %s",
             config.format.width, config.format.height, config.format.fps,
             csi_raw_format_to_string(config.format.raw_format));
    return false;
  }

  esp_cam_sensor_device_t *sensor = nullptr;
  for (esp_cam_sensor_detect_fn_t *detect_fn = &__esp_cam_sensor_detect_fn_array_start;
       detect_fn < &__esp_cam_sensor_detect_fn_array_end; ++detect_fn) {
    if (detect_fn->port != ESP_CAM_SENSOR_MIPI_CSI || detect_fn->sccb_addr != config.i2c_address) {
      continue;
    }
    esp_cam_sensor_config_t sensor_config = {};
    sensor_config.sccb_handle = config.sccb_adapter;
    sensor_config.reset_pin = GPIO_NUM_NC;
    sensor_config.pwdn_pin = static_cast<gpio_num_t>(config.power_down_pin);
    sensor_config.sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    sensor = (*(detect_fn->detect))(&sensor_config);
    if (sensor != nullptr) {
      break;
    }
  }
  if (sensor == nullptr) {
    ESP_LOGE(TAG, "No OV5647 MIPI sensor detected at address 0x%02X", config.i2c_address);
    return false;
  }
  this->sensor_ = sensor;
  ESP_LOGI(TAG, "Detected sensor: %s", sensor->name);

  esp_cam_sensor_format_array_t available_formats = {};
  if (esp_cam_sensor_query_format(sensor, &available_formats) != ESP_OK) {
    ESP_LOGE(TAG, "Sensor format query failed");
    return false;
  }
  const esp_cam_sensor_output_format_t requested_pixel_format =
      config.format.raw_format == CsiRawFormat::CSI_RAW_FORMAT_RAW8 ? ESP_CAM_SENSOR_PIXFORMAT_RAW8
                                                                    : ESP_CAM_SENSOR_PIXFORMAT_RAW10;
  const esp_cam_sensor_format_t *selected_format = nullptr;
  for (size_t index = 0; index < available_formats.count; index++) {
    const auto &candidate = available_formats.format_array[index];
    ESP_LOGI(TAG, "  [%zu] %s", index, candidate.name);
    if (candidate.width == config.format.width && candidate.height == config.format.height &&
        candidate.fps == config.format.fps && candidate.format == requested_pixel_format) {
      selected_format = &candidate;
    }
  }
  if (selected_format == nullptr || selected_format->isp_info == nullptr) {
    ESP_LOGE(TAG, "Requested OV5647 format not available: %" PRIu16 "x%" PRIu16 "@%" PRIu16 "fps %s",
             config.format.width, config.format.height, config.format.fps,
             csi_raw_format_to_string(config.format.raw_format));
    return false;
  }
  if (esp_cam_sensor_set_format(sensor, selected_format) != ESP_OK) {
    ESP_LOGE(TAG, "set_format failed");
    return false;
  }

  setup->format = {selected_format->width, selected_format->height, selected_format->fps, config.format.raw_format};
  setup->pixel_clock_hz = selected_format->isp_info->isp_v1_info.pclk;
  setup->lane_bit_rate_mbps = selected_format->mipi_info.mipi_clk / HERTZ_PER_MEGAHERTZ;
  setup->lane_count = selected_format->mipi_info.lane_num;
  setup->uses_line_sync = selected_format->mipi_info.line_sync_en;
  setup->bayer_order = csi_bayer_order_from_sensor_isp(selected_format->isp_info->isp_v1_info.bayer_type);

  const bool default_mirror_ok =
      this->read_register_bit_(OV5647_TIMING_TC_REG21, OV5647_ORIENTATION_MASK, &setup->default_horizontal_mirror);
  const bool default_flip_ok =
      this->read_register_bit_(OV5647_TIMING_TC_REG20, OV5647_ORIENTATION_MASK, &setup->default_vertical_flip);

  this->apply_controls_(config);

  const bool final_mirror_ok =
      this->read_register_bit_(OV5647_TIMING_TC_REG21, OV5647_ORIENTATION_MASK, &setup->final_horizontal_mirror);
  const bool final_flip_ok =
      this->read_register_bit_(OV5647_TIMING_TC_REG20, OV5647_ORIENTATION_MASK, &setup->final_vertical_flip);
  setup->orientation_state_valid = default_mirror_ok && default_flip_ok && final_mirror_ok && final_flip_ok;

  // Some sensor controls may temporarily restart the OV5647. Keep the sensor
  // stopped until the CSI and ISP pipeline is ready to accept data.
  if (!this->ensure_stream_stopped_()) {
    return false;
  }
  if (!setup->orientation_state_valid && (config.horizontal_mirror.has_value() || config.vertical_flip.has_value())) {
    ESP_LOGW(TAG, "Unable to read OV5647 orientation registers; Bayer auto cannot compensate orientation changes");
  }

  ESP_LOGI(TAG, "Format set: %s → %" PRIu16 "x%" PRIu16 "@%" PRIu16 "fps %s", selected_format->name,
           setup->format.width, setup->format.height, setup->format.fps,
           csi_raw_format_to_string(setup->format.raw_format));
  return true;
}

bool Ov5647Sensor::ensure_stream_stopped_() {
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(this->sensor_);
  if (sensor == nullptr) {
    return false;
  }

  int stream_enable = SENSOR_STREAM_DISABLE;
  const esp_err_t stop_error = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &stream_enable);
  if (stop_error != ESP_OK) {
    ESP_LOGE(TAG, "Explicit sensor stream stop failed: %s", esp_err_to_name(stop_error));
    return false;
  }
  this->streaming_ = false;

  esp_cam_sensor_reg_val_t mode_select = {.regaddr = OV5647_MODE_SELECT_REG, .value = 0};
  const esp_err_t read_error = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_G_REG, &mode_select);
  if (read_error != ESP_OK) {
    ESP_LOGW(TAG, "Unable to verify OV5647 stream state: %s", esp_err_to_name(read_error));
    return true;
  }

  const bool streaming = (mode_select.value & OV5647_MODE_SELECT_STREAMING_MASK) != 0;
  ESP_LOGI(TAG, "OV5647 stream guard: reg 0x0100=0x%02" PRIx32 " streaming=%s", mode_select.value,
           streaming ? "YES" : "NO");
  if (streaming) {
    ESP_LOGE(TAG, "OV5647 remained streaming after explicit stream stop");
    return false;
  }
  return true;
}

bool Ov5647Sensor::start_stream() {
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(this->sensor_);
  int stream_enable = SENSOR_STREAM_ENABLE;
  if (sensor == nullptr || esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &stream_enable) != ESP_OK) {
    ESP_LOGE(TAG, "Sensor stream start failed");
    return false;
  }
  this->streaming_ = true;
  ESP_LOGI(TAG, "Sensor streaming started");
  return true;
}

void Ov5647Sensor::stop_stream() {
  if (!this->streaming_) {
    return;
  }
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(this->sensor_);
  if (sensor != nullptr) {
    int stream_enable = 0;
    (void) esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &stream_enable);
  }
  this->streaming_ = false;
}

void Ov5647Sensor::reset() {
  this->stop_stream();
  if (this->sensor_ != nullptr) {
    (void) esp_cam_sensor_del_dev(static_cast<esp_cam_sensor_device_t *>(this->sensor_));
    this->sensor_ = nullptr;
  }
}

void Ov5647Sensor::set_night_mode(bool enabled) {
  this->night_mode_ = enabled;
  this->apply_night_mode_();
}

bool Ov5647Sensor::read_register_bit_(uint32_t reg, uint32_t mask, bool *enabled) const {
  if (this->sensor_ == nullptr || enabled == nullptr) {
    return false;
  }
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(this->sensor_);
  esp_cam_sensor_reg_val_t value = {.regaddr = reg, .value = 0};
  const esp_err_t error = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_G_REG, &value);
  if (error != ESP_OK) {
    return false;
  }
  *enabled = (value.value & mask) != 0;
  return true;
}

bool Ov5647Sensor::update_register_bit_(uint32_t reg, uint32_t mask, bool enabled, const char *name) {
  if (this->sensor_ == nullptr) {
    return false;
  }
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(this->sensor_);
  esp_cam_sensor_reg_val_t value = {.regaddr = reg, .value = 0};
  esp_err_t error = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_G_REG, &value);
  if (error != ESP_OK) {
    return false;
  }
  value.value = enabled ? value.value | mask : value.value & ~mask;
  error = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_S_REG, &value);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "%s failed: %s", name, esp_err_to_name(error));
  }
  return error == ESP_OK;
}

void Ov5647Sensor::apply_controls_(const Ov5647SensorConfig &config) {
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(this->sensor_);
  if (sensor == nullptr) {
    return;
  }
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_HMIRROR, "horizontal_mirror", config.horizontal_mirror);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_VFLIP, "vertical_flip", config.vertical_flip);
  if (config.test_pattern) {
    int enabled = 1;
    const esp_err_t error = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_S_TEST_PATTERN, &enabled);
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "test_pattern failed: %s", esp_err_to_name(error));
    }
  }
  if (config.wb_mode.has_value()) {
    const std::optional<int> automatic{*config.wb_mode == 0 ? 1 : 0};
    apply_sensor_parameter(sensor, ESP_CAM_SENSOR_AWB, "wb_mode:auto", automatic);
  }
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_WB, "wb_mode", config.wb_mode);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_AE_CONTROL, "aec_mode", config.aec_enabled);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_AE_LEVEL, "ae_level", config.ae_level);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_AGC, "agc_mode", config.agc_enabled);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_SHARPNESS, "sharpness", config.sharpness);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_DENOISE, "denoise", config.denoise);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_DPC, "dead_pixel_correction", config.dead_pixel_correction);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_BLC, "black_level_correction", config.black_level_correction);
  apply_sensor_parameter(sensor, ESP_CAM_SENSOR_LENC, "lens_shading_correction", config.lens_shading_correction);
  this->apply_night_mode_();
}

void Ov5647Sensor::apply_night_mode_() {
  if (!this->night_mode_.has_value() || this->sensor_ == nullptr) {
    return;
  }
  (void) this->update_register_bit_(OV5647_AEC_CTRL00_REG, OV5647_AEC_CTRL00_NIGHT_MODE_MASK, *this->night_mode_,
                                    "night_mode");
}

}  // namespace esphome::csi_camera
#endif
