#pragma once

#ifdef USE_ESP32

#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/camera/camera.h"
#include "esphome/core/helpers.h"

#ifdef USE_I2C
#include "esphome/components/i2c/i2c_bus.h"
#endif  // USE_I2C

namespace esphome {
namespace esp32_camera {

class ESP32Camera;

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
  ESP32_CAMERA_SIZE_1920X1080,  // FHD
  ESP32_CAMERA_SIZE_720X1280,   // PHD
  ESP32_CAMERA_SIZE_864X1536,   // P3MP
  ESP32_CAMERA_SIZE_2048X1536,  // QXGA
  ESP32_CAMERA_SIZE_2560X1440,  // QHD
  ESP32_CAMERA_SIZE_2560X1600,  // WQXGA
  ESP32_CAMERA_SIZE_1080X1920,  // PFHD
  ESP32_CAMERA_SIZE_2560X1920,  // QSXGA
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
class ESP32CameraImage : public camera::CameraImage {
 public:
  ESP32CameraImage(camera_fb_t *buffer, uint8_t requester);
  camera_fb_t *get_raw_buffer();
  uint8_t *get_data_buffer() override;
  size_t get_data_length() override;
  bool was_requested_by(camera::CameraRequester requester) const override;

 protected:
  camera_fb_t *buffer_;
  uint8_t requesters_;
};

struct CameraImageData {
  uint8_t *data;
  size_t length;
};

/* ---------------- CameraImageReader class ---------------- */
class ESP32CameraImageReader : public camera::CameraImageReader {
 public:
  void set_image(std::shared_ptr<camera::CameraImage> image) override;
  size_t available() const override;
  uint8_t *peek_data_buffer() override;
  void consume_data(size_t consumed) override;
  void return_image() override;

 protected:
  std::shared_ptr<ESP32CameraImage> image_;
  size_t offset_{0};
};

/* ---------------- ESP32Camera class ---------------- */
class ESP32Camera : public camera::Camera {
 public:
  ESP32Camera();

  /* setters */
  /* -- pin assignment */
  void set_data_pins(std::array<uint8_t, 8> pins);
  void set_vsync_pin(uint8_t pin);
  void set_href_pin(uint8_t pin);
  void set_pixel_clock_pin(uint8_t pin);
  void set_external_clock(uint8_t pin, uint32_t frequency);
  void set_i2c_pins(uint8_t sda, uint8_t scl);
#ifdef USE_I2C
  void set_i2c_id(i2c::InternalI2CBus *i2c_bus);
#endif  // USE_I2C
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
  /* -- frame buffer */
  void set_frame_buffer_mode(camera_grab_mode_t mode);
  void set_frame_buffer_count(uint8_t fb_count);
  void set_frame_buffer_location(camera_fb_location_t fb_location);

  /* public API (derivated) */
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  /* public API (specific) */
  void start_stream(camera::CameraRequester requester) override;
  void stop_stream(camera::CameraRequester requester) override;
  void request_image(camera::CameraRequester requester) override;
  void update_camera_parameters();

  /// Add a listener to receive camera events
  void add_listener(camera::CameraListener *listener) override { this->listeners_.push_back(listener); }
  camera::CameraImageReader *create_image_reader() override;

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
  std::shared_ptr<ESP32CameraImage> current_image_;
  uint8_t single_requesters_{0};
  uint8_t stream_requesters_{0};
  QueueHandle_t framebuffer_get_queue_;
  QueueHandle_t framebuffer_return_queue_;
  std::vector<camera::CameraListener *> listeners_;

  uint32_t last_idle_request_{0};
  uint32_t last_update_{0};
#if ESPHOME_LOG_LEVEL < ESPHOME_LOG_LEVEL_VERBOSE
  uint32_t last_log_time_{0};
  uint16_t frame_count_{0};
#endif
#ifdef USE_I2C
  i2c::InternalI2CBus *i2c_bus_{nullptr};
#endif  // USE_I2C
};

class ESP32CameraImageTrigger : public Trigger<CameraImageData>, public camera::CameraListener {
 public:
  explicit ESP32CameraImageTrigger(ESP32Camera *parent) { parent->add_listener(this); }
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override {
    CameraImageData camera_image_data{};
    camera_image_data.length = image->get_data_length();
    camera_image_data.data = image->get_data_buffer();
    this->trigger(camera_image_data);
  }
};

class ESP32CameraStreamStartTrigger : public Trigger<>, public camera::CameraListener {
 public:
  explicit ESP32CameraStreamStartTrigger(ESP32Camera *parent) { parent->add_listener(this); }
  void on_stream_start() override { this->trigger(); }
};

class ESP32CameraStreamStopTrigger : public Trigger<>, public camera::CameraListener {
 public:
  explicit ESP32CameraStreamStopTrigger(ESP32Camera *parent) { parent->add_listener(this); }
  void on_stream_stop() override { this->trigger(); }
};

}  // namespace esp32_camera
}  // namespace esphome

#endif
