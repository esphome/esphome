#pragma once

#ifdef USE_ESP_IDF

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_reader.h"
#include "esphome/components/audio/audio_decoder.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/ring_buffer.h"

#include "esp_err.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

namespace esphome {
namespace speaker {

// Internal sink/source buffers for reader and decoder
static const size_t DEFAULT_TRANSFER_BUFFER_SIZE = 24 * 1024;

enum class AudioPipelineType : uint8_t {
  MEDIA,
  ANNOUNCEMENT,
};

enum class AudioPipelineState : uint8_t {
  STARTING_FILE,
  STARTING_URL,
  PLAYING,
  STOPPING,
  STOPPED,
  PAUSED,
  ERROR_READING,
  ERROR_DECODING,
};

enum class InfoErrorSource : uint8_t {
  READER = 0,
  DECODER,
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
  optional<audio::AudioFileType> file_type;
  optional<audio::AudioStreamInfo> audio_stream_info;
  optional<DecodingError> decoding_err;
};

class AudioPipeline {
 public:
  /// @param speaker ESPHome speaker component for pipeline's audio output
  /// @param buffer_size Size of the buffer in bytes between the reader and decoder
  /// @param task_stack_in_psram True if the task stack should be allocated in PSRAM, false otherwise
  /// @param task_name FreeRTOS task base name
  /// @param priority FreeRTOS task priority
  AudioPipeline(speaker::Speaker *speaker, size_t buffer_size, bool task_stack_in_psram, std::string base_name,
                UBaseType_t priority);

  /// @brief Starts an audio pipeline given a media url
  /// @param uri media file url
  /// @return ESP_OK if successful or an appropriate error if not
  void start_url(const std::string &uri);

  /// @brief Starts an audio pipeline given a AudioFile pointer
  /// @param audio_file pointer to an AudioFile object
  /// @return ESP_OK if successful or an appropriate error if not
  void start_file(audio::AudioFile *audio_file);

  /// @brief Stops the pipeline. Sends a stop signal to each task (if running) and clears the ring buffers.
  /// @return ESP_OK if successful or ESP_ERR_TIMEOUT if the tasks did not indicate they stopped
  esp_err_t stop();

  /// @brief Processes the state of the audio pipeline based on the info_error_queue_ and event_group_. Handles creating
  /// and stopping the pipeline tasks. Needs to be regularly called to update the internal pipeline state.
  /// @return AudioPipelineState
  AudioPipelineState process_state();

  /// @brief Suspends any running tasks
  void suspend_tasks();
  /// @brief Resumes any running tasks
  void resume_tasks();

  uint32_t get_playback_ms() { return this->playback_ms_; }

  void set_pause_state(bool pause_state);

 protected:
  /// @brief Allocates the event group and info error queue.
  /// @return ESP_OK if successful or ESP_ERR_NO_MEM if it is unable to allocate all parts
  esp_err_t allocate_communications_();

  /// @brief Common start code for the pipeline, regardless if the source is a file or url.
  /// @return ESP_OK if successful or an appropriate error if not
  esp_err_t start_tasks_();

  /// @brief Resets the task related pointers and deallocates their stacks.
  void delete_tasks_();

  std::string base_name_;
  UBaseType_t priority_;

  uint32_t playback_ms_{0};

  bool hard_stop_{false};
  bool is_playing_{false};
  bool pause_state_{false};
  bool task_stack_in_psram_;

  // Pending file start state used to ensure the pipeline fully stops before attempting to start the next file
  bool pending_url_{false};
  bool pending_file_{false};

  speaker::Speaker *speaker_{nullptr};

  std::string current_uri_{};
  audio::AudioFile *current_audio_file_{nullptr};

  audio::AudioFileType current_audio_file_type_;
  audio::AudioStreamInfo current_audio_stream_info_;

  size_t buffer_size_;           // Ring buffer between reader and decoder
  size_t transfer_buffer_size_;  // Internal source/sink buffers for the audio reader and decoder

  std::weak_ptr<RingBuffer> raw_file_ring_buffer_;

  // Handles basic control/state of the three tasks
  EventGroupHandle_t event_group_{nullptr};

  // Receives detailed info (file type, stream info, resampling info) or specific errors from the three tasks
  QueueHandle_t info_error_queue_{nullptr};

  // Handles reading the media file from flash or a url
  static void read_task(void *params);
  TaskHandle_t read_task_handle_{nullptr};
  StaticTask_t read_task_stack_;
  StackType_t *read_task_stack_buffer_{nullptr};

  // Decodes the media file into PCM audio
  static void decode_task(void *params);
  TaskHandle_t decode_task_handle_{nullptr};
  StaticTask_t decode_task_stack_;
  StackType_t *decode_task_stack_buffer_{nullptr};
};

}  // namespace speaker
}  // namespace esphome

#endif
