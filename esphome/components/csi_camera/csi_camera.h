#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_ESP32

// Vérification spécifique pour ESP32-P4
#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET) && CONFIG_IDF_TARGET_ESP32P4
#define HAS_ESP32_P4_CAMERA 1
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_ctlr.h"
#include "driver/isp.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#endif

namespace esphome {
namespace tab5_camera {

class Tab5Camera : public Component, public i2c::I2CDevice {
 public:
  Tab5Camera() = default;
  ~Tab5Camera();

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // Configuration
  void set_name(const std::string &name) { this->name_ = name; }
  void set_external_clock_pin(uint8_t pin) { this->external_clock_pin_ = pin; }
  void set_external_clock_frequency(uint32_t freq) { this->external_clock_frequency_ = freq; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_sensor_address(uint8_t address) { this->sensor_address_ = address; this->set_i2c_address(address); }

  // Getters
  const std::string &get_name() const { return this->name_; }

  // Fonctions de capture
  bool take_snapshot();
  
  // Fonctions de streaming
  bool start_streaming();
  bool stop_streaming();

#ifdef HAS_ESP32_P4_CAMERA
  bool is_streaming() const { return this->streaming_active_; }
  uint8_t* get_frame_buffer() const { return static_cast<uint8_t*>(this->frame_buffer_); }
  size_t get_frame_buffer_size() const { return this->frame_buffer_size_; }
#else
  bool is_streaming() const { return false; }
  uint8_t* get_frame_buffer() const { return nullptr; }
  size_t get_frame_buffer_size() const { return 0; }
#endif

  // Callbacks pour le streaming
  void add_on_frame_callback(std::function<void(uint8_t*, size_t)> &&callback) {
    this->on_frame_callbacks_.add(std::move(callback));
  }

 protected:
#ifdef HAS_ESP32_P4_CAMERA
  bool init_camera_();
  bool init_sensor_();
  bool init_ldo_();
  void deinit_camera_();
  
  // Callbacks statiques pour le contrôleur de caméra
  static bool camera_get_new_vb_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
  static bool camera_get_finished_trans_callback(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
  
  // Tâche de streaming
  static void streaming_task(void *parameter);
  void streaming_loop_();
  
  // Variables ESP32-P4
  esp_cam_ctlr_handle_t cam_handle_{nullptr};
  isp_proc_handle_t isp_proc_{nullptr};
  esp_ldo_channel_handle_t ldo_mipi_phy_{nullptr};
  i2c_master_bus_handle_t i2c_bus_handle_{nullptr};
  void *frame_buffer_{nullptr};
  size_t frame_buffer_size_{0};
  bool camera_initialized_{false};
  bool sensor_initialized_{false};
  bool ldo_initialized_{false};
  
  // Variables de streaming
  TaskHandle_t streaming_task_handle_{nullptr};
  SemaphoreHandle_t frame_ready_semaphore_{nullptr};
  QueueHandle_t frame_queue_{nullptr};
  bool streaming_active_{false};
  bool streaming_should_stop_{false};
  
  // Structure pour les frames en queue
  struct FrameData {
    void* buffer;
    size_t size;
    uint32_t timestamp;
  };
#endif

  // Configuration
  std::string name_{"Tab5 Camera"};
  uint8_t external_clock_pin_{0};
  uint32_t external_clock_frequency_{20000000};  // 20MHz par défaut
  uint8_t sensor_address_{0x24};  // Adresse I2C par défaut du capteur
  GPIOPin *reset_pin_{nullptr};

  // Callbacks
  CallbackManager<void(uint8_t*, size_t)> on_frame_callbacks_;
};

}  // namespace tab5_camera
}  // namespace esphome

#endif  // USE_ESP32







