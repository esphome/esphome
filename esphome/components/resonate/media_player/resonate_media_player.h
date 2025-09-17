#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_MEDIA_PLAYER)

#include "esphome/components/resonate/resonate_hub.h"
#include "esphome/components/resonate/resonate_chunk_queue.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/media_player/media_player.h"
#if defined(USE_RESONATE_AUDIO)
#include "esphome/components/speaker/speaker.h"
#endif

#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#if defined(USE_RESONATE_AUDIO)
#include <freertos/event_groups.h>
#include <freertos/task.h>
#endif

namespace esphome {
namespace resonate {

#if defined(USE_RESONATE_AUDIO)
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
#endif

class ResonateMediaPlayer : public Component, public media_player::MediaPlayer, public Parented<ResonateHub> {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_CONNECTION; }
  void setup() override;
  void loop() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;

#if defined(USE_RESONATE_AUDIO)
  bool is_muted() const override { return this->is_muted_; }

  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }
#endif

 protected:
  // Receives commands from HA
  void control(const media_player::MediaPlayerCall &call) override;

  QueueHandle_t resonate_controls_queue_;

  bool force_publish_state_{false};
#if defined(USE_RESONATE_AUDIO)
  static void sync_task(void *params);
  TaskHandle_t sync_task_handle_{nullptr};
  StaticTask_t sync_task_stack_;
  StackType_t *sync_task_stack_buffer_{nullptr};

  EventGroupHandle_t event_group_{nullptr};

  audio::AudioStreamInfo audio_stream_info_;
  ResonateCodecFormat codec_format_{ResonateCodecFormat::UNSUPPORTED};

  optional<uint8_t> volume_;

  bool task_stack_in_psram_{false};

  bool is_muted_{false};

  bool task_processing_{false};  // Indicates sync task is finished starting and processing audio

  int32_t pending_frames_;  // Number of frames sent to the speaker but not played

  speaker::Speaker *speaker_{nullptr};

  QueueHandle_t playback_progress_queue_;

  std::unique_ptr<ResonateChunkQueue> decoded_chunk_queue_;

  uint32_t single_frames_added_{0};
  uint32_t single_frames_removed_{0};
  uint32_t hard_sync_added_frames_{0};
  uint32_t hard_sync_removed_frames_{0};
  uint32_t audible_syncs_{0};
#endif
};

}  // namespace resonate
}  // namespace esphome
#endif
