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

  /// @brief Set input PCM width: 2 = 16-bit, 3 = 24-bit, 4 = 32-bit (truncated to 24-bit on the wire).
  /// Must be called before write() if input width changes from the default (16-bit). Triggers a
  /// channel-status rebuild to reflect the new word length.
  void set_bytes_per_sample(uint8_t bytes_per_sample);

  /// @brief Get the configured input PCM width in bytes per sample
  uint8_t get_bytes_per_sample() const { return this->bytes_per_sample_; }

  /// @brief Convert PCM audio data to SPDIF BMC encoded data
  /// @param src Source PCM audio data (stereo, width matches set_bytes_per_sample)
  /// @param size Size of source data in bytes
  /// @param ticks_to_wait Timeout for blocking writes
  /// @param blocks_sent Optional pointer to receive the number of complete SPDIF blocks sent
  /// @param bytes_consumed Optional pointer to receive the number of PCM bytes consumed from src
  /// @return esp_err_t as returned from the callback
  esp_err_t write(const uint8_t *src, size_t size, TickType_t ticks_to_wait, uint32_t *blocks_sent = nullptr,
                  size_t *bytes_consumed = nullptr);

  /// @brief Emit one complete SPDIF block: pad any pending partial block with silence and send,
  /// or send a full silence block if nothing is pending. Always produces exactly one block on success.
  /// @param ticks_to_wait Timeout for blocking writes
  /// @return esp_err_t as returned from the callback
  esp_err_t flush_with_silence(TickType_t ticks_to_wait);

  /// @brief Reset the SPDIF block buffer and position tracking, discarding any partial block
  void reset();

  /// @brief Set the sample rate for Channel Status Block encoding
  /// @param sample_rate Sample rate in Hz (e.g., 44100, 48000)
  /// Call this before writing audio data to ensure correct channel status.
  void set_sample_rate(uint32_t sample_rate);

  /// @brief Get the currently configured sample rate
  uint32_t get_sample_rate() const { return this->sample_rate_; }

 protected:
  /// @brief Encode a single stereo silence frame at the current block position.
  /// @note Used only by flush_with_silence_typed_ to pad; the hot write path inlines the
  /// encoding body directly into write_typed_ to keep block_ptr / frame_in_block_ in registers.
  template<uint8_t Bps> void encode_silence_frame_();

  /// @brief Templated write loop. Called from the public write() via runtime dispatch on bytes_per_sample_.
  template<uint8_t Bps>
  HOT esp_err_t write_typed_(const uint8_t *src, size_t size, TickType_t ticks_to_wait, uint32_t *blocks_sent,
                             size_t *bytes_consumed);

  /// @brief Templated flush-with-silence. Pads the pending block with zeros at the configured width
  /// (or builds a full silence block when nothing is pending) and sends it. Always emits one block.
  template<uint8_t Bps> esp_err_t flush_with_silence_typed_(TickType_t ticks_to_wait);

  /// @brief Send the completed block via the appropriate callback
  esp_err_t send_block_(TickType_t ticks_to_wait);

  /// @brief Build the channel status block from current configuration
  void build_channel_status_();

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
  uint8_t bytes_per_sample_{2};  // Input PCM width: 2/3/4 (16/24/32-bit). 32-bit truncates to 24-bit on the wire.
  uint8_t frame_in_block_{0};    // 0-191, tracks stereo frame position within block
  bool preload_mode_{false};     // Whether to use preload callback vs write callback
  // True when spdif_block_buf_ currently holds a complete full-silence block valid for the active
  // channel status. A full silence block is deterministic for a given sample rate and word length,
  // so when this is set flush_with_silence() can re-send the buffer verbatim instead of re-encoding.
  bool block_buf_is_silence_block_{false};

  // Channel Status Block (192 bits = 24 bytes, transmitted over 192 frames)
  // Placed last since std::array<uint8_t> has 1-byte alignment
  std::array<uint8_t, 24> channel_status_{};
};

}  // namespace esphome::i2s_audio

#endif  // USE_I2S_AUDIO_SPDIF_MODE
