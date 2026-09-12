#pragma once
#if defined(USE_CSI_CAMERA) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "csi_types.h"
#include "esphome/components/camera_video/camera_source.h"
#include "esphome/components/esp_ldo/esp_ldo.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "i2c_sccb_adapter.h"
#include "ov5647_sensor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <driver/isp.h>
#include <esp_cam_ctlr.h>
#include <esp_cam_ctlr_csi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <optional>

namespace esphome::csi_camera {

class CsiVideoFrame final : public camera_video::CameraVideoFrame {
 public:
  uint8_t *get_data_buffer() override { return this->data_.load(std::memory_order_acquire); }
  size_t get_data_length() override { return this->capacity_; }
  uint64_t get_timestamp_us() const override { return this->timestamp_us_; }
  ~CsiVideoFrame() override = default;

 protected:
  void on_last_reference() override;

 private:
  friend class CsiCamera;
  bool allocate(size_t capacity, bool *dma_capable);
  bool try_acquire();
  void release_unpublished();
  camera_video::FrameRef<camera_video::CameraVideoFrame> commit(uint64_t timestamp_us);
  void release_storage();
  void free_storage_();

  std::atomic<uint8_t *> data_{nullptr};
  size_t capacity_{0};
  uint64_t timestamp_us_{0};
  std::atomic<bool> acquired_{false};
  std::atomic<bool> release_pending_{false};
};

class CsiCamera final : public Component, public i2c::I2CDevice, public camera_video::CameraVideoSource {
 public:
  void set_power_down_pin(int pin) { this->power_down_pin_ = pin; }
  void set_power_supply(esp_ldo::EspLdo *power_supply) { this->power_supply_ = power_supply; }
  void set_frame_buffer_count(int count) { this->frame_buffer_count_ = count; }
  void set_format(uint16_t width, uint16_t height, uint16_t fps, CsiRawFormat raw_format) {
    this->sensor_config_.format = {width, height, fps, raw_format};
  }
  void set_brightness(int brightness) { this->brightness_ = brightness; }
  void set_contrast(float contrast) { this->contrast_ = contrast; }
  void set_saturation(float saturation) { this->saturation_ = saturation; }
  void set_vertical_flip(bool enabled) { this->sensor_config_.vertical_flip = enabled; }
  void set_horizontal_mirror(bool enabled) { this->sensor_config_.horizontal_mirror = enabled; }
  void set_bayer_order_auto() { this->bayer_order_auto_ = true; }
  void set_bayer_order(CsiBayerOrder order) {
    this->bayer_order_auto_ = false;
    this->bayer_order_ = order;
  }
  void set_ccm(float rr, float rg, float rb, float gr, float gg, float gb, float br, float bg, float bb) {
    this->ccm_ = std::array<float, 9>{rr, rg, rb, gr, gg, gb, br, bg, bb};
  }
  void set_hue(uint32_t hue) { this->hue_ = hue; }
  void set_test_pattern(bool enabled) { this->sensor_config_.test_pattern = enabled; }
  void set_wb_mode(int mode) { this->sensor_config_.wb_mode = mode; }
  void set_aec_mode(bool enabled) { this->sensor_config_.aec_enabled = enabled; }
  void set_ae_level(int level) { this->sensor_config_.ae_level = level; }
  void set_agc_mode(bool enabled) { this->sensor_config_.agc_enabled = enabled; }
  void set_sharpness(int value) { this->sensor_config_.sharpness = value; }
  void set_denoise(int value) { this->sensor_config_.denoise = value; }
  void set_dead_pixel_correction(bool enabled) { this->sensor_config_.dead_pixel_correction = enabled; }
  void set_black_level_correction(bool enabled) { this->sensor_config_.black_level_correction = enabled; }
  void set_lens_shading_correction(bool enabled) { this->sensor_config_.lens_shading_correction = enabled; }
  void set_night_mode(bool enabled);

  void setup() override;
  void dump_config() override;
  void on_shutdown() override;
  bool teardown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  bool get_video_frame_spec(camera_video::CameraVideoFrameSpec *spec) const override {
    if (spec == nullptr || !this->video_source_ready_.load(std::memory_order_acquire))
      return false;
    *spec = this->video_frame_spec_;
    return spec->is_complete();
  }
  bool start_stream(camera_video::CameraVideoSourceListener *listener) override;
  void stop_stream(camera_video::CameraVideoSourceListener *listener) override;

 protected:
  static constexpr size_t MAX_CAMERA_LISTENERS = 4;
  static constexpr size_t MAX_FRAME_BUFFERS = 4;

  void setup_power_and_sensor_();
  void setup_buffers_();
  void setup_isp_();
  void start_pipeline_();
  void fail_setup_(const char *message);
  bool setup_sensor_(CsiSensorSetup &setup);
  bool allocate_frame_buffers_();
  void release_frame_buffers_();
  void request_capture_task_stop_();
  void wake_capture_task_();
  void cleanup_hardware_();
  bool configure_isp_(const CsiSensorSetup &setup, CsiBayerOrder bayer_order);
  bool configure_demosaic_(CsiBayerOrder bayer_order);
  void configure_ccm_();
  void configure_color_();
  bool configure_csi_controller_(const CsiSensorSetup &setup);
  void capture_task_body_();
  static void capture_task_entry(void *arg);
  static bool IRAM_ATTR dma_start(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *, void *);
  static bool IRAM_ATTR dma_complete(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *, void *);
  bool dma_start_cb_(esp_cam_ctlr_trans_t *trans);
  bool dma_complete_cb_(esp_cam_ctlr_trans_t *trans);

  int power_down_pin_{-1};
  esp_ldo::EspLdo *power_supply_{nullptr};
  int frame_buffer_count_{4};
  int brightness_{0};
  float contrast_{1.0f};
  float saturation_{1.5f};
  bool bayer_order_auto_{true};
  CsiBayerOrder bayer_order_{CsiBayerOrder::CSI_BAYER_ORDER_GBRG};
  std::optional<std::array<float, 9>> ccm_;
  uint32_t hue_{0};

  I2CSCCBAdapter i2c_adapter_{this};
  esp_cam_ctlr_handle_t cam_handle_{nullptr};
  isp_proc_handle_t isp_proc_{nullptr};
  Ov5647Sensor sensor_;
  Ov5647SensorConfig sensor_config_{};
  CsiSensorSetup sensor_setup_{};
  CsiBayerOrder effective_bayer_order_{CsiBayerOrder::CSI_BAYER_ORDER_GBRG};

  std::array<CsiVideoFrame, MAX_FRAME_BUFFERS> frame_slots_{};
  std::atomic<CsiVideoFrame *> ready_frame_{nullptr};
  camera_video::CameraVideoFrameSpec video_frame_spec_{};
  std::atomic<bool> video_source_ready_{false};
  mutable Mutex listeners_mutex_;
  StaticVector<camera_video::CameraVideoSourceListener *, MAX_CAMERA_LISTENERS> listeners_;
  std::atomic<bool> capture_task_stop_{false};
  std::atomic<bool> capture_task_exited_{true};
  std::atomic<TaskHandle_t> capture_task_handle_{nullptr};
  std::atomic<bool> shutdown_started_{false};
  bool controller_started_{false};
  bool controller_enabled_{false};
  bool isp_enabled_{false};
  std::atomic<uint32_t> dropped_frames_{0};
  std::atomic<uint32_t> delivered_frames_{0};
};

template<typename... Ts> class CsiCameraSetNightModeAction final : public Action<Ts...> {
 public:
  explicit CsiCameraSetNightModeAction(CsiCamera *camera) : camera_(camera) {}
  TEMPLATABLE_VALUE(bool, night_mode)
  void play(Ts... x) override { this->camera_->set_night_mode(this->night_mode_.value(x...)); }

 protected:
  CsiCamera *camera_;
};

}  // namespace esphome::csi_camera
#endif
