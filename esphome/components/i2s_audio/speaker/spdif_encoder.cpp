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

// Initialize S/PDIF buffer
bool SPDIFEncoder::setup() {
  this->spdif_block_buf_ = std::make_unique<uint32_t[]>(SPDIF_BLOCK_SIZE_U32);
  if (!this->spdif_block_buf_) {
    ESP_LOGE(TAG, "Buffer allocation failed (%zu bytes)", SPDIF_BLOCK_SIZE_BYTES);
    return false;
  }
  ESP_LOGV(TAG, "Buffer allocated (%zu bytes)", SPDIF_BLOCK_SIZE_BYTES);

  this->reset();
  return true;
}

void SPDIFEncoder::reset() {
  this->spdif_block_ptr_ = this->spdif_block_buf_.get();
  this->frame_in_block_ = 0;
  this->is_left_channel_ = true;
}

// LUT-free BMC encoding
// Encodes 'num_bits' bits from 'data', returns BMC output for MSB-first I2S transmission.
// Data is processed LSB-first (S/PDIF order), but output is placed MSB-first.
//
// BMC encoding rules:
// - Always transition at start of bit cell
// - Additional transition in middle for '1' bit
//
// Phase HIGH: bit 0 -> 00 (end LOW),  bit 1 -> 01 (end HIGH)
// Phase LOW:  bit 0 -> 11 (end HIGH), bit 1 -> 10 (end LOW)
//
// Key insight: '0' bits flip phase, '1' bits maintain it.
//
// Output bit placement (for MSB-first I2S transmission):
// - Data bit 0's encoding goes to MSB of output (transmitted first)
// - Data bit N-1's encoding goes to LSB of output (transmitted last)
uint32_t SPDIFEncoder::bmc_encode_(uint32_t data, uint8_t num_bits, bool &phase) {
  uint32_t bmc = 0;
  for (uint8_t i = 0; i < num_bits; i++) {
    bool bit = (data >> i) & 1;
    uint8_t bmc_pair = phase ? (bit ? 0b01 : 0b00) : (bit ? 0b10 : 0b11);
    // Place at MSB-first position: data bit 0 -> highest bit positions
    bmc |= static_cast<uint32_t>(bmc_pair) << ((num_bits - 1 - i) * 2);
    if (!bit) {
      phase = !phase;  // '0' flips phase, '1' maintains it
    }
  }
  return bmc;
}

void SPDIFEncoder::encode_sample_(const uint8_t *pcm_sample) {
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
  //   Bit 30:     C (Channel status) - 0 (simplified, no channel status block)
  //   Bit 31:     P (Parity) - even parity over bits 4-31
  // ============================================================================

  // Place 16-bit audio sample at bits 12-27 (little-endian input: [0]=LSB, [1]=MSB)
  uint32_t raw_subframe = (static_cast<uint32_t>(pcm_sample[1]) << 20) | (static_cast<uint32_t>(pcm_sample[0]) << 12);

  // V, U, C are all 0 for basic operation (bits 28-30 already zero)

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
  // BMC encode the data portion (bits 4-31)
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

  // All preambles end at phase HIGH (last bit = 1 for B, M, and W preambles)
  bool phase = true;

  // Encode subframe bits 4-7 (4 bits -> 8 BMC bits) - first aux nibble
  uint32_t bits_4_7 = 0;  // Always zeros for 16-bit audio
  uint32_t bmc_4_7 = bmc_encode_(bits_4_7, 4, phase);

  // Encode subframe bits 8-11 (4 bits -> 8 BMC bits) - second aux nibble
  uint32_t bits_8_11 = 0;  // Always zeros for 16-bit audio
  uint32_t bmc_8_11 = bmc_encode_(bits_8_11, 4, phase);

  // Encode subframe bits 12-15 (4 bits -> 8 BMC bits) - audio low nibble
  uint32_t bits_12_15 = (raw_subframe >> 12) & 0xF;
  uint32_t bmc_12_15 = bmc_encode_(bits_12_15, 4, phase);

  // Encode subframe bits 16-23 (8 bits -> 16 BMC bits) - audio mid byte
  uint32_t bits_16_23 = (raw_subframe >> 16) & 0xFF;
  uint32_t bmc_16_23 = bmc_encode_(bits_16_23, 8, phase);

  // Encode subframe bits 24-31 (8 bits -> 16 BMC bits) - audio high nibble + VUCP
  uint32_t bits_24_31 = (raw_subframe >> 24) & 0xFF;
  uint32_t bmc_24_31 = bmc_encode_(bits_24_31, 8, phase);

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
  //   bits 24-31: preamble  (transmitted 1st, as MSB of upper halfword)
  //   bits 16-23: bmc_4_7   (transmitted 2nd, as LSB of upper halfword)
  //   bits 8-15:  bmc_8_11  (transmitted 3rd, as MSB of lower halfword)
  //   bits 0-7:   bmc_12_15 (transmitted 4th, as LSB of lower halfword)
  //
  // word[1] layout:
  //   bits 16-31: bmc_16_23 (transmitted 5th)
  //   bits 0-15:  bmc_24_31 (transmitted 6th)
  this->spdif_block_ptr_[0] = bmc_12_15 | (bmc_8_11 << 8) | (bmc_4_7 << 16) | (static_cast<uint32_t>(preamble) << 24);
  this->spdif_block_ptr_[1] = bmc_24_31 | (bmc_16_23 << 16);
  this->spdif_block_ptr_ += 2;

  // ============================================================================
  // Update position tracking
  // ============================================================================
  if (!this->is_left_channel_) {
    // Completed a stereo frame, advance frame counter
    this->frame_in_block_ = (this->frame_in_block_ + 1) % SPDIF_BLOCK_SAMPLES;
  }
  this->is_left_channel_ = !this->is_left_channel_;
}

esp_err_t SPDIFEncoder::send_block_(TickType_t ticks_to_wait) {
  // Use the appropriate callback based on preload mode
  SPDIFBlockCallback &callback = this->preload_mode_ ? this->preload_callback_ : this->write_callback_;

  esp_err_t err = callback(this->spdif_block_buf_.get(), SPDIF_BLOCK_SIZE_BYTES, ticks_to_wait);

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

esp_err_t SPDIFEncoder::write(const uint8_t *src, size_t size, TickType_t ticks_to_wait, uint32_t *blocks_sent) {
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
      return err;
    }
    ++block_count;
  }

  if (blocks_sent != nullptr) {
    *blocks_sent = block_count;
  }
  return ESP_OK;
}

esp_err_t SPDIFEncoder::flush_with_silence(TickType_t ticks_to_wait) {
  // First, send any pending complete block from a previous failed send
  if (this->spdif_block_ptr_ >= &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
    esp_err_t err = this->send_block_(ticks_to_wait);
    if (err != ESP_OK) {
      return err;
    }
  }

  if (!this->has_pending_data()) {
    return ESP_OK;  // Nothing to flush
  }

  // Encode silence (zeros) until the block is complete
  static const uint8_t SILENCE[2] = {0, 0};

  while (this->spdif_block_ptr_ < &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
    this->encode_sample_(SILENCE);
  }

  return this->send_block_(ticks_to_wait);
}

}  // namespace esphome::i2s_audio

#endif  // USE_I2S_AUDIO_SPDIF_MODE
