#pragma once

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_SPDIF_MODE)

#include <cstdint>
#include <functional>
#include <freertos/FreeRTOS.h>

namespace esphome::i2s_audio {

// A SPDIF sample is 64-bits
static constexpr uint8_t SPDIF_BITS_PER_SAMPLE = 64;
// Number of samples in a SPDIF block
static constexpr uint16_t SPDIF_BLOCK_SAMPLES = 192;
// To emulate bi-phase mark code (BMC) (aka differential Manchester encoding) we send twice
//  as many bits per sample so that we can generate the transitions this encoding requires.
static constexpr uint8_t EMULATED_BMC_BITS_PER_SAMPLE = SPDIF_BITS_PER_SAMPLE * 2;
static constexpr uint16_t SPDIF_BLOCK_SIZE_BYTES = SPDIF_BLOCK_SAMPLES * (EMULATED_BMC_BITS_PER_SAMPLE / 8);
static constexpr uint32_t SPDIF_BLOCK_SIZE_U32 = SPDIF_BLOCK_SIZE_BYTES / sizeof(uint32_t);  // One block, 1536 bytes

class SPDIFEncoder {
 public:
  /// @brief Initialize the BMC lookup table and working buffer
  void setup();

  /// @brief Function to call when a block of data is complete (called from write)
  void set_block_complete_callback(
      std::function<esp_err_t(uint32_t *data, size_t size, TickType_t ticks_to_wait)> callback) {
    this->block_complete_callback_ = std::move(callback);
  }

  /// @brief Convert PCM audio data to SPDIF BMC encoded data
  /// @param src Source PCM audio data
  /// @param size Size of source data in bytes
  /// @return esp_err_t as returned from block_complete_callback_
  esp_err_t write(const uint8_t *src, size_t size, TickType_t ticks_to_wait);

  /// @brief Reset the SPDIF block buffer
  void reset() { this->spdif_block_ptr_ = this->spdif_block_buf_; }

 protected:
  std::function<esp_err_t(uint32_t *data, size_t size, TickType_t ticks_to_wait)> block_complete_callback_;

  // Working buffer that holds an entire SPDIF block ready for I2S output
  uint32_t spdif_block_buf_[SPDIF_BLOCK_SIZE_U32];
  uint32_t *spdif_block_ptr_{nullptr};
};

}  // namespace esphome::i2s_audio

#endif  // USE_I2S_AUDIO_SPDIF_MODE
