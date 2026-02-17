#pragma once

#include "esphome/core/defines.h"

#if defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/sendspin/sendspin_audio_ring_buffer.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/components/sendspin/sendspin_decoder.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <freertos/queue.h>

namespace esphome {
namespace sendspin {

// Stores the timing information of audio played received from the speaker
struct PlaybackProgress {
  uint32_t frames_played;    // Number of audio frames played since last progress update
  int64_t finish_timestamp;  // The timestamp when the audio frames should finish playing
};

enum class SendspinGenerationState : uint8_t {
  START_TASK,
  GENERATING,
  IDLE,
};

enum class SyncTaskState : uint8_t {
  INITIAL_SYNC,
  LOAD_CHUNK,
  SYNCHRONIZE_AUDIO,
  TRANSFER_AUDIO,
};

enum class DecodeResult : uint8_t {
  SUCCESS,  // Audio decoded successfully (or header processed)
  SKIPPED,  // Chunk skipped because it can't be played in time
  FAILED,   // Decoder failed to decode the chunk
};

// Stores all the variables need by segments of the sync task
struct SyncContext {
  AudioRingBufferEntry *encoded_entry{nullptr};
  audio::AudioStreamInfo current_stream_info;
  size_t bytes_per_frame;
  std::unique_ptr<audio::AudioSinkTransferBuffer> decode_buffer;  // Reusable decode + output buffer
  int64_t decoded_timestamp{0};                                   // Timestamp for decoded audio
  std::unique_ptr<audio::AudioSinkTransferBuffer> interpolation_transfer_buffer;
  std::unique_ptr<SendspinDecoder> decoder;
  audio::AudioSinkCallback *audio_sink{nullptr};  // Owned by task stack, outlives this context
  size_t pipeline_index;
  bool release_chunk;
  bool initial_decode;
  bool hard_syncing{true};  // Starts true so initial sync uses tight settle threshold
  uint32_t buffered_frames;
  int64_t new_audio_client_playtime;
};

// Forward declaration
class SendspinMediaSource;

/// @brief Context for a single pipeline's
struct SendspinMediaSourcePipeline {
  bool paused{false};
  bool pending_start{false};
  SendspinGenerationState generation_state{SendspinGenerationState::IDLE};
  EventGroupHandle_t event_group{nullptr};
  QueueHandle_t controls_queue{nullptr};
  TaskHandle_t sync_task_handle{nullptr};
  StaticTask_t sync_task_stack;
  StackType_t *sync_task_stack_buffer{nullptr};

  QueueHandle_t playback_progress_queue;
  audio::AudioStreamInfo stream_info;
  std::unique_ptr<SendspinAudioRingBuffer> encoded_ring_buffer;
  uint32_t single_frames_added{0};
  uint32_t single_frames_removed{0};
  uint32_t hard_sync_added_frames{0};
  uint32_t hard_sync_removed_frames{0};
};

/// @brief Parameters passed to generate task
struct GenerateTaskParams {
  SendspinMediaSource *source;
  size_t pipeline;
};

class SendspinMediaSource : public Component, public media_source::MediaSource, public Parented<SendspinHub> {
 public:
  void setup() override;
  void loop() override;

  // MediaSource interface implementation
  void init_pipelines(size_t pipeline_count) override;
  bool play_uri(const std::string &uri, size_t pipeline) override;
  void handle_command(media_source::MediaSourceCommand command, size_t pipeline) override;
  media_source::MediaSourceCapabilities get_capabilities() override;

  void notify_volume_changed(float volume) override;
  void notify_mute_changed(bool is_muted) override;
  void notify_audio_played(uint32_t frames, int64_t timestamp, size_t pipeline) override;

  // Configuration setters
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

 protected:
  static void sync_task(void *params);

  /// @brief Handles the INITIAL_SYNC state: feeds zeros to prime the audio pipeline.
  SyncTaskState sync_handle_initial_sync_(SyncContext &sync_context);

  /// @brief Handles the LOAD_CHUNK state: loads and decodes the next encoded chunk.
  SyncTaskState sync_handle_load_chunk_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Handles the SYNCHRONIZE_AUDIO state: applies sync corrections based on predicted error.
  SyncTaskState sync_handle_synchronize_audio_(SyncContext &sync_context,
                                               SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Handles the TRANSFER_AUDIO state: sends buffered audio to the sink.
  SyncTaskState sync_handle_transfer_audio_(SyncContext &sync_context);

  /// @brief Updates buffered_frames and new_audio_client_playtime after sending audio to the speaker.
  /// These two must always be updated together to keep the playtime estimate consistent.
  void sync_track_sent_audio_(SyncContext &sync_context, size_t bytes_sent);

  /// @brief Transfers audio from interpolation and decode buffers to the sink.
  /// Returns true when all data has been sent, false if more transfers are needed.
  bool sync_transfer_audio_(SyncContext &sync_context);

  /// @brief Loads the next encoded chunk from the ring buffer.
  /// Returns true if a chunk is available, false if none ready yet.
  bool sync_load_next_chunk_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Removes last decoded frame, blending into the second-to-last to minimize glitches.
  /// Returns -1 if a frame was removed, 0 if preconditions not met.
  int32_t sync_soft_sync_remove_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Adds one interpolated frame between the first two decoded frames.
  /// Returns 1 if a frame was added, 0 if preconditions not met.
  int32_t sync_soft_sync_add_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Drains stale audio from the ring buffer until a codec header is found.
  void sync_drain_until_codec_header_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Decodes the current encoded chunk.
  DecodeResult sync_decode_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Processes playback progress messages from the speaker to update buffered_frames and playtime.
  void sync_process_playback_progress_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  FixedVector<SendspinMediaSourcePipeline> sendspin_pipelines_;
  bool task_stack_in_psram_{false};
};

#endif

}  // namespace sendspin
}  // namespace esphome
