/**
 * @file transcoder.cpp
 * @brief Implementation of centralized hardware media codec management
 */

#include "transcoder.h"

namespace esphome {
namespace transcoder {

static const char *const TAG = "transcoder";

// Global transcoder instance
Transcoder *global_transcoder = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void Transcoder::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Transcoder...");

  // Initialize JPEG Decoder
#ifdef USE_HARDWARE_JPEG_DECODER
  // ESP32-P4: Hardware JPEG decoder
  jpeg_decode_engine_cfg_t decode_eng_cfg = {};
  decode_eng_cfg.intr_priority = 0;
  decode_eng_cfg.timeout_ms = 200;

  esp_err_t ret = jpeg_new_decoder_engine(&decode_eng_cfg, &this->jpeg_decoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create hardware JPEG decoder: %s", esp_err_to_name(ret));
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Hardware JPEG decoder initialized (ESP32-P4)");

#elif defined(USE_ESP_JPEG_DECODER)
  // ESP32-S2/S3: esp_jpeg decoder
  this->esp_jpeg_decoder_initialized_ = true;
  ESP_LOGI(TAG, "ESP-JPEG decoder ready (ESP32-S2/S3)");
#endif

  // Initialize JPEG Encoder
#ifdef USE_HARDWARE_JPEG_ENCODER
  // ESP32-P4: Hardware JPEG encoder
  jpeg_encode_engine_cfg_t encode_eng_cfg = {};
  encode_eng_cfg.intr_priority = 0;
  encode_eng_cfg.timeout_ms = 200;

  ret = jpeg_new_encoder_engine(&encode_eng_cfg, &this->jpeg_encoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create hardware JPEG encoder: %s", esp_err_to_name(ret));
    // Don't mark as failed - encoder is optional
  } else {
    ESP_LOGI(TAG, "Hardware JPEG encoder initialized (ESP32-P4)");
  }
#endif

  // Initialize H.264 Decoder
#ifdef USE_HARDWARE_H264_DECODER
  // ESP32-P4: Hardware H.264 decoder
  // Note: Actual initialization will be added when ESP-IDF provides stable H.264 driver API
  // As of ESP-IDF 5.3, H.264 hardware support exists but API may not be finalized
  this->h264_decoder_initialized_ = false;
  ESP_LOGW(TAG, "H.264 decoder: Hardware available but driver API pending (ESP32-P4)");
#endif

  // Initialize H.264 Encoder
#ifdef USE_HARDWARE_H264_ENCODER
  // ESP32-P4: Hardware H.264 encoder
  // Note: Actual initialization will be added when ESP-IDF provides stable H.264 driver API
  this->h264_encoder_initialized_ = false;
  ESP_LOGW(TAG, "H.264 encoder: Hardware available but driver API pending (ESP32-P4)");
#endif

  ESP_LOGCONFIG(TAG, "Transcoder setup complete");
}

void Transcoder::dump_config() {
  ESP_LOGCONFIG(TAG, "Transcoder:");

  // Report JPEG Decoder status
#ifdef TRANSCODER_JPEG_AVAILABLE
  ESP_LOGCONFIG(TAG, "  JPEG Decoder: %s",
                this->is_jpeg_decoder_available() ? "Available" : "Not available");
#ifdef USE_HARDWARE_JPEG_DECODER
  ESP_LOGCONFIG(TAG, "    Type: Hardware (ESP32-P4)");
  if (this->jpeg_decoder_) {
    ESP_LOGCONFIG(TAG, "    Handle: %p", this->jpeg_decoder_);
  }
#elif defined(USE_ESP_JPEG_DECODER)
  ESP_LOGCONFIG(TAG, "    Type: ESP-JPEG (ESP32-S2/S3)");
#endif
#else
  ESP_LOGCONFIG(TAG, "  JPEG Decoder: Not supported on this platform");
#endif

  // Report JPEG Encoder status
#ifdef USE_HARDWARE_JPEG_ENCODER
  ESP_LOGCONFIG(TAG, "  JPEG Encoder: %s",
                this->is_jpeg_encoder_available() ? "Available" : "Not available");
  ESP_LOGCONFIG(TAG, "    Type: Hardware (ESP32-P4)");
  if (this->jpeg_encoder_) {
    ESP_LOGCONFIG(TAG, "    Handle: %p", this->jpeg_encoder_);
  }
#else
  ESP_LOGCONFIG(TAG, "  JPEG Encoder: Not supported on this platform");
#endif

  // Report H.264 Decoder status
#ifdef TRANSCODER_H264_AVAILABLE
  ESP_LOGCONFIG(TAG, "  H.264 Decoder: %s (driver API pending)",
                this->is_h264_decoder_available() ? "Ready" : "Hardware available");
  ESP_LOGCONFIG(TAG, "    Type: Hardware (ESP32-P4)");
#else
  ESP_LOGCONFIG(TAG, "  H.264 Decoder: Not supported on this platform");
#endif

  // Report H.264 Encoder status
#ifdef USE_HARDWARE_H264_ENCODER
  ESP_LOGCONFIG(TAG, "  H.264 Encoder: %s (driver API pending)",
                this->is_h264_encoder_available() ? "Ready" : "Hardware available");
  ESP_LOGCONFIG(TAG, "    Type: Hardware (ESP32-P4)");
#else
  ESP_LOGCONFIG(TAG, "  H.264 Encoder: Not supported on this platform");
#endif
}

}  // namespace transcoder
}  // namespace esphome
