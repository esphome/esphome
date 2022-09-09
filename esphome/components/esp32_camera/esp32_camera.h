#pragma once

#ifdef USE_ESP32

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace esp32_camera {

class ESP32Camera;

/* ---------------- enum classes ---------------- */
enum CameraRequester { IDLE, API_REQUESTER, WEB_REQUESTER };

enum ESP32CameraFrameSize {
  ESP32_CAMERA_SIZE_160X120,    // QQVGA
  ESP32_CAMERA_SIZE_176X144,    // QCIF
  ESP32_CAMERA_SIZE_240X176,    // HQVGA
  ESP32_CAMERA_SIZE_320X240,    // QVGA
  ESP32_CAMERA_SIZE_400X296,    // CIF
  ESP32_CAMERA_SIZE_640X480,    // VGA
  ESP32_CAMERA_SIZE_800X600,    // SVGA
  ESP32_CAMERA_SIZE_1024X768,   // XGA
  ESP32_CAMERA_SIZE_1280X1024,  // SXGA
  ESP32_CAMERA_SIZE_1600X1200,  // UXGA
};

enum ESP32AgcGainCeiling {
  ESP32_GAINCEILING_2X = GAINCEILING_2X,
  ESP32_GAINCEILING_4X = GAINCEILING_4X,
  ESP32_GAINCEILING_8X = GAINCEILING_8X,
  ESP32_GAINCEILING_16X = GAINCEILING_16X,
  ESP32_GAINCEILING_32X = GAINCEILING_32X,
  ESP32_GAINCEILING_64X = GAINCEILING_64X,
  ESP32_GAINCEILING_128X = GAINCEILING_128X,
};

enum ESP32GainControlMode {
  ESP32_GC_MODE_MANU = false,
  ESP32_GC_MODE_AUTO = true,
};

enum ESP32WhiteBalanceMode {
  ESP32_WB_MODE_AUTO = 0U,
  ESP32_WB_MODE_SUNNY = 1U,
  ESP32_WB_MODE_CLOUDY = 2U,
  ESP32_WB_MODE_OFFICE = 3U,
  ESP32_WB_MODE_HOME = 4U,
};

enum ESP32SpecialEffect {
  ESP32_SPECIAL_EFFECT_NONE = 0U,
  ESP32_SPECIAL_EFFECT_NEGATIVE = 1U,
  ESP32_SPECIAL_EFFECT_GRAYSCALE = 2U,
  ESP32_SPECIAL_EFFECT_RED_TINT = 3U,
  ESP32_SPECIAL_EFFECT_GREEN_TINT = 4U,
  ESP32_SPECIAL_EFFECT_BLUE_TINT = 5U,
  ESP32_SPECIAL_EFFECT_SEPIA = 6U,
};

/* ---------------- CameraImage class ---------------- */
class CameraImage {
 public:
  CameraImage(camera_fb_t *buffer, uint8_t requester);
  camera_fb_t *get_raw_buffer();
  uint8_t *get_data_buffer();
  size_t get_data_length();
  bool was_requested_by(CameraRequester requester) const;

 protected:
  camera_fb_t *buffer_;
  uint8_t requesters_;
};

/* ---------------- CameraImageReader class ---------------- */
class CameraImageReader {
 public:
  void set_image(std::shared_ptr<CameraImage> image);
  size_t available() const;
  uint8_t *peek_data_buffer();
  void consume_data(size_t consumed);
  void return_image();

 protected:
  std::shared_ptr<CameraImage> image_;
  size_t offset_{0};
};

/* ---------------- ESP32Camera class ---------------- */
class ESP32Camera : public Component, public EntityBase {
 public:
  ESP32Camera(const std::string &name);
  ESP32Camera();

  /* setters */
  /* -- pin assignment */
  void set_data_pins(std::array<uint8_t, 8> pins);
  void set_vsync_pin(uint8_t pin);
  void set_href_pin(uint8_t pin);
  void set_pixel_clock_pin(uint8_t pin);
  void set_external_clock(uint8_t pin, uint32_t frequency);
  void set_i2c_pins(uint8_t sda, uint8_t scl);
  void set_reset_pin(uint8_t pin);
  void set_power_down_pin(uint8_t pin);
  /* -- image */
  void set_frame_size(ESP32CameraFrameSize size);
  void set_jpeg_quality(uint8_t quality);
  void set_vertical_flip(bool vertical_flip);
  void set_horizontal_mirror(bool horizontal_mirror);
  void set_contrast(int contrast);
  void set_brightness(int brightness);
  void set_saturation(int saturation);
  void set_special_effect(ESP32SpecialEffect effect);
  /* -- exposure */
  void set_aec_mode(ESP32GainControlMode mode);
  void set_aec2(bool aec2);
  void set_ae_level(int ae_level);
  void set_aec_value(uint32_t aec_value);
  /* -- gains */
  void set_agc_mode(ESP32GainControlMode mode);
  void set_agc_value(uint8_t agc_value);
  void set_agc_gain_ceiling(ESP32AgcGainCeiling gain_ceiling);
  /* -- white balance */
  void set_wb_mode(ESP32WhiteBalanceMode mode);
  /* -- test */
  void set_test_pattern(bool test_pattern);
  /* -- framerates */
  void set_max_update_interval(uint32_t max_update_interval);
  void set_idle_update_interval(uint32_t idle_update_interval);

  /* public API (derivated) */
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  /* public API (specific) */
  void add_image_callback(std::function<void(std::shared_ptr<CameraImage>)> &&f);
  void start_stream(CameraRequester requester);
  void stop_stream(CameraRequester requester);
  void request_image(CameraRequester requester);
  void update_camera_parameters();

  void add_stream_start_callback(std::function<void()> &&callback);
  void add_stream_stop_callback(std::function<void()> &&callback);

 protected:
  /* internal methods */
  bool has_requested_image_() const;
  bool can_return_image_() const;

  static void framebuffer_task(void *pv);

  /* attributes */
  /* camera configuration */
  camera_config_t config_{};
  /* -- image */
  bool vertical_flip_{true};
  bool horizontal_mirror_{true};
  int contrast_{0};
  int brightness_{0};
  int saturation_{0};
  ESP32SpecialEffect special_effect_{ESP32_SPECIAL_EFFECT_NONE};
  /* -- exposure */
  ESP32GainControlMode aec_mode_{ESP32_GC_MODE_AUTO};
  bool aec2_{false};
  int ae_level_{0};
  uint32_t aec_value_{300};
  /* -- gains */
  ESP32GainControlMode agc_mode_{ESP32_GC_MODE_AUTO};
  uint8_t agc_value_{0};
  ESP32AgcGainCeiling agc_gain_ceiling_{ESP32_GAINCEILING_2X};
  /* -- white balance */
  ESP32WhiteBalanceMode wb_mode_{ESP32_WB_MODE_AUTO};
  /* -- Test */
  bool test_pattern_{false};
  /* -- framerates */
  uint32_t max_update_interval_{1000};
  uint32_t idle_update_interval_{15000};

  esp_err_t init_error_{ESP_OK};
  std::shared_ptr<CameraImage> current_image_;
  uint8_t single_requesters_{0};
  uint8_t stream_requesters_{0};
  QueueHandle_t framebuffer_get_queue_;
  QueueHandle_t framebuffer_return_queue_;
  CallbackManager<void(std::shared_ptr<CameraImage>)> new_image_callback_;
  CallbackManager<void()> stream_start_callback_{};
  CallbackManager<void()> stream_stop_callback_{};

  uint32_t last_idle_request_{0};
  uint32_t last_update_{0};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32Camera *global_esp32_camera;

class ESP32CameraStreamStartTrigger : public Trigger<> {
 public:
  explicit ESP32CameraStreamStartTrigger(ESP32Camera *parent) {
    parent->add_stream_start_callback([this]() { this->trigger(); });
  }

 protected:
};
class ESP32CameraStreamStopTrigger : public Trigger<> {
 public:
  explicit ESP32CameraStreamStopTrigger(ESP32Camera *parent) {
    parent->add_stream_stop_callback([this]() { this->trigger(); });
  }

 protected:
};

}  // namespace esp32_camera
}  // namespace esphome

#endif
