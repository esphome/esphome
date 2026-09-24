#include "jpeg_encoder.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <cstdlib>
#include "esphome/core/log.h"

namespace esphome::mipi_csi {

static const char *const TAG = "mipi_csi.jpeg";

/// Encoder engines need a per-frame timeout. 5 s is far longer than any supported frame takes.
static constexpr int ENCODE_TIMEOUT_MS = 5000;
/// Smallest output buffer we will allocate, so that tiny frames still have room for the headers.
static constexpr size_t MIN_OUTPUT_SIZE = 8192;

bool JpegEncoder::init(uint16_t width, uint16_t height, jpeg_enc_input_format_t input_format,
                       jpeg_down_sampling_type_t sub_sample, uint8_t quality, size_t frame_size) {
  jpeg_encode_engine_cfg_t engine_config{};
  engine_config.timeout_ms = ENCODE_TIMEOUT_MS;
  esp_err_t err = jpeg_new_encoder_engine(&engine_config, &this->engine_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Encoder engine setup failed: %s", esp_err_to_name(err));
    return false;
  }

  // A JPEG is expected to be well under the raw frame size, and three quarters of it is the
  // headroom Espressif's own examples use. Encoding fails outright if a frame ever exceeds it.
  size_t requested = frame_size * 3 / 4;
  if (requested < MIN_OUTPUT_SIZE) {
    requested = MIN_OUTPUT_SIZE;
  }
  jpeg_encode_memory_alloc_cfg_t memory_config{};
  memory_config.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
  this->output_ = static_cast<uint8_t *>(jpeg_alloc_encoder_mem(requested, &memory_config, &this->output_size_));
  if (this->output_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %zu byte JPEG output buffer", requested);
    return false;
  }

  this->config_.width = width;
  this->config_.height = height;
  this->config_.src_type = input_format;
  this->config_.sub_sample = sub_sample;
  this->config_.image_quality = quality;
  ESP_LOGD(TAG, "Encoder ready: %ux%u, quality %u, %zu byte output buffer", width, height, quality, this->output_size_);
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

JpegEncoder::~JpegEncoder() {
  if (this->engine_ != nullptr) {
    jpeg_del_encoder_engine(this->engine_);
  }
  // jpeg_alloc_encoder_mem() hands back plain heap memory.
  free(this->output_);  // NOLINT(cppcoreguidelines-no-malloc)
}

}  // namespace esphome::mipi_csi

#endif
