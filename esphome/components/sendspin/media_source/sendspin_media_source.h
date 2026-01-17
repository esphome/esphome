#pragma once

#include "esphome/core/defines.h"

#if defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/sendspin/sendspin_audio_chunk.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_chunk_queue.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/components/sendspin/sendspin_decoder.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <deque>
#include <optional>

namespace esphome {
namespace sendspin {

// Stores the timing information of audio played received from the speaker
struct PlaybackProgress {
  uint32_t frames_played;    // Number of audio frames played since last progress update
  int64_t finish_timestamp;  // The timestamp when the audio frames should finish playing
};

// Stores the timing information for decoded chunks of audio sent to the speaker
struct InternalAudioTiming {
  int64_t timestamp;          // Timestamp when this audio chunk should finish playing
  uint32_t total_frames;      // Total number of audio frames in this chunk, including corrections
  int32_t frame_corrections;  // Number of frames in this added/removed by the decoder to maintain sync
};

enum class SendspinGenerationState : uint8_t {
  START_TASK,
  GENERATING,
  IDLE,
};

// Stores all the variables need by segments of the sync task
struct SyncContext {
  std::shared_ptr<SendspinAudioChunk> encoded_chunk;
  std::shared_ptr<SendspinAudioChunk> decoded_chunk;
  audio::AudioStreamInfo current_stream_info;
  size_t bytes_per_frame;
  std::unique_ptr<audio::AudioSinkTransferBuffer> output_transfer_buffer;
  std::unique_ptr<audio::AudioSinkTransferBuffer> interpolation_transfer_buffer;
  std::unique_ptr<SendspinDecoder> decoder;
  bool release_chunk;
  bool initial_decode;
  int64_t pending_frame_corrections;
  int synced_chunks;
  std::deque<InternalAudioTiming> chunk_timings;
  std::optional<int64_t> last_error;
  int64_t temporary_hard_sync_threshold;
  int64_t recent_error_us;
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
  std::unique_ptr<audio::AudioChunkQueue> encoded_chunk_queue;
  uint32_t single_frames_added_{0};
  uint32_t single_frames_removed_{0};
  uint32_t hard_sync_added_frames_{0};
  uint32_t hard_sync_removed_frames_{0};
  uint32_t audible_syncs_{0};
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

  // Return true if ready to move onto next stage, false if more audio needs to be sent
  bool sync_transfer_audio_(SyncContext &sync_context);

  /// @brief Return true fi ready to move onto next stage, false if needed to run again
  bool sync_load_next_chunk_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Determines the raw sync error (not taking into account pending corrections). Return true if ready to go
  /// onto the next stage, false if need to run again.
  bool sync_determine_raw_sync_error_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  /// @brief Determine the error after pending corrections. Returns true always
  bool sync_determine_predicted_error_(SyncContext &sync_context);

  /// @biref Return true if ready to move onto next stage, otherwise call again
  bool sync_synchronize_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  void sync_hard_sync_add_silence_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);
  void sync_hard_sync_remove_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context,
                                    InternalAudioTiming &timings, int32_t &frame_corrections);

  void sync_soft_sync_remove_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context,
                                    InternalAudioTiming &timings, int32_t &frame_corrections);
  void sync_soft_sync_add_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context,
                                 InternalAudioTiming &timings, int32_t &frame_corrections);

  void sync_soft_reset_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  bool sync_decode_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context);

  void set_transfer_callbacks_(SyncContext &sync_context, int pipeline);

  FixedVector<SendspinMediaSourcePipeline> sendspin_pipelines_;
  bool task_stack_in_psram_{false};
};

#endif

}  // namespace sendspin
}  // namespace esphome
