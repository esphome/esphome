/**
 * @file transcoder.h
 * @brief Centralized hardware media codec management for ESPHome
 *
 * Provides unified access to hardware-accelerated media codecs including:
 * - JPEG encoder/decoder (ESP32-S2/S3/P4)
 * - H.264 encoder/decoder (ESP32-P4)
 *
 * Components requiring codec support should depend on this component and
 * access codecs through the global_transcoder accessor.
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"

// Platform-specific codec headers
#ifdef USE_ESP_JPEG_DECODER
// ESP32-S2/S3: esp_jpeg from ESP Component Registry
#include "esp_jpeg_dec.h"
#include "esp_jpeg_enc.h"
#endif

#ifdef USE_HARDWARE_JPEG_DECODER
// ESP32-P4: Hardware JPEG codec
#include "driver/jpeg_decode.h"
#include "driver/jpeg_encode.h"
#include "driver/jpeg_types.h"
#include "hal/color_types.h"
#endif

#if defined(USE_ESP_H264_DECODER) || defined(USE_ESP_H264_ENCODER)
// ESP32-P4: Hardware H.264 encoder + Software decoder
// ESP32-S3: Software H.264 encoder/decoder
// Use esp_h264 from ESP Component Registry
#include "esp_h264_dec.h"
#include "esp_h264_enc_single.h"
#include "esp_h264_types.h"
#endif

namespace esphome {
namespace transcoder {

/**
 * @brief Codec types supported by the transcoder
 */
enum class CodecType {
  JPEG_DECODER,
  JPEG_ENCODER,
  H264_DECODER,
  H264_ENCODER,
};

/**
 * @brief Main transcoder component managing hardware media codecs
 *
 * This component initializes and provides access to hardware-accelerated
 * media codecs available on the platform. It handles platform-specific
 * initialization and provides a unified API for other components.
 */
class Transcoder : public Component {
 public:
  Transcoder();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // ========== JPEG Decoder API ==========

#if defined(USE_HARDWARE_JPEG_DECODER)
  /**
   * @brief Get hardware JPEG decoder handle (ESP32-P4)
   * Lazily initializes decoder on first access (like old storage_image code)
   * @return Pointer to JPEG decoder handle, or nullptr if initialization fails
   */
  jpeg_decoder_handle_t get_jpeg_decoder();

  /**
   * @brief Check if JPEG decoder is available
   */
  bool is_jpeg_decoder_available() { return this->jpeg_decoder_ != nullptr; }

#elif defined(USE_ESP_JPEG_DECODER)
  /**
   * @brief Get esp_jpeg decoder config (ESP32-S2/S3)
   * @return Pointer to decoder config, or nullptr if not initialized
   */
  esp_jpeg_image_cfg_t *get_esp_jpeg_decoder_config() { return &this->esp_jpeg_decoder_cfg_; }

  /**
   * @brief Check if esp_jpeg decoder is available
   */
  bool is_jpeg_decoder_available() { return this->esp_jpeg_decoder_initialized_; }
#endif

  // ========== JPEG Encoder API ==========

#if defined(USE_HARDWARE_JPEG_ENCODER)
  /**
   * @brief Get hardware JPEG encoder handle (ESP32-P4)
   * @return Pointer to JPEG encoder handle, or nullptr if not available
   */
  jpeg_encoder_handle_t get_jpeg_encoder() { return this->jpeg_encoder_; }

  /**
   * @brief Check if JPEG encoder is available
   */
  bool is_jpeg_encoder_available() { return this->jpeg_encoder_ != nullptr; }
#endif

  // ========== H.264 Decoder API ==========

#ifdef USE_ESP_H264_DECODER
  /**
   * @brief Get H.264 decoder handle
   * @return Pointer to decoder handle, or nullptr if not available
   */
  esp_h264_dec_handle_t get_h264_decoder() { return this->h264_decoder_; }

  /**
   * @brief Check if H.264 decoder is available
   */
  bool is_h264_decoder_available() { return this->h264_decoder_ != nullptr; }
#endif

  // ========== H.264 Encoder API ==========

#ifdef USE_ESP_H264_ENCODER
  /**
   * @brief Get H.264 encoder handle
   * @return Pointer to encoder handle, or nullptr if not available
   */
  esp_h264_enc_handle_t get_h264_encoder() { return this->h264_encoder_; }

  /**
   * @brief Check if H.264 encoder is available
   */
  bool is_h264_encoder_available() { return this->h264_encoder_ != nullptr; }
#endif

 protected:
  // JPEG Decoder state
#ifdef USE_HARDWARE_JPEG_DECODER
  jpeg_decoder_handle_t jpeg_decoder_{nullptr};
#endif

#ifdef USE_ESP_JPEG_DECODER
  esp_jpeg_image_cfg_t esp_jpeg_decoder_cfg_{};
  bool esp_jpeg_decoder_initialized_{false};
#endif

  // JPEG Encoder state
#ifdef USE_HARDWARE_JPEG_ENCODER
  jpeg_encoder_handle_t jpeg_encoder_{nullptr};
#endif

  // H.264 Decoder state
#ifdef USE_ESP_H264_DECODER
  esp_h264_dec_handle_t h264_decoder_{nullptr};
#endif

  // H.264 Encoder state
#ifdef USE_ESP_H264_ENCODER
  esp_h264_enc_handle_t h264_encoder_{nullptr};
#endif
};

/// Global transcoder instance accessor
extern Transcoder *global_transcoder;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace transcoder
}  // namespace esphome
