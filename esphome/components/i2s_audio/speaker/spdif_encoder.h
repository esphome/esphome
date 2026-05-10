#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_SPDIF_MODE)

#include <array>
#include <cstdint>
#include <memory>
#include <freertos/FreeRTOS.h>
#include "esp_err.h"
#include "esphome/core/helpers.h"

namespace esphome::i2s_audio {

// A SPDIF sample is 64-bits
static constexpr uint8_t SPDIF_BITS_PER_SAMPLE = 64;
// Number of samples in a SPDIF block
static constexpr uint16_t SPDIF_BLOCK_SAMPLES = 192;
// To emulate bi-phase mark code (BMC) (aka differential Manchester encoding) we send twice
//  as many bits per sample so that we can generate the transitions this encoding requires.
static constexpr uint8_t EMULATED_BMC_BITS_PER_SAMPLE = SPDIF_BITS_PER_SAMPLE * 2;
static constexpr uint16_t SPDIF_BLOCK_SIZE_BYTES = SPDIF_BLOCK_SAMPLES * (EMULATED_BMC_BITS_PER_SAMPLE / 8);
static constexpr uint32_t SPDIF_BLOCK_SIZE_U32 = SPDIF_BLOCK_SIZE_BYTES / sizeof(uint32_t);  // 3072 bytes / 4 = 768
// I2S frame count for one SPDIF block (for new driver where frame = 8 bytes for 32-bit stereo)
static constexpr uint32_t SPDIF_BLOCK_I2S_FRAMES = SPDIF_BLOCK_SIZE_BYTES / 8;  // 3072 / 8 = 384 frames
// PCM bytes needed for one complete SPDIF block (192 stereo frames * 2 bytes per sample * 2 channels)
static constexpr uint16_t SPDIF_PCM_BYTES_PER_BLOCK = SPDIF_BLOCK_SAMPLES * 2 * 2;  // = 768 bytes

/// Callback signature for block completion (raw function pointer for minimal overhead)
/// @param user_ctx User context pointer passed during callback registration
/// @param data Pointer to SPDIF encoded block data
/// @param size Size of the block in bytes (always SPDIF_BLOCK_SIZE_BYTES)
/// @param ticks_to_wait FreeRTOS ticks to wait for write completion
/// @return ESP_OK on success, or an error code
using SPDIFBlockCallback = esp_err_t (*)(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait);

class SPDIFEncoder {
 public:
  /// @brief Initialize the SPDIF working buffer
  /// @return true if setup was successful, false if allocation failed
  bool setup();

  /// @brief Set callback for normal writes (used when channel is running)
  /// @param callback Function pointer to call when a block is ready
  /// @param user_ctx Context pointer passed to callback (typically 'this' pointer of speaker)
  void set_write_callback(SPDIFBlockCallback callback, void *user_ctx) {
    this->write_callback_ = callback;
    this->write_callback_ctx_ = user_ctx;
  }

  /// @brief Set callback for preload writes (used when preloading to DMA before enabling channel)
  /// @param callback Function pointer to call when a block is ready for preload
  /// @param user_ctx Context pointer passed to callback (typically 'this' pointer of speaker)
  void set_preload_callback(SPDIFBlockCallback callback, void *user_ctx) {
    this->preload_callback_ = callback;
    this->preload_callback_ctx_ = user_ctx;
  }

  /// @brief Enable or disable preload mode
  /// When in preload mode, completed blocks use the preload callback instead of write callback
  void set_preload_mode(bool preload) { this->preload_mode_ = preload; }

  /// @brief Check if currently in preload mode
  bool is_preload_mode() const { return this->preload_mode_; }

  /// @brief Convert PCM audio data to SPDIF BMC encoded data
  /// @param src Source PCM audio data (16-bit stereo)
  /// @param size Size of source data in bytes
  /// @param ticks_to_wait Timeout for blocking writes
  /// @param blocks_sent Optional pointer to receive the number of complete SPDIF blocks sent
  /// @param bytes_consumed Optional pointer to receive the number of PCM bytes consumed from src
  /// @return esp_err_t as returned from the callback
  esp_err_t write(const uint8_t *src, size_t size, TickType_t ticks_to_wait, uint32_t *blocks_sent = nullptr,
                  size_t *bytes_consumed = nullptr);

  /// @brief Get the number of PCM bytes currently pending in the partial block buffer
  /// @return Number of pending PCM bytes (0 to SPDIF_PCM_BYTES_PER_BLOCK - 1)
  size_t get_pending_pcm_bytes() const;

  /// @brief Get the number of PCM frames currently pending in the partial block buffer
  /// @return Number of pending PCM frames (0 to SPDIF_BLOCK_SAMPLES - 1)
  uint32_t get_pending_frames() const { return this->get_pending_pcm_bytes() / 4; }

  /// @brief Check if there is a partial block pending
  bool has_pending_data() const { return this->spdif_block_ptr_ != this->spdif_block_buf_.get(); }

  /// @brief Emit one complete SPDIF block: pad any pending partial block with silence and send,
  /// or send a full silence block if nothing is pending. Always produces exactly one block on success.
  /// @param ticks_to_wait Timeout for blocking writes
  /// @return esp_err_t as returned from the callback
  esp_err_t flush_with_silence(TickType_t ticks_to_wait);

  /// @brief Reset the SPDIF block buffer and position tracking, discarding any partial block
  void reset();

  /// @brief Set the sample rate for Channel Status Block encoding
  /// @param sample_rate Sample rate in Hz (e.g., 44100, 48000, 96000)
  /// Call this before writing audio data to ensure correct channel status.
  void set_sample_rate(uint32_t sample_rate);

  /// @brief Get the currently configured sample rate
  uint32_t get_sample_rate() const { return this->sample_rate_; }

 protected:
  /// @brief Encode a single 16-bit PCM sample into the current block position
  HOT void encode_sample_(const uint8_t *pcm_sample);

  /// @brief Send the completed block via the appropriate callback
  esp_err_t send_block_(TickType_t ticks_to_wait);

  /// @brief Build the channel status block from current configuration
  void build_channel_status_();

  /// @brief Get the channel status bit for a specific frame
  /// @param frame Frame number (0-191)
  /// @return The C bit value for this frame
  ESPHOME_ALWAYS_INLINE inline bool get_channel_status_bit_(uint8_t frame) const {
    // Channel status is 192 bits transmitted over 192 frames
    // Bit N is transmitted in frame N, LSB-first within each byte
    return (this->channel_status_[frame >> 3] >> (frame & 7)) & 1;
  }

  // Member ordering optimized to minimize padding (largest alignment first)

  // 4-byte aligned members (pointers and uint32_t)
  SPDIFBlockCallback write_callback_{nullptr};
  SPDIFBlockCallback preload_callback_{nullptr};
  void *write_callback_ctx_{nullptr};
  void *preload_callback_ctx_{nullptr};
  std::unique_ptr<uint32_t[]> spdif_block_buf_;  // Working buffer for SPDIF block (heap allocated)
  uint32_t *spdif_block_ptr_{nullptr};           // Current position in block buffer
  uint32_t sample_rate_{48000};                  // Sample rate for Channel Status Block encoding

  // 1-byte aligned members (grouped together to avoid internal padding)
  uint8_t frame_in_block_{0};   // 0-191, tracks stereo frame position within block
  bool is_left_channel_{true};  // Alternates L/R for stereo samples
  bool preload_mode_{false};    // Whether to use preload callback vs write callback

  // Channel Status Block (192 bits = 24 bytes, transmitted over 192 frames)
  // Placed last since std::array<uint8_t> has 1-byte alignment
  std::array<uint8_t, 24> channel_status_{};
};

}  // namespace esphome::i2s_audio

#endif  // USE_I2S_AUDIO_SPDIF_MODE
