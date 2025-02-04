#pragma once

#ifdef USE_ESP32

#include "audio.h"
#include "audio_transfer_buffer.h"

#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

#include "esphome/core/ring_buffer.h"

#include "esp_err.h"

#include <resampler.h>  // esp-audio-libs

namespace esphome {
namespace audio {

enum class AudioResamplerState : uint8_t {
  RESAMPLING,  // More data is available to resample
  FINISHED,    // All file data has been resampled and transferred
  FAILED,      // Unused state included for consistency among Audio classes
};

class AudioResampler {
  /*
   * @brief Class that facilitates resampling audio.
   * The audio data is read from a ring buffer source, resampled, and sent to an audio sink (ring buffer or speaker
   * component). Also supports converting bits per sample.
   */
 public:
  /// @brief Allocates the input and output transfer buffers
  /// @param input_buffer_size Size of the input transfer buffer in bytes.
  /// @param output_buffer_size Size of the output transfer buffer in bytes.
  AudioResampler(size_t input_buffer_size, size_t output_buffer_size);

  /// @brief Adds a source ring buffer for audio data. Takes ownership of the ring buffer in a shared_ptr.
  /// @param input_ring_buffer weak_ptr of a shared_ptr of the sink ring buffer to transfer ownership
  /// @return ESP_OK if successsful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_source(std::weak_ptr<RingBuffer> &input_ring_buffer);

  /// @brief Adds a sink ring buffer for resampled audio. Takes ownership of the ring buffer in a shared_ptr.
  /// @param output_ring_buffer weak_ptr of a shared_ptr of the sink ring buffer to transfer ownership
  /// @return ESP_OK if successsful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_sink(std::weak_ptr<RingBuffer> &output_ring_buffer);

#ifdef USE_SPEAKER
  /// @brief Adds a sink speaker for decoded audio.
  /// @param speaker pointer to speaker component
  /// @return ESP_OK if successsful, ESP_ERR_NO_MEM if the transfer buffer wasn't allocated
  esp_err_t add_sink(speaker::Speaker *speaker);
#endif

  /// @brief Sets up the class to resample.
  /// @param input_stream_info The incoming sample rate, bits per sample, and number of channels
  /// @param output_stream_info The desired outgoing sample rate, bits per sample, and number of channels
  /// @param number_of_taps Number of taps per FIR filter
  /// @param number_of_filters Number of FIR filters
  /// @return ESP_OK if it is able to convert the incoming stream,
  ///         ESP_ERR_NO_MEM if the transfer buffers failed to allocate,
  ///         ESP_ERR_NOT_SUPPORTED if the stream can't be converted.
  esp_err_t start(AudioStreamInfo &input_stream_info, AudioStreamInfo &output_stream_info, uint16_t number_of_taps,
                  uint16_t number_of_filters);

  /// @brief Resamples audio from the ring buffer source and writes to the sink.
  /// @param stop_gracefully If true, it indicates the file decoder is finished. The resampler will resample all the
  ///                        remaining audio and then finish.
  /// @param ms_differential Pointer to a (int32_t) variable that will store the difference, in milliseconds, between
  ///                        the duration of input audio used and the duration of output audio generated.
  /// @return AudioResamplerState
  AudioResamplerState resample(bool stop_gracefully, int32_t *ms_differential);

  /// @brief Pauses sending resampled audio to the sink. If paused, it will continue to process internal buffers.
  /// @param pause_state If true, audio data is not sent to the sink.
  void set_pause_output_state(bool pause_state) { this->pause_output_ = pause_state; }

 protected:
  std::unique_ptr<AudioSourceTransferBuffer> input_transfer_buffer_;
  std::unique_ptr<AudioSinkTransferBuffer> output_transfer_buffer_;

  size_t input_buffer_size_;
  size_t output_buffer_size_;

  uint32_t accumulated_frames_used_{0};
  uint32_t accumulated_frames_generated_{0};

  bool pause_output_{false};

  AudioStreamInfo input_stream_info_;
  AudioStreamInfo output_stream_info_;

  std::unique_ptr<esp_audio_libs::resampler::Resampler> resampler_;
};

}  // namespace audio
}  // namespace esphome

#endif
