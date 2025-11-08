#pragma once

#if USE_ESP32_VARIANT_ESP32P4 && USE_CSI_CAMERA_SENSOR

#include "esphome/components/camera/buffer_pool.h"
#include "esphome/components/camera/ram_allocator_cache_aligned.h"
#include "esphome/components/camera/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include "isp.h"
#include "i2c_sccb_adapter.h"

#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_xclk.h"

namespace esphome::camera_sensor {
/// CSI Camera Sensor connected via MIPI-CSI interface.
/// This class represents a camera sensor attached through the MIPI-CSI bus.
/// It works together with the ISP to process raw image data into RGB formats.
/// The CSI host interfaces directly with the camera sensor, and once frames
/// are processed by the ISP, they are transferred to system memory through DMA.
class CSICameraSensor : public camera::Sensor, public i2c::I2CDevice {
 public:
  /// Construct a CSI camera sensor with a given mode and pixel format.
  CSICameraSensor(std::string mode, camera::PixelFormat pixel_format);
  /// Attach the ISP processor instance to the camera sensor.
  void set_isp(ISP *isp) { this->isp_ = isp; }
  /// Configure optional XCLK, PWDN, and RESET control pins.
  void set_pins(int xclk, int pwdn, int reset);
  /// Set the camera clock frequency in Hz.
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  /// Define the number of DMA frame buffers to allocate.
  void set_buffers(uint16_t buffers) { this->buffers_ = buffers; }
  /// Set factory default horizontal flip. Ensures user flip_x setting behaves
  /// correctly by compensating for sensors that are pre-flipped from factory.
  void set_factory_flip_x(bool factory_flip_x) { this->factory_flip_x_ = factory_flip_x; }
  /// Enable or disable horizontal flip at runtime.
  void set_flip_x(bool flip_x) {
    this->flip_x_ = flip_x;
    this->set_flip_x_ = true;
  }
  /// Enable or disable vertical flip at runtime.
  void set_flip_y(bool flip_y) {
    this->flip_y_ = flip_y;
    this->set_flip_y_ = true;
  }
  /// Enable or disable byte swapping in the image data stream.
  void set_byte_swap(bool byte_swap) { this->byte_swap_ = byte_swap; }
  /// Enable or disable the camera sensor test pattern mode.
  void set_test_pattern(bool test_pattern) {
    this->test_pattern_ = test_pattern;
    this->set_test_pattern_ = true;
  }
  /// Set sensor gain. Increase brightness, may add noise.
  void set_gain(uint32_t gain) {
    this->gain_ = gain;
    this->set_gain_ = true;
  }
  /// Set sensor exposure time. Increase brightness, may add blur.
  void set_exposure(uint32_t exposure) {
    this->exposure_ = exposure;
    this->set_exposure_ = true;
  }
  // -------- Sensor --------
  bool configure() override;
  camera::Buffer *acquire_frame_buffer() override;
  void return_frame_buffer(camera::Buffer *buffer) override;
  camera::Resolution get_resolution() override { return this->image_spec_; }
  camera::ImageFormat get_image_format() override { return camera::IMAGE_FORMAT_RAW; }
  std::optional<camera::PixelFormat> get_pixel_format() override { return this->image_spec_.format; }
  void log_config() override;
  // -------------------------

 protected:
  static bool dma_start_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
  static bool dma_complete_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
  bool dma_start_(esp_cam_ctlr_trans_t *trans);
  bool dma_complete_(esp_cam_ctlr_trans_t *trans);
  cam_ctlr_color_t to_internal_(camera::PixelFormat format);
  cam_ctlr_color_t to_internal_(esp_cam_sensor_output_format_t format);

  /// CSI DMA transactions have no user data and operate on void*. Instead of mapping void* directly to
  /// camera pipeline buffers, the InternalBuffer stores the allocated frame and passes it through the pipeline.
  class InternalBuffer : public camera::Buffer {
   public:
    // -------- Buffer --------
    uint8_t *get_data() const override { return this->data_buffer_; }
    size_t get_size() const override { return this->data_length_; }
    // ------------------------
    uint8_t *data_buffer_{};
    size_t data_length_{};
  };

  bool byte_swap_{};
  bool factory_flip_x_{};
  bool flip_x_{};
  bool flip_y_{};
  bool test_pattern_{};
  bool set_flip_x_{};
  bool set_flip_y_{};
  bool set_test_pattern_{};
  bool set_gain_{};
  bool set_exposure_{};
  uint16_t buffers_{};
  int reset_pin_{};
  int pwdn_pin_{};
  int xclk_pin_{};
  size_t frame_buffer_size_{};
  uint32_t frequency_{};
  uint32_t gain_{};
  uint32_t exposure_{};
  QueueHandle_t produced_{};
  QueueHandle_t consumed_{};
  I2CSCCBAdapter i2c_adapter_;
  ISP *isp_{};
  std::string mode_{};
  camera::CameraImageSpec image_spec_;
  esp_cam_sensor_device_t *sensor_{};
  esp_cam_ctlr_handle_t cam_ctrl_handle_{};
  esp_cam_ctlr_evt_cbs_t cam_ctlr_evt_cbs_{};
  esp_cam_sensor_xclk_handle_t xclk_handle_{};
  esp_cam_sensor_format_t *format_{};
  camera::BufferPool<InternalBuffer> pool_;
  camera::RAMAllocatorCacheAligned<uint8_t> allocator_;
};

}  // namespace esphome::camera_sensor

#endif
