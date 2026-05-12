#include "spdif_encoder.h"

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_SPDIF_MODE)

#include "esphome/core/log.h"

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.spdif_encoder";

// S/PDIF preamble patterns (8 BMC bits each)
// These are the BMC-encoded sync patterns that violate normal BMC rules for easy detection.
// All preambles end at phase HIGH (last bit = 1), enabling consistent data encoding.
// Preamble is placed at bits 24-31 of word[0] for MSB-first transmission.
static constexpr uint8_t PREAMBLE_B = 0x17;  // Block start (left channel, frame 0)
static constexpr uint8_t PREAMBLE_M = 0x1d;  // Left channel (not block start)
static constexpr uint8_t PREAMBLE_W = 0x1b;  // Right channel

// BMC encoding of 4 zero bits starting at phase HIGH: 00_11_00_11 = 0x33
// Since both aux nibbles (bits 4-7, 8-11) are zero for 16-bit audio and phase is preserved, both are 0x33.
static constexpr uint32_t BMC_ZERO_NIBBLE = 0x33;

// Constexpr BMC encoder for compile-time LUT generation.
// Encodes with start phase=true (HIGH). The complement property allows phase=false
// via XOR: bmc_encode(v, N, false) == bmc_encode(v, N, true) ^ mask
static constexpr uint16_t bmc_lut_encode(uint32_t data, uint8_t num_bits) {
  uint16_t bmc = 0;
  bool phase = true;
  for (uint8_t i = 0; i < num_bits; i++) {
    bool bit = (data >> i) & 1;
    uint8_t bmc_pair = phase ? (bit ? 0b01 : 0b00) : (bit ? 0b10 : 0b11);
    bmc |= static_cast<uint16_t>(bmc_pair) << ((num_bits - 1 - i) * 2);
    if (!bit)
      phase = !phase;
  }
  return bmc;
}

// 4-bit BMC lookup table: 16 entries (16 bytes in flash)
// Index: 4-bit data value (0-15), always phase=true start
static constexpr auto BMC_LUT_4 = [] {
  std::array<uint8_t, 16> t{};
  for (uint32_t i = 0; i < 16; i++)
    t[i] = static_cast<uint8_t>(bmc_lut_encode(i, 4));
  return t;
}();

// 8-bit BMC lookup table: 256 entries (512 bytes in flash)
// Index: 8-bit data value (0-255), always phase=true start
static constexpr auto BMC_LUT_8 = [] {
  std::array<uint16_t, 256> t{};
  for (uint32_t i = 0; i < 256; i++)
    t[i] = bmc_lut_encode(i, 8);
  return t;
}();

// Initialize S/PDIF buffer
bool SPDIFEncoder::setup() {
  this->spdif_block_buf_ = std::make_unique<uint32_t[]>(SPDIF_BLOCK_SIZE_U32);
  if (!this->spdif_block_buf_) {
    ESP_LOGE(TAG, "Buffer allocation failed (%zu bytes)", SPDIF_BLOCK_SIZE_BYTES);
    return false;
  }
  ESP_LOGV(TAG, "Buffer allocated (%zu bytes)", SPDIF_BLOCK_SIZE_BYTES);

  // Build initial channel status block with default sample rate
  this->build_channel_status_();

  this->reset();
  return true;
}

void SPDIFEncoder::reset() {
  this->spdif_block_ptr_ = this->spdif_block_buf_.get();
  this->frame_in_block_ = 0;
  this->is_left_channel_ = true;
}

void SPDIFEncoder::set_sample_rate(uint32_t sample_rate) {
  if (this->sample_rate_ != sample_rate) {
    this->sample_rate_ = sample_rate;
    this->build_channel_status_();
    ESP_LOGD(TAG, "Sample rate set to %lu Hz", (unsigned long) sample_rate);
  }
}

void SPDIFEncoder::build_channel_status_() {
  // IEC 60958-3 Consumer Channel Status Block (192 bits = 24 bytes)
  // Transmitted LSB-first within each byte, one bit per frame via C bit
  //
  // Byte 0: Control bits
  //   Bit 0: 0 = Consumer format (not professional AES3)
  //   Bit 1: 0 = PCM audio (not non-audio data like AC3)
  //   Bit 2: 0 = No copyright assertion
  //   Bits 3-5: 000 = No pre-emphasis
  //   Bits 6-7: 00 = Mode 0 (basic consumer format)
  //
  // Byte 1: Category code (0x00 = general, 0x01 = CD, etc.)
  //
  // Byte 2: Source/channel numbers
  //   Bits 0-3: Source number (0 = unspecified)
  //   Bits 4-7: Channel number (0 = unspecified)
  //
  // Byte 3: Sample frequency and clock accuracy
  //   Bits 0-3: Sample frequency code
  //   Bits 4-5: Clock accuracy (00 = Level II, ±1000 ppm, appropriate for ESP32)
  //   Bits 6-7: Reserved (0)
  //
  // Bytes 4-23: Reserved (zeros for basic compliance)

  // Clear all bytes first
  this->channel_status_.fill(0);

  // Byte 0: Consumer, PCM audio, no copyright, no pre-emphasis, Mode 0
  // All bits are 0, which is already set

  // Byte 1: Category code = 0x00 (general)
  // Already 0

  // Byte 2: Source/channel unspecified
  // Already 0

  // Byte 3: Sample frequency code (bits 0-3) + clock accuracy (bits 4-5)
  // Clock accuracy = 00 (Level II, ±1000 ppm) - appropriate for ESP32
  uint8_t freq_code;
  switch (this->sample_rate_) {
    case 44100:
      freq_code = 0x0;  // 0000
      break;
    case 48000:
      freq_code = 0x2;  // 0010
      break;
    default:
      // Other values are possible but they're not supported by ESPHome
      freq_code = 0x1;  // 0001 = not indicated
      ESP_LOGW(TAG, "Unsupported sample rate %lu Hz, channel status will indicate 'not specified'",
               (unsigned long) this->sample_rate_);
      break;
  }
  // Byte 3: freq_code in bits 0-3, clock accuracy (00) in bits 4-5
  this->channel_status_[3] = freq_code;  // Clock accuracy bits 4-5 are already 0

  // Bytes 4-23 remain zero (word length not specified, no original sample freq, etc.)
}

HOT void SPDIFEncoder::encode_sample_(const uint8_t *pcm_sample) {
  // ============================================================================
  // Build raw 32-bit subframe (IEC 60958 format)
  // ============================================================================
  // Bit layout:
  //   Bits 0-3:   Preamble (handled separately, not in raw_subframe)
  //   Bits 4-7:   Auxiliary audio data (zeros for 16-bit audio)
  //   Bits 8-11:  Audio LSB extension (zeros for 16-bit audio)
  //   Bits 12-27: 16-bit audio sample (MSB-aligned in 20-bit audio field)
  //   Bit 28:     V (Validity) - 0 = valid audio
  //   Bit 29:     U (User data) - 0
  //   Bit 30:     C (Channel status) - from channel status block
  //   Bit 31:     P (Parity) - even parity over bits 4-31
  // ============================================================================

  // Place 16-bit audio sample at bits 12-27 (little-endian input: [0]=LSB, [1]=MSB)
  uint32_t raw_subframe = (static_cast<uint32_t>(pcm_sample[1]) << 20) | (static_cast<uint32_t>(pcm_sample[0]) << 12);

  // V = 0 (valid audio), U = 0 (no user data)
  // C = channel status bit for current frame (same bit used for both L and R subframes)
  bool c_bit = this->get_channel_status_bit_(this->frame_in_block_);
  if (c_bit) {
    raw_subframe |= (1U << 30);
  }

  // Calculate even parity over bits 4-30
  // This ensures consistent BMC ending phase regardless of audio content
  uint32_t bits_4_30 = (raw_subframe >> 4) & 0x07FFFFFF;  // 27 bits (4-30)
  uint32_t ones_count = __builtin_popcount(bits_4_30);
  uint32_t parity = ones_count & 1;  // 1 if odd count, 0 if even
  raw_subframe |= parity << 31;      // Set P bit to make total even

  // ============================================================================
  // Select preamble based on position in block and channel
  // ============================================================================
  // B = block start (left channel, frame 0 of 192-frame block)
  // M = left channel (frames 1-191)
  // W = right channel (all frames)
  uint8_t preamble;
  if (this->is_left_channel_) {
    preamble = (this->frame_in_block_ == 0) ? PREAMBLE_B : PREAMBLE_M;
  } else {
    preamble = PREAMBLE_W;
  }

  // ============================================================================
  // BMC encode the data portion (bits 4-31) using lookup tables
  // ============================================================================
  // The I2S uses 16-bit halfword swap: bits 16-31 transmit before bits 0-15.
  // This applies to BOTH word[0] and word[1].
  //
  // word[0] transmission order: [16-23] → [24-31] → [0-7] → [8-15]
  // For correct S/PDIF subframe order (preamble → aux → audio):
  //   - bits 16-23: preamble (8 BMC bits)
  //   - bits 24-31: BMC(subframe bits 4-7) - first aux nibble
  //   - bits 0-7:   BMC(subframe bits 8-11) - second aux nibble
  //   - bits 8-15:  BMC(subframe bits 12-15) - audio low nibble
  //
  // word[1] transmission order: [16-31] → [0-15]
  // For correct S/PDIF subframe order:
  //   - bits 16-31: BMC(subframe bits 16-23) - audio mid byte
  //   - bits 0-15:  BMC(subframe bits 24-31) - audio high nibble + VUCP
  // ============================================================================

  // All preambles end at phase HIGH. Bits 4-11 are always zero for 16-bit audio;
  // two zero nibbles flip phase 8 times total → back to HIGH.
  // So bits 12-15 always start encoding at phase=true.

  // Bits 12-15: 4-bit LUT lookup (always phase=true start)
  uint32_t nibble = (raw_subframe >> 12) & 0xF;
  uint32_t bmc_12_15 = BMC_LUT_4[nibble];

  // Phase tracking via branchless XOR mask:
  // - 0x0000 means phase=true (use LUT value directly)
  // - 0xFFFF means phase=false (complement LUT value)
  // End phase = start XOR (popcount & 1) since zero-bits flip phase,
  // and for even bit widths: #zeros parity == popcount parity.
  uint32_t phase_mask = -(__builtin_popcount(nibble) & 1u) & 0xFFFF;

  // Bits 16-23: 8-bit LUT lookup with phase correction
  uint32_t byte_mid = (raw_subframe >> 16) & 0xFF;
  uint32_t bmc_16_23 = BMC_LUT_8[byte_mid] ^ phase_mask;
  phase_mask ^= -(__builtin_popcount(byte_mid) & 1u) & 0xFFFF;

  // Bits 24-31: 8-bit LUT lookup with phase correction
  uint32_t byte_hi = (raw_subframe >> 24) & 0xFF;
  uint32_t bmc_24_31 = BMC_LUT_8[byte_hi] ^ phase_mask;

  // ============================================================================
  // Combine with correct positioning for I2S transmission
  // ============================================================================
  // I2S with halfword swap: transmits bits 16-31, then bits 0-15.
  // Within each halfword, MSB (highest bit) is transmitted first.
  //
  // For upper halfword (bits 16-31): bit 31 → bit 16
  // For lower halfword (bits 0-15):  bit 15 → bit 0
  //
  // Desired S/PDIF order: preamble → bmc_4_7 → bmc_8_11 → bmc_12_15
  //
  // word[0] layout for correct transmission:
  //   bits 24-31: preamble        (transmitted 1st, as MSB of upper halfword)
  //   bits 16-23: BMC_ZERO_NIBBLE (transmitted 2nd, aux bits 4-7)
  //   bits 8-15:  BMC_ZERO_NIBBLE (transmitted 3rd, aux bits 8-11)
  //   bits 0-7:   bmc_12_15       (transmitted 4th, audio low nibble)
  //
  // word[1] layout:
  //   bits 16-31: bmc_16_23 (transmitted 5th)
  //   bits 0-15:  bmc_24_31 (transmitted 6th)
  this->spdif_block_ptr_[0] =
      bmc_12_15 | (BMC_ZERO_NIBBLE << 8) | (BMC_ZERO_NIBBLE << 16) | (static_cast<uint32_t>(preamble) << 24);
  this->spdif_block_ptr_[1] = bmc_24_31 | (bmc_16_23 << 16);
  this->spdif_block_ptr_ += 2;

  // ============================================================================
  // Update position tracking
  // ============================================================================
  if (!this->is_left_channel_) {
    // Completed a stereo frame, advance frame counter
    if (++this->frame_in_block_ >= SPDIF_BLOCK_SAMPLES) {
      this->frame_in_block_ = 0;
    }
  }
  this->is_left_channel_ = !this->is_left_channel_;
}

esp_err_t SPDIFEncoder::send_block_(TickType_t ticks_to_wait) {
  // Use the appropriate callback and context based on preload mode
  SPDIFBlockCallback callback;
  void *ctx;

  if (this->preload_mode_) {
    callback = this->preload_callback_;
    ctx = this->preload_callback_ctx_;
  } else {
    callback = this->write_callback_;
    ctx = this->write_callback_ctx_;
  }

  if (callback == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t err = callback(ctx, this->spdif_block_buf_.get(), SPDIF_BLOCK_SIZE_BYTES, ticks_to_wait);

  if (err == ESP_OK) {
    // Reset pointer for next block; position tracking continues from where it left off
    this->spdif_block_ptr_ = this->spdif_block_buf_.get();
  }

  return err;
}

size_t SPDIFEncoder::get_pending_pcm_bytes() const {
  if (this->spdif_block_ptr_ == nullptr || this->spdif_block_buf_ == nullptr) {
    return 0;
  }
  // Each PCM sample (2 bytes) produces 2 uint32_t values in the SPDIF buffer
  // So pending uint32s / 2 = pending samples, and each sample is 2 bytes
  size_t pending_uint32s = this->spdif_block_ptr_ - this->spdif_block_buf_.get();
  size_t pending_samples = pending_uint32s / 2;
  return pending_samples * 2;  // 2 bytes per sample
}

HOT esp_err_t SPDIFEncoder::write(const uint8_t *src, size_t size, TickType_t ticks_to_wait, uint32_t *blocks_sent,
                                  size_t *bytes_consumed) {
  const uint8_t *pcm_data = src;
  const uint8_t *pcm_end = src + size;
  uint32_t block_count = 0;

  while (pcm_data < pcm_end) {
    // Check if there's a pending complete block from a previous failed send
    if (this->spdif_block_ptr_ >= &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
      esp_err_t err = this->send_block_(ticks_to_wait);
      if (err != ESP_OK) {
        if (blocks_sent != nullptr) {
          *blocks_sent = block_count;
        }
        if (bytes_consumed != nullptr) {
          *bytes_consumed = pcm_data - src;
        }
        return err;
      }
      ++block_count;
    }

    // Encode one 16-bit sample
    this->encode_sample_(pcm_data);
    pcm_data += 2;
  }

  // Send any complete block that was just finished
  if (this->spdif_block_ptr_ >= &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
    esp_err_t err = this->send_block_(ticks_to_wait);
    if (err != ESP_OK) {
      if (blocks_sent != nullptr) {
        *blocks_sent = block_count;
      }
      if (bytes_consumed != nullptr) {
        *bytes_consumed = pcm_data - src;
      }
      return err;
    }
    ++block_count;
  }

  if (blocks_sent != nullptr) {
    *blocks_sent = block_count;
  }
  if (bytes_consumed != nullptr) {
    *bytes_consumed = size;
  }
  return ESP_OK;
}

esp_err_t SPDIFEncoder::flush_with_silence(TickType_t ticks_to_wait) {
  // If a complete block is already pending (from a previous failed send), emit just that block.
  // Otherwise pad the partial block with silence (or generate a full silence block if empty)
  // and send. Always emits exactly one block on success.
  if (this->spdif_block_ptr_ < &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
    static const uint8_t SILENCE[2] = {0, 0};
    while (this->spdif_block_ptr_ < &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
      this->encode_sample_(SILENCE);
    }
  }
  return this->send_block_(ticks_to_wait);
}

}  // namespace esphome::i2s_audio

#endif  // USE_I2S_AUDIO_SPDIF_MODE
