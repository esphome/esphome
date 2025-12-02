#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_AUDIO)

#include "sendspin_audio_chunk.h"
#include "sendspin_protocol.h"

#include "esphome/components/audio/audio.h"
// #include "esphome/components/audio/audio_chunk_queue.h"

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

  /// @brief Setups the appropriate decoder and then processs the codec header (which may be a dummy header).
  /// @param header_chunk SendspinAudioChunk with header
  /// @param stream_info Pointer to AudioStreamInfo that will be filled out when decoding the header
  /// @return True if successful, false otherwise
  bool process_header(std::shared_ptr<SendspinAudioChunk> header_chunk, audio::AudioStreamInfo *stream_info);

  /// @brief Decodes an encoded audio chunk.
  /// @param encoded_chunk SendspinAudioChunk pointer with encoded audio
  /// @param decoded_chunk Reference to shared_ptr to store decoded audio
  ///                      For PCM: shares the same data with new shared_ptr
  ///                      For other codecs: new allocation
  /// @return True if successful, false otherwise
  bool decode_audio_chunk(std::shared_ptr<SendspinAudioChunk> encoded_chunk,
                          std::shared_ptr<SendspinAudioChunk> &decoded_chunk);

  SendspinCodecFormat get_current_codec() const { return this->current_codec_; }

 protected:
  bool decode_dummy_header_(std::shared_ptr<SendspinAudioChunk> header_chunk, audio::AudioStreamInfo *stream_info);

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
