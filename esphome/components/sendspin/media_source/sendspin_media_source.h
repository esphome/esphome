#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "sendspin_audio_ring_buffer.h"
#include "sendspin_decoder.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/media_source/media_source.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
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
  SUCCESS,            // Audio decoded successfully (or header processed)
  SKIPPED,            // Chunk skipped because it can't be played in time
  FAILED,             // Decoder failed to decode the chunk
  ALLOCATION_FAILED,  // Buffer allocation failed, task should stop
};

// Stores all the variables needed by segments of the sync task
struct SyncContext {
  // Smart pointers (4 bytes each on ESP32)
  std::unique_ptr<audio::AudioSinkTransferBuffer> decode_buffer;  // Reusable decode + output buffer
  std::unique_ptr<audio::AudioSinkTransferBuffer> interpolation_transfer_buffer;
  std::unique_ptr<SendspinDecoder> decoder;

  // Raw pointers (4 bytes each on ESP32)
  AudioRingBufferEntry *encoded_entry{nullptr};
  audio::AudioSinkCallback *audio_sink{nullptr};  // Owned by task stack, outlives this context

  // 64-bit members
  int64_t decoded_timestamp{0};  // Timestamp for decoded audio
  int64_t new_audio_client_playtime{0};

  // 32-bit members
  uint32_t buffered_frames{0};
  size_t bytes_per_frame{0};
  audio::AudioStreamInfo current_stream_info;  // Contains uint32_t and smaller members

  // 8-bit members
  bool release_chunk{false};
  bool initial_decode{false};
  bool hard_syncing{true};  // Starts true so initial sync uses tight settle threshold
};

class SendspinMediaSource : public Component, public media_source::MediaSource, public Parented<SendspinHub> {
 public:
  void setup() override;
  void loop() override;

  // MediaSource interface implementation
  bool play_uri(const std::string &uri) override;
  void handle_command(media_source::MediaSourceCommand command) override;
  bool can_handle(const std::string &uri) const override { return uri.starts_with("sendspin://"); }
  bool has_internal_playlist() const override { return true; }

  void notify_volume_changed(float volume) override;
  void notify_mute_changed(bool is_muted) override;
  void notify_audio_played(uint32_t frames, int64_t timestamp) override;

  // Configuration setters
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

 protected:
  static void sync_task(void *params);

  /// @brief Handles the INITIAL_SYNC state: feeds zeros to prime the audio pipeline.
  SyncTaskState sync_handle_initial_sync_(SyncContext &sync_context);

  /// @brief Handles the LOAD_CHUNK state: loads and decodes the next encoded chunk.
  SyncTaskState sync_handle_load_chunk_(SyncContext &sync_context);

  /// @brief Handles the SYNCHRONIZE_AUDIO state: applies sync corrections based on predicted error.
  SyncTaskState sync_handle_synchronize_audio_(SyncContext &sync_context);

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
  bool sync_load_next_chunk_(SyncContext &sync_context);

  /// @brief Removes last decoded frame, blending into the second-to-last to minimize glitches.
  /// Returns -1 if a frame was removed, 0 if preconditions not met.
  int32_t sync_soft_sync_remove_audio_(SyncContext &sync_context);

  /// @brief Adds one interpolated frame between the first two decoded frames.
  /// Returns 1 if a frame was added, 0 if preconditions not met.
  int32_t sync_soft_sync_add_audio_(SyncContext &sync_context);

  /// @brief Drains stale audio from the ring buffer until a codec header is found.
  void sync_drain_until_codec_header_(SyncContext &sync_context);

  /// @brief Decodes the current encoded chunk.
  DecodeResult sync_decode_audio_(SyncContext &sync_context);

  /// @brief Drains stale audio from the ring buffer, preserving any codec header for the next task.
  void sync_drain_stale_audio_();

  /// @brief Processes playback progress messages from the speaker to update buffered_frames and playtime.
  void sync_process_playback_progress_(SyncContext &sync_context);

  // Large struct (contains StaticTask_t TCB, pointers, uint32_t, bool)
  StaticTask sync_task_;

  // Smart pointers
  std::unique_ptr<SendspinAudioRingBuffer> encoded_ring_buffer_;

  // Raw pointers (FreeRTOS handles are pointers)
  EventGroupHandle_t event_group_{nullptr};
  QueueHandle_t controls_queue_{nullptr};
  QueueHandle_t playback_progress_queue_{nullptr};

  // Raw pointer to a codec header entry still checked out from the ring buffer.
  // Held across task restarts so the new task can process it without re-draining.
  AudioRingBufferEntry *pending_codec_header_{nullptr};

  // 8-bit members
  SendspinGenerationState generation_state_{SendspinGenerationState::IDLE};
  std::atomic<bool> pending_start_{false};
  bool task_stack_in_psram_{false};
};

}  // namespace sendspin
}  // namespace esphome

#endif
