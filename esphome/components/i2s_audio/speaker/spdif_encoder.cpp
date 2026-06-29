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
// Used as a constant in the 16-bit subframe path, where bits 4-11 are always zero.
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

// Compile-time parity helper (constexpr-friendly, runs only at LUT build time).
static constexpr uint32_t bmc_lut_parity(uint32_t value, uint32_t num_bits) {
  uint32_t p = 0;
  for (uint32_t b = 0; b < num_bits; b++)
    p ^= (value >> b) & 1u;
  return p;
}

// Combined BMC + phase-delta lookup tables.
// Each entry packs the BMC pattern (lower bits, phase=high start) together with
// a phase-mask delta in bits 16-31 (0xFFFF if the input has odd parity, else 0).
// XORing the delta into the running phase mask propagates parity across chunks
// without an explicit popcount.

// 4-bit BMC lookup table: 16 entries x uint32_t = 64 bytes in flash.
// Bits 0-7   : 8-bit BMC pattern (phase=high start)
// Bits 16-31 : phase-mask delta (0xFFFFu if odd parity, else 0)
static constexpr auto BMC_LUT_4 = [] {
  std::array<uint32_t, 16> t{};
  for (uint32_t i = 0; i < 16; i++) {
    uint32_t bmc = bmc_lut_encode(i, 4);
    uint32_t delta = bmc_lut_parity(i, 4) ? 0xFFFF0000u : 0u;
    t[i] = bmc | delta;
  }
  return t;
}();

// 8-bit BMC lookup table: 256 entries x uint32_t = 1024 bytes in flash.
// Bits 0-15  : 16-bit BMC pattern (phase=high start)
// Bits 16-31 : phase-mask delta (0xFFFFu if odd parity, else 0)
static constexpr auto BMC_LUT_8 = [] {
  std::array<uint32_t, 256> t{};
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t bmc = bmc_lut_encode(i, 8);
    uint32_t delta = bmc_lut_parity(i, 8) ? 0xFFFF0000u : 0u;
    t[i] = bmc | delta;
  }
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

  // Build initial channel status block with default sample rate and width
  this->build_channel_status_();

  this->reset();
  return true;
}

void SPDIFEncoder::reset() {
  this->spdif_block_ptr_ = this->spdif_block_buf_.get();
  this->frame_in_block_ = 0;
  this->block_buf_is_silence_block_ = false;
}

void SPDIFEncoder::set_sample_rate(uint32_t sample_rate) {
  if (this->sample_rate_ != sample_rate) {
    this->sample_rate_ = sample_rate;
    this->build_channel_status_();
    ESP_LOGD(TAG, "Sample rate set to %lu Hz", (unsigned long) sample_rate);
  }
}

void SPDIFEncoder::set_bytes_per_sample(uint8_t bytes_per_sample) {
  if (bytes_per_sample != 2 && bytes_per_sample != 3 && bytes_per_sample != 4) {
    ESP_LOGE(TAG, "Unsupported bytes per sample: %u", (unsigned) bytes_per_sample);
    return;
  }
  if (this->bytes_per_sample_ != bytes_per_sample) {
    this->bytes_per_sample_ = bytes_per_sample;
    this->build_channel_status_();
    // Discard any partial block built at the previous width so we never mix widths on the wire.
    this->reset();
    ESP_LOGD(TAG, "Input width set to %u-bit", (unsigned) bytes_per_sample * 8);
  }
}

void SPDIFEncoder::build_channel_status_() {
  // IEC 60958-3 Consumer Channel Status Block (192 bits = 24 bytes)
  // Transmitted LSB-first within each byte, one bit per frame via C bit.

  // Any cached silence block was built for the previous channel status; it is now stale.
  this->block_buf_is_silence_block_ = false;

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

  // Byte 4: Word length encoding (IEC 60958-3 consumer)
  //   bit 0:    max length flag (0 = max 20 bits, 1 = max 24 bits)
  //   bits 1-3: word length code relative to the max
  // For our supported widths:
  //   16-bit (max 20): 0b0010 = 0x02 -- "16 bits, max 20"
  //   24-bit (max 24): 0b1101 = 0x0D -- "24 bits, max 24"
  //   32-bit input is truncated to 24-bit on the wire, so use the 24-bit code.
  uint8_t word_length_code;
  switch (this->bytes_per_sample_) {
    case 2:
      word_length_code = 0x02;
      break;
    case 3:  // Shared case
    case 4:
      word_length_code = 0x0D;
      break;
    default:
      word_length_code = 0x00;  // not specified
      break;
  }
  this->channel_status_[4] = word_length_code;
}

// Extract the C bit for the given frame from channel_status_ and shift it into bit 30
// so it can be OR'd directly into a raw subframe.
ESPHOME_ALWAYS_INLINE static inline uint32_t c_bit_for_frame(const std::array<uint8_t, 24> &channel_status,
                                                             uint32_t frame) {
  return static_cast<uint32_t>((channel_status[frame >> 3] >> (frame & 7)) & 1u) << 30;
}

// ============================================================================
// IEC 60958 subframe bit layout
// ============================================================================
//   Bits 0-3:   Preamble (handled separately, not in raw_subframe)
//   Bits 4-7:   Auxiliary audio data / 24-bit audio LSB
//   Bits 8-11:  Audio LSB extension (zero for 16-bit, low nibble of audio for 24-bit)
//   Bits 12-27: Audio sample (16 high bits in 16-bit mode, mid 16 bits in 24-bit mode)
//   Bit 28:     V (Validity) - 0 = valid audio
//   Bit 29:     U (User data) - 0
//   Bit 30:     C (Channel status) - from channel status block
//   Bit 31:     P (Parity) - even parity over bits 4-31
// ============================================================================

// Build a raw IEC 60958 subframe from PCM little-endian input of width Bps bytes.
// Caller is responsible for OR-ing in the C bit and parity.
template<uint8_t Bps> ESPHOME_ALWAYS_INLINE static inline uint32_t build_raw_subframe(const uint8_t *pcm_sample) {
  static_assert(Bps == 2 || Bps == 3 || Bps == 4, "Unsupported bytes per sample");
  if constexpr (Bps == 2) {
    // 16-bit input: MSB-aligned in the 20-bit audio field, bits 12-27.
    return (static_cast<uint32_t>(pcm_sample[1]) << 20) | (static_cast<uint32_t>(pcm_sample[0]) << 12);
  } else if constexpr (Bps == 3) {
    // 24-bit input: full 24-bit audio field, bits 4-27.
    return (static_cast<uint32_t>(pcm_sample[2]) << 20) | (static_cast<uint32_t>(pcm_sample[1]) << 12) |
           (static_cast<uint32_t>(pcm_sample[0]) << 4);
  } else {  // Bps == 4
    // 32-bit input truncated to 24-bit: drop the lowest byte.
    return (static_cast<uint32_t>(pcm_sample[3]) << 20) | (static_cast<uint32_t>(pcm_sample[2]) << 12) |
           (static_cast<uint32_t>(pcm_sample[1]) << 4);
  }
}

// BMC-encode a subframe and write the two output uint32 words to dst. Caller passes
// raw_subframe with the C bit set (bit 30) and the P bit cleared (bit 31 = 0). P is
// derived from the cumulative parity-mask delta of the per-byte LUT lookups.
//
// I2S halfword swap means word[0] transmits as: bits 24-31, 16-23, 8-15, 0-7.
// word[1] transmits as: bits 16-31, 0-15. Within each halfword, MSB-first.
// All preambles end at phase HIGH, so phase=true at the start of bit 4.
//
// P-bit derivation: BMC_LUT_*'s upper half encodes the parity of the input chunk. Each
// chunk's parity delta is shifted down (`lut >> 16`) into a phase_mask that lives in the
// low 16 bits, so the same value can also be XORed against subsequent BMC patterns to
// invert phase. XOR'ing those deltas through all chunks (with bit 31 = 0) yields the
// parity of bits 4-30 in the low bits of phase_mask -- the required value of the P bit
// for even total parity. The BMC of bit 31 lives in bit 0 of the high-byte BMC output
// (i = 7 maps to position (8-1-7)*2 = 0); flipping the source bit flips only the lower
// BMC bit (= phase XOR bit), so applying P is `bmc_24_31 ^= phase_mask & 1u`.
template<uint8_t Bps>
ESPHOME_ALWAYS_INLINE static inline void bmc_encode_subframe(uint32_t raw_subframe, uint8_t preamble, uint32_t *dst) {
  if constexpr (Bps == 2) {
    // 16-bit path: bits 4-11 are zero, encoded inline as BMC_ZERO_NIBBLE constants.
    // Eight zero source bits with start phase=HIGH end at phase=HIGH (popcount of zeros is even),
    // so encoding of bits 12-15 starts at phase=true. Zeros contribute 0 to parity.
    uint32_t nibble = (raw_subframe >> 12) & 0xF;
    uint32_t lut_n = BMC_LUT_4[nibble];
    uint32_t bmc_12_15 = lut_n & 0xFFu;
    uint32_t phase_mask = lut_n >> 16;  // 0xFFFFu if odd parity, else 0

    uint32_t byte_mid = (raw_subframe >> 16) & 0xFF;
    uint32_t lut_m = BMC_LUT_8[byte_mid];
    uint32_t bmc_16_23 = (lut_m & 0xFFFFu) ^ phase_mask;
    phase_mask ^= lut_m >> 16;

    uint32_t byte_hi = (raw_subframe >> 24) & 0xFF;  // bit 7 (= P) is 0 by precondition
    uint32_t lut_h = BMC_LUT_8[byte_hi];
    uint32_t bmc_24_31 = (lut_h & 0xFFFFu) ^ phase_mask;
    phase_mask ^= lut_h >> 16;
    // phase_mask now reflects parity of bits 4-30. Apply P by flipping bit 0 of bmc_24_31.
    bmc_24_31 ^= phase_mask & 1u;

    dst[0] = bmc_12_15 | (BMC_ZERO_NIBBLE << 8) | (BMC_ZERO_NIBBLE << 16) | (static_cast<uint32_t>(preamble) << 24);
    dst[1] = bmc_24_31 | (bmc_16_23 << 16);
  } else {
    // 24-bit (and 32-bit truncated) path: bits 4-11 are live audio.
    uint32_t byte_lo = (raw_subframe >> 4) & 0xFF;
    uint32_t lut_l = BMC_LUT_8[byte_lo];
    uint32_t bmc_4_11 = lut_l & 0xFFFFu;
    uint32_t phase_mask = lut_l >> 16;  // 0xFFFFu if odd parity, else 0

    uint32_t nibble = (raw_subframe >> 12) & 0xF;
    uint32_t lut_n = BMC_LUT_4[nibble];
    uint32_t bmc_12_15 = (lut_n & 0xFFu) ^ (phase_mask & 0xFFu);
    phase_mask ^= lut_n >> 16;

    uint32_t byte_mid = (raw_subframe >> 16) & 0xFF;
    uint32_t lut_m = BMC_LUT_8[byte_mid];
    uint32_t bmc_16_23 = (lut_m & 0xFFFFu) ^ phase_mask;
    phase_mask ^= lut_m >> 16;

    uint32_t byte_hi = (raw_subframe >> 24) & 0xFF;  // bit 7 (= P) is 0 by precondition
    uint32_t lut_h = BMC_LUT_8[byte_hi];
    uint32_t bmc_24_31 = (lut_h & 0xFFFFu) ^ phase_mask;
    phase_mask ^= lut_h >> 16;
    bmc_24_31 ^= phase_mask & 1u;

    // word[0]: bits 24-31 = preamble, bits 8-23 = bmc(4-11), bits 0-7 = bmc(12-15)
    // word[1]: bits 16-31 = bmc(16-23), bits 0-15 = bmc(24-31)
    dst[0] = bmc_12_15 | (bmc_4_11 << 8) | (static_cast<uint32_t>(preamble) << 24);
    dst[1] = bmc_24_31 | (bmc_16_23 << 16);
  }
}

template<uint8_t Bps> void SPDIFEncoder::encode_silence_frame_() {
  static constexpr uint8_t SILENCE[4] = {0, 0, 0, 0};
  uint32_t raw = build_raw_subframe<Bps>(SILENCE) | c_bit_for_frame(this->channel_status_, this->frame_in_block_);
  uint8_t preamble_l = (this->frame_in_block_ == 0) ? PREAMBLE_B : PREAMBLE_M;
  bmc_encode_subframe<Bps>(raw, preamble_l, this->spdif_block_ptr_);
  bmc_encode_subframe<Bps>(raw, PREAMBLE_W, this->spdif_block_ptr_ + 2);
  this->spdif_block_ptr_ += 4;
  if (++this->frame_in_block_ >= SPDIF_BLOCK_SAMPLES) {
    this->frame_in_block_ = 0;
  }
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

template<uint8_t Bps>
HOT esp_err_t SPDIFEncoder::write_typed_(const uint8_t *src, size_t size, TickType_t ticks_to_wait,
                                         uint32_t *blocks_sent, size_t *bytes_consumed) {
  const uint8_t *pcm_data = src;
  const uint8_t *const pcm_end = src + size;
  uint32_t block_count = 0;

  // Hot state lives in locals so the compiler can keep it in registers across the
  // per-frame encoding work; byte writes through block_ptr may alias the member fields,
  // which would block register allocation if the encoding read them directly from this->*.
  uint32_t *block_ptr = this->spdif_block_ptr_;
  uint32_t *const block_buf = this->spdif_block_buf_.get();
  uint32_t *const block_end = block_buf + SPDIF_BLOCK_SIZE_U32;
  uint32_t frame = this->frame_in_block_;
  const std::array<uint8_t, 24> &channel_status = this->channel_status_;

  auto save_state = [&]() {
    this->spdif_block_ptr_ = block_ptr;
    this->frame_in_block_ = static_cast<uint8_t>(frame);
  };

  auto report_out_params = [&]() {
    if (blocks_sent != nullptr)
      *blocks_sent = block_count;
    if (bytes_consumed != nullptr)
      *bytes_consumed = pcm_data - src;
  };

  // Send a completed block if the buffer is full, propagating any error.
  // send_block_ resets this->spdif_block_ptr_ to block_buf on success and leaves it
  // unchanged on error -- mirror both behaviors in our local block_ptr.
  auto maybe_send = [&]() -> esp_err_t {
    if (block_ptr >= block_end) {
      esp_err_t err = this->send_block_(ticks_to_wait);
      if (err != ESP_OK) {
        save_state();
        report_out_params();
        return err;
      }
      block_ptr = block_buf;
      ++block_count;
    }
    return ESP_OK;
  };

  // Hot path: encode L+R pairs in two peeled sub-loops. Frame 0 carries the only
  // buffer-full check and uses PREAMBLE_B (a block fills exactly when frame wraps from
  // 191 back to 0). Frames 1..191 use PREAMBLE_M and need no buffer-full check or
  // preamble branch. The encoding body is inlined here so block_ptr lives in a register
  // for the duration of the loop.
  while (pcm_data + 2 * Bps <= pcm_end) {
    if (frame == 0) {
      esp_err_t err = maybe_send();
      if (err != ESP_OK)
        return err;

      uint32_t c_bit = c_bit_for_frame(channel_status, 0);
      uint32_t raw_l = build_raw_subframe<Bps>(pcm_data) | c_bit;
      uint32_t raw_r = build_raw_subframe<Bps>(pcm_data + Bps) | c_bit;
      bmc_encode_subframe<Bps>(raw_l, PREAMBLE_B, block_ptr);
      bmc_encode_subframe<Bps>(raw_r, PREAMBLE_W, block_ptr + 2);
      block_ptr += 4;
      frame = 1;
      pcm_data += 2 * Bps;
    }

    // The inner loop runs until min(SPDIF_BLOCK_SAMPLES, frame + input_frames). The
    // input-size bound is folded into end_frame so a single `frame < end_frame` test
    // governs termination.
    uint32_t input_frames = static_cast<uint32_t>(pcm_end - pcm_data) / (2u * Bps);
    uint32_t end_frame = SPDIF_BLOCK_SAMPLES;
    if (frame + input_frames < end_frame)
      end_frame = frame + input_frames;

    while (frame < end_frame) {
      uint32_t c_bit = c_bit_for_frame(channel_status, frame);
      uint32_t raw_l = build_raw_subframe<Bps>(pcm_data) | c_bit;
      uint32_t raw_r = build_raw_subframe<Bps>(pcm_data + Bps) | c_bit;
      bmc_encode_subframe<Bps>(raw_l, PREAMBLE_M, block_ptr);
      bmc_encode_subframe<Bps>(raw_r, PREAMBLE_W, block_ptr + 2);
      block_ptr += 4;
      ++frame;
      pcm_data += 2 * Bps;
    }
    if (frame >= SPDIF_BLOCK_SAMPLES)
      frame = 0;
  }

  // Send any complete block that was just finished.
  if (block_ptr >= block_end) {
    esp_err_t err = this->send_block_(ticks_to_wait);
    if (err != ESP_OK) {
      save_state();
      report_out_params();
      return err;
    }
    block_ptr = block_buf;
    ++block_count;
  }

  save_state();
  report_out_params();
  return ESP_OK;
}

HOT esp_err_t SPDIFEncoder::write(const uint8_t *src, size_t size, TickType_t ticks_to_wait, uint32_t *blocks_sent,
                                  size_t *bytes_consumed) {
  if (size > 0) {
    // Real PCM is about to be encoded into the buffer, so it is no longer a full-silence block.
    this->block_buf_is_silence_block_ = false;
  }
  switch (this->bytes_per_sample_) {
    case 2:
      return this->write_typed_<2>(src, size, ticks_to_wait, blocks_sent, bytes_consumed);
    case 3:
      return this->write_typed_<3>(src, size, ticks_to_wait, blocks_sent, bytes_consumed);
    case 4:
      return this->write_typed_<4>(src, size, ticks_to_wait, blocks_sent, bytes_consumed);
    default:
      return ESP_ERR_INVALID_STATE;
  }
}

template<uint8_t Bps> esp_err_t SPDIFEncoder::flush_with_silence_typed_(TickType_t ticks_to_wait) {
  // If a complete block is already pending (from a previous failed send), emit just that block.
  // Otherwise pad the partial block with silence (or generate a full silence block if empty) and
  // send. Always emits exactly one block on success.
  if (this->spdif_block_ptr_ < &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
    const bool was_empty = (this->spdif_block_ptr_ == this->spdif_block_buf_.get());
    // Continuous-silence idle case: a full silence block is byte-identical every time for the
    // active channel status, so when the buffer already holds one, re-send it as-is.
    if (was_empty && this->block_buf_is_silence_block_) {
      return this->send_block_(ticks_to_wait);
    }
    // Pad with silence frames at the configured width.
    while (this->spdif_block_ptr_ < &this->spdif_block_buf_[SPDIF_BLOCK_SIZE_U32]) {
      this->encode_silence_frame_<Bps>();
    }
    // The buffer is a reusable full-silence block only if it was built entirely from silence; a
    // partial real-audio block padded out with silence is not.
    this->block_buf_is_silence_block_ = was_empty;
  }
  return this->send_block_(ticks_to_wait);
}

esp_err_t SPDIFEncoder::flush_with_silence(TickType_t ticks_to_wait) {
  switch (this->bytes_per_sample_) {
    case 2:
      return this->flush_with_silence_typed_<2>(ticks_to_wait);
    case 3:
      return this->flush_with_silence_typed_<3>(ticks_to_wait);
    case 4:
      return this->flush_with_silence_typed_<4>(ticks_to_wait);
    default:
      return ESP_ERR_INVALID_STATE;
  }
}

}  // namespace esphome::i2s_audio

#endif  // USE_I2S_AUDIO_SPDIF_MODE
