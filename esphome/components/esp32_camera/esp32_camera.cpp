#ifdef USE_ESP32

#include "esp32_camera.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <freertos/task.h>

namespace esphome {
namespace esp32_camera {

static const char *const TAG = "esp32_camera";

void ESP32Camera::setup() {
  global_esp32_camera = this;

  this->last_update_ = millis();
  esp_err_t err = esp_camera_init(&this->config_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    this->init_error_ = err;
    this->mark_failed();
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, this->vertical_flip_);
  s->set_hmirror(s, this->horizontal_mirror_);
  s->set_contrast(s, this->contrast_);
  s->set_brightness(s, this->brightness_);
  s->set_saturation(s, this->saturation_);
  s->set_colorbar(s, this->test_pattern_);
  this->framebuffer_get_queue_ = xQueueCreate(1, sizeof(camera_fb_t *));
  this->framebuffer_return_queue_ = xQueueCreate(1, sizeof(camera_fb_t *));
  xTaskCreatePinnedToCore(&ESP32Camera::framebuffer_task,
                          "framebuffer_task",  // name
                          1024,                // stack size
                          nullptr,             // task pv params
                          0,                   // priority
                          nullptr,             // handle
                          1                    // core
  );
}
void ESP32Camera::dump_config() {
  auto conf = this->config_;
  ESP_LOGCONFIG(TAG, "ESP32 Camera:");
  ESP_LOGCONFIG(TAG, "  Name: %s", this->name_.c_str());
  ESP_LOGCONFIG(TAG, "  Internal: %s", YESNO(this->internal_));
#ifdef USE_ARDUINO
  ESP_LOGCONFIG(TAG, "  Board Has PSRAM: %s", YESNO(psramFound()));
#endif  // USE_ARDUINO
  ESP_LOGCONFIG(TAG, "  Data Pins: D0:%d D1:%d D2:%d D3:%d D4:%d D5:%d D6:%d D7:%d", conf.pin_d0, conf.pin_d1,
                conf.pin_d2, conf.pin_d3, conf.pin_d4, conf.pin_d5, conf.pin_d6, conf.pin_d7);
  ESP_LOGCONFIG(TAG, "  VSYNC Pin: %d", conf.pin_vsync);
  ESP_LOGCONFIG(TAG, "  HREF Pin: %d", conf.pin_href);
  ESP_LOGCONFIG(TAG, "  Pixel Clock Pin: %d", conf.pin_pclk);
  ESP_LOGCONFIG(TAG, "  External Clock: Pin:%d Frequency:%u", conf.pin_xclk, conf.xclk_freq_hz);
  ESP_LOGCONFIG(TAG, "  I2C Pins: SDA:%d SCL:%d", conf.pin_sscb_sda, conf.pin_sscb_scl);
  ESP_LOGCONFIG(TAG, "  Reset Pin: %d", conf.pin_reset);
  switch (this->config_.frame_size) {
    case FRAMESIZE_QQVGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 160x120 (QQVGA)");
      break;
    case FRAMESIZE_QCIF:
      ESP_LOGCONFIG(TAG, "  Resolution: 176x155 (QCIF)");
      break;
    case FRAMESIZE_HQVGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 240x176 (HQVGA)");
      break;
    case FRAMESIZE_QVGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 320x240 (QVGA)");
      break;
    case FRAMESIZE_CIF:
      ESP_LOGCONFIG(TAG, "  Resolution: 400x296 (CIF)");
      break;
    case FRAMESIZE_VGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 640x480 (VGA)");
      break;
    case FRAMESIZE_SVGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 800x600 (SVGA)");
      break;
    case FRAMESIZE_XGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 1024x768 (XGA)");
      break;
    case FRAMESIZE_SXGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 1280x1024 (SXGA)");
      break;
    case FRAMESIZE_UXGA:
      ESP_LOGCONFIG(TAG, "  Resolution: 1600x1200 (UXGA)");
      break;
    default:
      break;
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup Failed: %s", esp_err_to_name(this->init_error_));
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  auto st = s->status;
  ESP_LOGCONFIG(TAG, "  JPEG Quality: %u", st.quality);
  // ESP_LOGCONFIG(TAG, "  Framebuffer Count: %u", conf.fb_count);
  ESP_LOGCONFIG(TAG, "  Contrast: %d", st.contrast);
  ESP_LOGCONFIG(TAG, "  Brightness: %d", st.brightness);
  ESP_LOGCONFIG(TAG, "  Saturation: %d", st.saturation);
  ESP_LOGCONFIG(TAG, "  Vertical Flip: %s", ONOFF(st.vflip));
  ESP_LOGCONFIG(TAG, "  Horizontal Mirror: %s", ONOFF(st.hmirror));
  // ESP_LOGCONFIG(TAG, "  Special Effect: %u", st.special_effect);
  // ESP_LOGCONFIG(TAG, "  White Balance Mode: %u", st.wb_mode);
  // ESP_LOGCONFIG(TAG, "  Auto White Balance: %u", st.awb);
  // ESP_LOGCONFIG(TAG, "  Auto White Balance Gain: %u", st.awb_gain);
  // ESP_LOGCONFIG(TAG, "  Auto Exposure Control: %u", st.aec);
  // ESP_LOGCONFIG(TAG, "  Auto Exposure Control 2: %u", st.aec2);
  // ESP_LOGCONFIG(TAG, "  Auto Exposure Level: %d", st.ae_level);
  // ESP_LOGCONFIG(TAG, "  Auto Exposure Value: %u", st.aec_value);
  // ESP_LOGCONFIG(TAG, "  AGC: %u", st.agc);
  // ESP_LOGCONFIG(TAG, "  AGC Gain: %u", st.agc_gain);
  // ESP_LOGCONFIG(TAG, "  Gain Ceiling: %u", st.gainceiling);
  // ESP_LOGCONFIG(TAG, "  BPC: %u", st.bpc);
  // ESP_LOGCONFIG(TAG, "  WPC: %u", st.wpc);
  // ESP_LOGCONFIG(TAG, "  RAW_GMA: %u", st.raw_gma);
  // ESP_LOGCONFIG(TAG, "  Lens Correction: %u", st.lenc);
  // ESP_LOGCONFIG(TAG, "  DCW: %u", st.dcw);
  ESP_LOGCONFIG(TAG, "  Test Pattern: %s", YESNO(st.colorbar));
}
void ESP32Camera::loop() {
  // check if we can return the image
  if (this->can_return_image_()) {
    // return image
    auto *fb = this->current_image_->get_raw_buffer();
    xQueueSend(this->framebuffer_return_queue_, &fb, portMAX_DELAY);
    this->current_image_.reset();
  }

  // Check if we should fetch a new image
  if (!this->has_requested_image_())
    return;
  if (this->current_image_.use_count() > 1) {
    // image is still in use
    return;
  }
  const uint32_t now = millis();
  if (now - this->last_update_ <= this->max_update_interval_)
    return;

  // request new image
  camera_fb_t *fb;
  if (xQueueReceive(this->framebuffer_get_queue_, &fb, 0L) != pdTRUE) {
    // no frame ready
    ESP_LOGVV(TAG, "No frame ready");
    return;
  }

  if (fb == nullptr) {
    ESP_LOGW(TAG, "Got invalid frame from camera!");
    xQueueSend(this->framebuffer_return_queue_, &fb, portMAX_DELAY);
    return;
  }
  this->current_image_ = std::make_shared<CameraImage>(fb);

  ESP_LOGD(TAG, "Got Image: len=%u", fb->len);
  this->new_image_callback_.call(this->current_image_);
  this->last_update_ = now;
  this->single_requester_ = false;
}
void ESP32Camera::framebuffer_task(void *pv) {
  while (true) {
    camera_fb_t *framebuffer = esp_camera_fb_get();
    xQueueSend(global_esp32_camera->framebuffer_get_queue_, &framebuffer, portMAX_DELAY);
    // return is no-op for config with 1 fb
    xQueueReceive(global_esp32_camera->framebuffer_return_queue_, &framebuffer, portMAX_DELAY);
    esp_camera_fb_return(framebuffer);
  }
}
ESP32Camera::ESP32Camera(const std::string &name) : EntityBase(name) {
  this->config_.pin_pwdn = -1;
  this->config_.pin_reset = -1;
  this->config_.pin_xclk = -1;
  this->config_.ledc_timer = LEDC_TIMER_0;
  this->config_.ledc_channel = LEDC_CHANNEL_0;
  this->config_.pixel_format = PIXFORMAT_JPEG;
  this->config_.frame_size = FRAMESIZE_VGA;  // 640x480
  this->config_.jpeg_quality = 10;
  this->config_.fb_count = 1;

  global_esp32_camera = this;
}
ESP32Camera::ESP32Camera() : ESP32Camera("") {}
void ESP32Camera::set_data_pins(std::array<uint8_t, 8> pins) {
  this->config_.pin_d0 = pins[0];
  this->config_.pin_d1 = pins[1];
  this->config_.pin_d2 = pins[2];
  this->config_.pin_d3 = pins[3];
  this->config_.pin_d4 = pins[4];
  this->config_.pin_d5 = pins[5];
  this->config_.pin_d6 = pins[6];
  this->config_.pin_d7 = pins[7];
}
void ESP32Camera::set_vsync_pin(uint8_t pin) { this->config_.pin_vsync = pin; }
void ESP32Camera::set_href_pin(uint8_t pin) { this->config_.pin_href = pin; }
void ESP32Camera::set_pixel_clock_pin(uint8_t pin) { this->config_.pin_pclk = pin; }
void ESP32Camera::set_external_clock(uint8_t pin, uint32_t frequency) {
  this->config_.pin_xclk = pin;
  this->config_.xclk_freq_hz = frequency;
}
void ESP32Camera::set_i2c_pins(uint8_t sda, uint8_t scl) {
  this->config_.pin_sscb_sda = sda;
  this->config_.pin_sscb_scl = scl;
}
void ESP32Camera::set_frame_size(ESP32CameraFrameSize size) {
  switch (size) {
    case ESP32_CAMERA_SIZE_160X120:
      this->config_.frame_size = FRAMESIZE_QQVGA;
      break;
    case ESP32_CAMERA_SIZE_176X144:
      this->config_.frame_size = FRAMESIZE_QCIF;
      break;
    case ESP32_CAMERA_SIZE_240X176:
      this->config_.frame_size = FRAMESIZE_HQVGA;
      break;
    case ESP32_CAMERA_SIZE_320X240:
      this->config_.frame_size = FRAMESIZE_QVGA;
      break;
    case ESP32_CAMERA_SIZE_400X296:
      this->config_.frame_size = FRAMESIZE_CIF;
      break;
    case ESP32_CAMERA_SIZE_640X480:
      this->config_.frame_size = FRAMESIZE_VGA;
      break;
    case ESP32_CAMERA_SIZE_800X600:
      this->config_.frame_size = FRAMESIZE_SVGA;
      break;
    case ESP32_CAMERA_SIZE_1024X768:
      this->config_.frame_size = FRAMESIZE_XGA;
      break;
    case ESP32_CAMERA_SIZE_1280X1024:
      this->config_.frame_size = FRAMESIZE_SXGA;
      break;
    case ESP32_CAMERA_SIZE_1600X1200:
      this->config_.frame_size = FRAMESIZE_UXGA;
      break;
  }
}
void ESP32Camera::set_jpeg_quality(uint8_t quality) { this->config_.jpeg_quality = quality; }
void ESP32Camera::set_reset_pin(uint8_t pin) { this->config_.pin_reset = pin; }
void ESP32Camera::set_power_down_pin(uint8_t pin) { this->config_.pin_pwdn = pin; }
void ESP32Camera::add_image_callback(std::function<void(std::shared_ptr<CameraImage>)> &&f) {
  this->new_image_callback_.add(std::move(f));
}
void ESP32Camera::set_vertical_flip(bool vertical_flip) { this->vertical_flip_ = vertical_flip; }
void ESP32Camera::set_horizontal_mirror(bool horizontal_mirror) { this->horizontal_mirror_ = horizontal_mirror; }
void ESP32Camera::set_contrast(int contrast) { this->contrast_ = contrast; }
void ESP32Camera::set_brightness(int brightness) { this->brightness_ = brightness; }
void ESP32Camera::set_saturation(int saturation) { this->saturation_ = saturation; }
float ESP32Camera::get_setup_priority() const { return setup_priority::DATA; }
uint32_t ESP32Camera::hash_base() { return 3010542557UL; }
void ESP32Camera::request_image() { this->single_requester_ = true; }
void ESP32Camera::request_stream() { this->last_stream_request_ = millis(); }
bool ESP32Camera::has_requested_image_() const {
  if (this->single_requester_)
    // single request
    return true;

  uint32_t now = millis();
  if (now - this->last_stream_request_ < 5000)
    // stream request
    return true;

  if (this->idle_update_interval_ != 0 && now - this->last_update_ > this->idle_update_interval_)
    // idle update
    return true;

  return false;
}
bool ESP32Camera::can_return_image_() const { return this->current_image_.use_count() == 1; }
void ESP32Camera::set_max_update_interval(uint32_t max_update_interval) {
  this->max_update_interval_ = max_update_interval;
}
void ESP32Camera::set_idle_update_interval(uint32_t idle_update_interval) {
  this->idle_update_interval_ = idle_update_interval;
}
void ESP32Camera::set_test_pattern(bool test_pattern) { this->test_pattern_ = test_pattern; }

ESP32Camera *global_esp32_camera;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void CameraImageReader::set_image(std::shared_ptr<CameraImage> image) {
  this->image_ = std::move(image);
  this->offset_ = 0;
}
size_t CameraImageReader::available() const {
  if (!this->image_)
    return 0;

  return this->image_->get_data_length() - this->offset_;
}
void CameraImageReader::return_image() { this->image_.reset(); }
void CameraImageReader::consume_data(size_t consumed) { this->offset_ += consumed; }
uint8_t *CameraImageReader::peek_data_buffer() { return this->image_->get_data_buffer() + this->offset_; }

camera_fb_t *CameraImage::get_raw_buffer() { return this->buffer_; }
uint8_t *CameraImage::get_data_buffer() { return this->buffer_->buf; }
size_t CameraImage::get_data_length() { return this->buffer_->len; }
CameraImage::CameraImage(camera_fb_t *buffer) : buffer_(buffer) {}

}  // namespace esp32_camera
}  // namespace esphome

#endif
