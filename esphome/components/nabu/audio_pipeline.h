#pragma once

#ifdef USE_ESP_IDF

#include "audio_reader.h"
#include "audio_decoder.h"
#include "audio_resampler.h"
#include "audio_mixer.h"
#include "nabu_media_helpers.h"

#include "esphome/components/audio/audio.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

namespace esphome {
namespace nabu {

enum class AudioPipelineType : uint8_t {
  MEDIA,
  ANNOUNCEMENT,
};

enum class AudioPipelineState : uint8_t {
  PLAYING,
  STOPPED,
  ERROR_READING,
  ERROR_DECODING,
  ERROR_RESAMPLING,
};

enum class InfoErrorSource : uint8_t {
  READER = 0,
  DECODER,
  RESAMPLER,
};

enum class DecodingError : uint8_t {
  FAILED_HEADER = 0,
  INCOMPATIBLE_BITS_PER_SAMPLE,
  INCOMPATIBLE_CHANNELS,
};

// Used to pass information from each task.
struct InfoErrorEvent {
  InfoErrorSource source;
  optional<esp_err_t> err;
  optional<MediaFileType> file_type;
  optional<audio::AudioStreamInfo> audio_stream_info;
  optional<ResampleInfo> resample_info;
  optional<DecodingError> decoding_err;
};

class AudioPipeline {
 public:
  AudioPipeline(AudioMixer *mixer, AudioPipelineType pipeline_type);

  /// @brief Starts an audio pipeline given a media url
  /// @param uri media file url
  /// @param target_sample_rate the desired sample rate of the audio stream
  /// @param task_name FreeRTOS task name
  /// @param priority FreeRTOS task priority
  /// @return ESP_OK if successful or an appropriate error if not
  esp_err_t start(const std::string &uri, uint32_t target_sample_rate, const std::string &task_name,
                  UBaseType_t priority = 1);

  /// @brief Starts an audio pipeline given a MediaFile pointer
  /// @param media_file pointer to a MediaFile object
  /// @param target_sample_rate the desired sample rate of the audio stream
  /// @param task_name FreeRTOS task name
  /// @param priority FreeRTOS task priority
  /// @return ESP_OK if successful or an appropriate error if not
  esp_err_t start(MediaFile *media_file, uint32_t target_sample_rate, const std::string &task_name,
                  UBaseType_t priority = 1);

  /// @brief Stops the pipeline. Sends a stop signal to each task (if running) and clears the ring buffers.
  /// @return ESP_OK if successful or ESP_ERR_TIMEOUT if the tasks did not indicate they stopped
  esp_err_t stop();

  /// @brief Gets the state of the audio pipeline based on the info_error_queue_ and event_group_
  /// @return AudioPipelineState
  AudioPipelineState get_state();

  /// @brief Resets the ring buffers, discarding any existing data
  void reset_ring_buffers();

  /// @brief Suspends any running tasks
  void suspend_tasks();
  /// @brief Resumes any running tasks
  void resume_tasks();

 protected:
  /// @brief Allocates the ring buffers, event group, and info error queue.
  /// @return ESP_OK if successful or ESP_ERR_NO_MEM if it is unable to allocate all parts
  esp_err_t allocate_buffers_();

  /// @brief Common start code for the pipeline, regardless if the source is a file or url.
  /// @param target_sample_rate the desired sample rate of the audio stream
  /// @param task_name FreeRTOS task name
  /// @param priority FreeRTOS task priority
  /// @return ESP_OK if successful or an appropriate error if not
  esp_err_t common_start_(uint32_t target_sample_rate, const std::string &task_name, UBaseType_t priority);

  // Pointer to the media player's mixer object. The resample task feeds the appropriate ring buffer directly
  AudioMixer *mixer_;

  std::string current_uri_{};
  MediaFile *current_media_file_{nullptr};

  MediaFileType current_media_file_type_;
  audio::AudioStreamInfo current_audio_stream_info_;
  ResampleInfo current_resample_info_;
  uint32_t target_sample_rate_;

  AudioPipelineType pipeline_type_;

  std::unique_ptr<RingBuffer> raw_file_ring_buffer_;
  std::unique_ptr<RingBuffer> decoded_ring_buffer_;

  // Handles basic control/state of the three tasks
  EventGroupHandle_t event_group_{nullptr};

  // Receives detailed info (file type, stream info, resampling info) or specific errors from the three tasks
  QueueHandle_t info_error_queue_{nullptr};

  // Handles reading the media file from flash or a url
  static void read_task_(void *params);
  TaskHandle_t read_task_handle_{nullptr};
  StaticTask_t read_task_stack_;
  StackType_t *read_task_stack_buffer_{nullptr};

  // Decodes the media file into PCM audio
  static void decode_task_(void *params);
  TaskHandle_t decode_task_handle_{nullptr};
  StaticTask_t decode_task_stack_;
  StackType_t *decode_task_stack_buffer_{nullptr};

  // Resamples the audio to match the specified target sample rate. Converts mono audio to stereo audio if necessary.
  static void resample_task_(void *params);
  TaskHandle_t resample_task_handle_{nullptr};
  StaticTask_t resample_task_stack_;
  StackType_t *resample_task_stack_buffer_{nullptr};
};

}  // namespace nabu
}  // namespace esphome

#endif
