#pragma once

#ifdef USE_CSI_CAMERA_SENSOR
#include "esphome/components/camera/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include "i2c_sccb_adapter.h"

#include "driver/isp_core.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_sensor.h"
#include "esp_sccb_io_interface.h"
#include "esp_ldo_regulator.h"

namespace esphome::camera_sensor {

class CSICameraSensorBuffer : public camera::Buffer {
 public:
  // -------- Buffer --------
  uint8_t *get_data_buffer() override { return this->data_buffer_; }
  size_t get_data_length() override { return this->data_length_; }
  // ------------------------
  uint8_t *data_buffer_;
  size_t data_length_;
};

class CSICameraSensor : public camera::Sensor, public i2c::I2CDevice {
 public:
  CSICameraSensor(camera::CameraImageSpec *spec);
  void set_pins(int xclk, int pwdn, int reset);
  void set_framebuffers(int framebuffers) { this->framebuffers_ = framebuffers; }
  void set_byte_swap(bool byte_swap) { this->byte_swap_ = byte_swap; }
  // -------- Sensor --------
  camera::SensorError capture_pixels() override;
  camera::Buffer *get_image_buffer() override;
  camera::CameraImageSpec *get_image_spec() override { return this->image_spec_; }
  bool camera_sensor_setup() override;
  void camera_sensor_dump_config() override;
  // -------------------------
  bool init_transaction(esp_cam_ctlr_trans_t *trans);
  bool finished_transaction(esp_cam_ctlr_trans_t *trans);

 protected:
  static bool get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
  static bool trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
  void start_stream_();
  void stop_stream_();

  QueueHandle_t produced_{};
  QueueHandle_t consumed_{};
  size_t frame_buffer_size_{};
  I2CSCCBAdapter i2c_adapter_;
  camera::CameraImageSpec *image_spec_{};
  int framebuffers_{};
  int reset_pin_{};
  int pwdn_pin_{};
  int xclk_pin_{};
  bool byte_swap_{};
  CSICameraSensorBuffer buffer_{};
  esp_ldo_channel_handle_t ldo_mipi_phy_{};
  esp_cam_sensor_device_t *sensor_{};
  esp_cam_ctlr_handle_t cam_ctrl_handle_{};
  esp_cam_ctlr_evt_cbs_t cam_ctlr_evt_cbs_{};
  esp_cam_sensor_format_t *format_{};
  isp_proc_handle_t isp_proc_handle_{};
};

}  // namespace esphome::camera_sensor

#endif
