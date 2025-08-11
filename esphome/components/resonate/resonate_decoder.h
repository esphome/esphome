#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

#include "resonate_chunk_queue.h"
#include "resonate_protocol.h"

#include "esphome/components/audio/audio.h"

#include <flac_decoder.h>
#include <opus.h>

namespace esphome {
namespace resonate {

class ResonateDecoder {
 public:
  // TODO: Most of this code should actually be in the audio component. The AudioDecoder class should be separated into
  // one that handles the streaming data and another (with basically this code) that handles just decoding 1 frame at a
  // time and nothing more
  ~ResonateDecoder() { this->reset_decoders(); }

  // Reset the state of the FLAC and Opus decoders
  void reset_decoders();

  /// @brief Setups the appropriate decoder and then processs the codec header (which may be a dummy header). If
  /// successfully parsed, the header_chunk data is deallocated.
  /// @param header_chunk AudioChunk with header
  /// @param stream_info Pointer to AudioStreamInfo that will be filled out when decoding the header
  /// @return True if successful, false otherwise
  bool process_header(AudioChunk *header_chunk, audio::AudioStreamInfo *stream_info);

  /// @brief Decodes an encoded audio chunk. If succesful, the encoded_chunk is deallocated.
  /// @param encoded_chunk AudioChunk pointer with encoded audio
  /// @param decoded_chunk AudioChunk pointer to store decoded audio
  /// @return True if successful, false otherwise
  bool decode_audio_chunk(AudioChunk *encoded_chunk, AudioChunk *decoded_chunk);

  ResonateCodecFormat get_current_codec() const { return this->current_codec_; }

 protected:
  bool decode_dummy_header_(const AudioChunk &header_chunk, audio::AudioStreamInfo *stream_info);

  std::unique_ptr<esp_audio_libs::flac::FLACDecoder> flac_decoder_;
  OpusDecoder *opus_decoder_{nullptr};
  size_t opus_decoder_size_{0};

  audio::AudioStreamInfo current_stream_info_;
  ResonateCodecFormat current_codec_ = ResonateCodecFormat::RESONATE_CODEC_UNSUPPORTED;
};

}  // namespace resonate
}  // namespace esphome
#endif
