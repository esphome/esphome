#pragma once

#ifdef USE_ESP32

#include "audio.h"
#include "audio_transfer_buffer.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

#include "esp_err.h"

// micro-flac
#ifdef USE_AUDIO_FLAC_SUPPORT
#include <micro_flac/flac_decoder.h>
#endif

// micro-mp3
#ifdef USE_AUDIO_MP3_SUPPORT
#include <micro_mp3/mp3_decoder.h>
#endif

// micro-opus
#ifdef USE_AUDIO_OPUS_SUPPORT
#include <micro_opus/ogg_opus_decoder.h>
#endif

// micro-wav
#ifdef USE_AUDIO_WAV_SUPPORT
#include <micro_wav/wav_decoder.h>
#endif

namespace esphome {
namespace audio {

enum class AudioDecoderState : uint8_t {
  DECODING = 0,  // More data is available to decode
  FINISHED,      // All file data has been decoded and transferred
  FAILED,        // Encountered an error
};

// Only used within the AudioDecoder class; conveys the state of the particular file type decoder
enum class FileDecoderState : uint8_t {
  MORE_TO_PROCESS,     // Successsfully read a file chunk and more data is available to decode
  IDLE,                // Not enough data to decode, waiting for more to be transferred
  POTENTIALLY_FAILED,  // Decoder encountered a potentially recoverable error if more file data is available
  FAILED,              // Decoder encoutnered an uncrecoverable error
  END_OF_FILE,         // The specific file decoder knows its the end of the file
};

class AudioDecoder {
  /*
   * @brief Class that facilitates decoding an audio file.
   * The audio file is read from a source (ring buffer or const data pointer), decoded, and sent to an audio sink
   * (ring buffer, speaker component, or callback).
   * Supports flac, mp3, ogg opus, and wav formats (each enabled independently at compile time).
   */
 public:
  /// @brief Allocates the output transfer buffer and stores the input buffer size for later use by add_source()
  /// @param input_buffer_size Size of the input transfer buffer in bytes.
  /// @param output_buffer_size Size of the output transfer buffer in bytes.
  AudioDecoder(size_t input_buffer_size, size_t output_buffer_size);

  ~AudioDecoder() = default;

  /// @brief Adds a source ring buffer for raw file data. Takes ownership of the ring buffer in a shared_ptr.
  /// @param input_ring_buffer weak_ptr of a shared_ptr of the sink ring buffer to transfer ownership
  /// @return ESP_OK if successsful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_source(std::weak_ptr<RingBuffer> &input_ring_buffer);

  /// @brief Adds a sink ring buffer for decoded audio. Takes ownership of the ring buffer in a shared_ptr.
  /// @param output_ring_buffer weak_ptr of a shared_ptr of the sink ring buffer to transfer ownership
  /// @return ESP_OK if successsful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_sink(std::weak_ptr<RingBuffer> &output_ring_buffer);

#ifdef USE_SPEAKER
  /// @brief Adds a sink speaker for decoded audio.
  /// @param speaker pointer to speaker component
  /// @return ESP_OK if successsful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_sink(speaker::Speaker *speaker);
#endif

  /// @brief Adds a const data pointer as the source for raw file data. Does not allocate a transfer buffer.
  /// @param data_pointer Pointer to the const audio data (e.g., stored in flash memory)
  /// @param length Size of the data in bytes
  /// @return ESP_OK
  esp_err_t add_source(const uint8_t *data_pointer, size_t length);

  /// @brief Adds a callback as the sink for decoded audio.
  /// @param callback Pointer to the AudioSinkCallback implementation
  /// @return ESP_OK if successful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_sink(AudioSinkCallback *callback);

  /// @brief Sets up decoding the file
  /// @param audio_file_type AudioFileType of the file
  /// @return ESP_OK if successful, ESP_ERR_NO_MEM if the transfer buffers fail to allocate, or ESP_ERR_NOT_SUPPORTED if
  /// the format isn't supported.
  esp_err_t start(AudioFileType audio_file_type);

  /// @brief Decodes audio from the ring buffer source and writes to the sink.
  /// @param stop_gracefully If true, it indicates the file source is finished. The decoder will decode all the
  /// reamining data and then finish.
  /// @return AudioDecoderState
  AudioDecoderState decode(bool stop_gracefully);

  /// @brief Gets the audio stream information, if it has been decoded from the files header
  /// @return optional<AudioStreamInfo> with the audio information. If not available yet, returns no value.
  const optional<audio::AudioStreamInfo> &get_audio_stream_info() const { return this->audio_stream_info_; }

  /// @brief Returns the duration of audio (in milliseconds) decoded and sent to the sink
  /// @return Duration of decoded audio in milliseconds
  uint32_t get_playback_ms() const { return this->playback_ms_; }

  /// @brief Pauses sending resampled audio to the sink. If paused, it will continue to process internal buffers.
  /// @param pause_state If true, audio data is not sent to the sink.
  void set_pause_output_state(bool pause_state) { this->pause_output_ = pause_state; }

 protected:
#ifdef USE_AUDIO_FLAC_SUPPORT
  FileDecoderState decode_flac_();
  std::unique_ptr<micro_flac::FLACDecoder> flac_decoder_;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
  FileDecoderState decode_mp3_();
  std::unique_ptr<micro_mp3::Mp3Decoder> mp3_decoder_;
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
  FileDecoderState decode_opus_();
  std::unique_ptr<micro_opus::OggOpusDecoder> opus_decoder_;
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
  FileDecoderState decode_wav_();
  std::unique_ptr<micro_wav::WAVDecoder> wav_decoder_;
#endif

  std::unique_ptr<AudioReadableBuffer> input_buffer_;
  std::unique_ptr<AudioSinkTransferBuffer> output_transfer_buffer_;

  AudioFileType audio_file_type_{AudioFileType::NONE};
  optional<AudioStreamInfo> audio_stream_info_{};

  size_t input_buffer_size_{0};
  size_t free_buffer_required_{0};

  uint32_t potentially_failed_count_{0};
  uint32_t accumulated_frames_written_{0};
  uint32_t playback_ms_{0};

  bool end_of_file_{false};

  bool pause_output_{false};
};
}  // namespace audio
}  // namespace esphome

#endif
