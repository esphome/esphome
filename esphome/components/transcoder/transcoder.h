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

#ifdef USE_HARDWARE_H264_DECODER
// ESP32-P4: Hardware H.264 codec
// Note: H.264 driver headers - these may need to be updated based on ESP-IDF version
// As of ESP-IDF 5.3, H.264 support is available but headers may vary
#include "driver/h264_types.h"
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
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // ========== JPEG Decoder API ==========

#if defined(USE_HARDWARE_JPEG_DECODER)
  /**
   * @brief Get hardware JPEG decoder handle (ESP32-P4)
   * @return Pointer to JPEG decoder handle, or nullptr if not available
   */
  jpeg_decoder_handle_t get_jpeg_decoder() { return this->jpeg_decoder_; }

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

#ifdef USE_HARDWARE_H264_DECODER
  /**
   * @brief Check if H.264 decoder is available (ESP32-P4)
   * Note: Actual H.264 API will be added once ESP-IDF provides stable headers
   */
  bool is_h264_decoder_available() { return this->h264_decoder_initialized_; }
#endif

  // ========== H.264 Encoder API ==========

#ifdef USE_HARDWARE_H264_ENCODER
  /**
   * @brief Check if H.264 encoder is available (ESP32-P4)
   * Note: Actual H.264 API will be added once ESP-IDF provides stable headers
   */
  bool is_h264_encoder_available() { return this->h264_encoder_initialized_; }
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
#ifdef USE_HARDWARE_H264_DECODER
  bool h264_decoder_initialized_{false};
  // Actual decoder handle will be added when ESP-IDF headers are available
#endif

  // H.264 Encoder state
#ifdef USE_HARDWARE_H264_ENCODER
  bool h264_encoder_initialized_{false};
  // Actual encoder handle will be added when ESP-IDF headers are available
#endif
};

/// Global transcoder instance accessor
extern Transcoder *global_transcoder;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace transcoder
}  // namespace esphome
