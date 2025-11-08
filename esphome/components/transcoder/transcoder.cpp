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

Transcoder::Transcoder() { global_transcoder = this; }

void Transcoder::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Transcoder...");

  // Note: JPEG decoder now uses lazy initialization (created on first use)
  // This matches the old storage_image behavior and ensures hardware is ready
#ifdef TRANSCODER_ENABLE_JPEG_DECODER
#ifdef USE_ESP_JPEG_DECODER
  // ESP32-S2/S3: esp_jpeg decoder can be marked ready immediately
  this->esp_jpeg_decoder_initialized_ = true;
  ESP_LOGI(TAG, "ESP-JPEG decoder ready (ESP32-S2/S3)");
#endif
#endif

  // Initialize JPEG Encoder (only if requested by components)
#ifdef TRANSCODER_ENABLE_JPEG_ENCODER
#ifdef USE_HARDWARE_JPEG_ENCODER
  // ESP32-P4: Hardware JPEG encoder
  jpeg_encode_engine_cfg_t encode_eng_cfg = {};
  encode_eng_cfg.intr_priority = 0;
  encode_eng_cfg.timeout_ms = 200;

  esp_err_t ret = jpeg_new_encoder_engine(&encode_eng_cfg, &this->jpeg_encoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create hardware JPEG encoder: %s", esp_err_to_name(ret));
    // Don't mark as failed - encoder is optional
  } else {
    ESP_LOGI(TAG, "Hardware JPEG encoder initialized (ESP32-P4)");
  }
#endif
#endif

  // Initialize H.264 Decoder (only if requested by components)
#ifdef TRANSCODER_ENABLE_H264_DECODER
#ifdef USE_ESP_H264_DECODER
  // ESP32-P4: Hardware encoder + Software decoder
  // ESP32-S3: Software encoder/decoder
  // Using esp_h264 v1.1.2 from ESP Component Registry
  esp_h264_dec_config_t dec_config = {};
  dec_config.pic_num = 3;  // Number of reference pictures
  dec_config.timeout = 200;  // Match JPEG timeout

  ret = esp_h264_dec_open(&dec_config, &this->h264_decoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create H.264 decoder: %s", esp_err_to_name(ret));
    // Don't mark as failed - decoder is optional
  } else {
    ESP_LOGI(TAG, "H.264 decoder initialized");
  }
#endif
#endif

  // Initialize H.264 Encoder (only if requested by components)
#ifdef TRANSCODER_ENABLE_H264_ENCODER
#ifdef USE_ESP_H264_ENCODER
  // ESP32-P4: Hardware encoder
  // ESP32-S3: Software encoder
  // Using esp_h264 v1.1.2 from ESP Component Registry
  esp_h264_enc_config_t enc_config = {};
  enc_config.width = 640;  // Default resolution (will be reconfigured by component)
  enc_config.height = 480;
  enc_config.fps = 30;
  enc_config.gop = 30;
  enc_config.bitrate = 1000000;  // 1 Mbps
  enc_config.timeout = 200;

  ret = esp_h264_enc_open(&enc_config, &this->h264_encoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create H.264 encoder: %s", esp_err_to_name(ret));
    // Don't mark as failed - encoder is optional
  } else {
    ESP_LOGI(TAG, "H.264 encoder initialized");
  }
#endif
#endif

  ESP_LOGCONFIG(TAG, "Transcoder setup complete");
}

// Lazy initialization for JPEG decoder (matches old storage_image behavior)
// Note: Implementation not guarded to ensure it's always available when header declares it
#ifdef USE_HARDWARE_JPEG_DECODER
jpeg_decoder_handle_t Transcoder::get_jpeg_decoder() {
  // Return existing decoder if already initialized
  if (this->jpeg_decoder_ != nullptr) {
    return this->jpeg_decoder_;
  }

  // Initialize decoder on first use (after all hardware is ready)
  ESP_LOGI(TAG, "Lazy-initializing hardware JPEG decoder (ESP32-P4)...");

  jpeg_decode_engine_cfg_t decode_eng_cfg = {};
  decode_eng_cfg.intr_priority = 0;
  decode_eng_cfg.timeout_ms = 200;

  esp_err_t ret = jpeg_new_decoder_engine(&decode_eng_cfg, &this->jpeg_decoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create hardware JPEG decoder: %s", esp_err_to_name(ret));
    return nullptr;
  }

  ESP_LOGI(TAG, "Hardware JPEG decoder initialized successfully (handle: %p)", this->jpeg_decoder_);
  return this->jpeg_decoder_;
}
#endif  // USE_HARDWARE_JPEG_DECODER

void Transcoder::dump_config() {
  ESP_LOGCONFIG(TAG, "Transcoder:");

  // Report JPEG Decoder status (only if enabled)
#ifdef TRANSCODER_ENABLE_JPEG_DECODER
  ESP_LOGCONFIG(TAG, "  JPEG Decoder: %s",
                this->is_jpeg_decoder_available() ? "Available" : "Not available");
#ifdef USE_HARDWARE_JPEG_DECODER
  ESP_LOGCONFIG(TAG, "    Type: Hardware (ESP32-P4)");
  if (this->jpeg_decoder_) {
    ESP_LOGCONFIG(TAG, "    Handle: %p", this->jpeg_decoder_);
  }
#elif defined(USE_ESP_JPEG_DECODER)
  ESP_LOGCONFIG(TAG, "    Type: ESP-JPEG (ESP32-S2/S3)");
#elif defined(USE_JPEGDEC)
  ESP_LOGCONFIG(TAG, "    Type: JPEGDec (fallback)");
#endif
#endif

  // Report JPEG Encoder status (only if enabled)
#ifdef TRANSCODER_ENABLE_JPEG_ENCODER
  ESP_LOGCONFIG(TAG, "  JPEG Encoder: %s",
                this->is_jpeg_encoder_available() ? "Available" : "Not available");
#ifdef USE_HARDWARE_JPEG_ENCODER
  ESP_LOGCONFIG(TAG, "    Type: Hardware (ESP32-P4)");
  if (this->jpeg_encoder_) {
    ESP_LOGCONFIG(TAG, "    Handle: %p", this->jpeg_encoder_);
  }
#endif
#endif

  // Report H.264 Decoder status (only if enabled)
#ifdef TRANSCODER_ENABLE_H264_DECODER
  ESP_LOGCONFIG(TAG, "  H.264 Decoder: %s",
                this->is_h264_decoder_available() ? "Available" : "Not available");
#ifdef USE_ESP_H264_DECODER
  ESP_LOGCONFIG(TAG, "    Type: esp_h264 v1.1.2 (ESP32-P4/S3)");
  if (this->h264_decoder_) {
    ESP_LOGCONFIG(TAG, "    Handle: %p", this->h264_decoder_);
  }
#endif
#endif

  // Report H.264 Encoder status (only if enabled)
#ifdef TRANSCODER_ENABLE_H264_ENCODER
  ESP_LOGCONFIG(TAG, "  H.264 Encoder: %s",
                this->is_h264_encoder_available() ? "Available" : "Not available");
#ifdef USE_ESP_H264_ENCODER
  ESP_LOGCONFIG(TAG, "    Type: esp_h264 v1.1.2 (ESP32-P4/S3)");
  if (this->h264_encoder_) {
    ESP_LOGCONFIG(TAG, "    Handle: %p", this->h264_encoder_);
  }
#endif
#endif
}

}  // namespace transcoder
}  // namespace esphome
