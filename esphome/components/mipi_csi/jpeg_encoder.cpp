#include "jpeg_encoder.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include "esphome/core/log.h"

namespace esphome::mipi_csi {

ESPHOME_LOG_TAG(TAG, "mipi_csi.jpeg");

/// Encoder engines need a per-frame timeout. 5 s is far longer than any supported frame takes.
static constexpr int ENCODE_TIMEOUT_MS = 5000;

bool JpegEncoder::init(uint16_t width, uint16_t height, size_t frame_size) {
  jpeg_encode_engine_cfg_t engine_config{};
  engine_config.timeout_ms = ENCODE_TIMEOUT_MS;
  esp_err_t err = jpeg_new_encoder_engine(&engine_config, &this->engine_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Encoder engine setup failed: %s", esp_err_to_name(err));
    return false;
  }

  jpeg_encode_memory_alloc_cfg_t memory_config{};
  memory_config.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
  this->output_ = static_cast<uint8_t *>(jpeg_alloc_encoder_mem(frame_size, &memory_config, &this->output_size_));
  if (this->output_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %zu byte JPEG output buffer", frame_size);
    return false;
  }

  this->config_.width = width;
  this->config_.height = height;
  return true;
}

size_t JpegEncoder::encode(const uint8_t *frame, size_t frame_length) {
  uint32_t encoded_length = 0;
  esp_err_t err = jpeg_encoder_process(this->engine_, &this->config_, frame, frame_length, this->output_,
                                       this->output_size_, &encoded_length);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Encoding frame failed: %s", esp_err_to_name(err));
    return 0;
  }
  return encoded_length;
}

}  // namespace esphome::mipi_csi

#endif
