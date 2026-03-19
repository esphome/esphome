#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/sendspin/sendspin_protocol.h"  // For ChunkType, DummyHeader, SendspinCodecFormat

#include "esphome/components/audio/audio.h"

#include <flac_decoder.h>
#include <opus.h>

namespace esphome {
namespace sendspin {

class SendspinDecoder {
 public:
  // TODO: Most of this code should actually be in the audio component. The AudioDecoder class should be separated into
  // one that handles the streaming data and another (with basically this code) that handles just decoding 1 frame at a
  // time and nothing more
  ~SendspinDecoder() { this->reset_decoders(); }

  // Reset the state of the FLAC and Opus decoders
  void reset_decoders();

  /// @brief Sets up the appropriate decoder and processes the codec header (which may be a dummy header).
  /// @param data Pointer to the header data.
  /// @param data_size Size of the header data in bytes.
  /// @param chunk_type Type of header chunk.
  /// @param stream_info Pointer to AudioStreamInfo that will be filled out when decoding the header.
  /// @return True if successful, false otherwise.
  bool process_header(const uint8_t *data, size_t data_size, ChunkType chunk_type, audio::AudioStreamInfo *stream_info);

  /// @brief Decodes an encoded audio chunk into a caller-provided buffer.
  /// @param data Pointer to the encoded audio data.
  /// @param data_size Size of the encoded audio data in bytes.
  /// @param output_buffer Pointer to the buffer where decoded audio will be written.
  /// @param output_buffer_size Size of the output buffer in bytes.
  /// @param decoded_size Pointer to store the number of decoded bytes written.
  /// @return True if successful, false otherwise.
  bool decode_audio_chunk(const uint8_t *data, size_t data_size, uint8_t *output_buffer, size_t output_buffer_size,
                          size_t *decoded_size);

  size_t get_maximum_decoded_size() const { return this->maximum_decoded_size_; }

  SendspinCodecFormat get_current_codec() const { return this->current_codec_; }

 protected:
  bool decode_dummy_header_(const uint8_t *data, size_t data_size, audio::AudioStreamInfo *stream_info);

  std::unique_ptr<esp_audio_libs::flac::FLACDecoder> flac_decoder_;
  OpusDecoder *opus_decoder_{nullptr};
  size_t opus_decoder_size_{0};

  size_t maximum_decoded_size_{0};

  audio::AudioStreamInfo current_stream_info_;
  SendspinCodecFormat current_codec_ = SendspinCodecFormat::UNSUPPORTED;
};

}  // namespace sendspin
}  // namespace esphome
#endif
