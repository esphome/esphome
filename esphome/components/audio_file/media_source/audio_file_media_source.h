#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio_file/audio_file.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/core/component.h"

#include <micro_decoder/decoder_source.h>
#include <micro_decoder/types.h>

#include <atomic>
#include <memory>
#include <string>

namespace esphome::audio_file {

// Inherits from two unrelated listener-style interfaces:
//   - media_source::MediaSource: this source reports state and writes audio *to* an orchestrator
//     (the orchestrator calls set_listener() on us with a MediaSourceListener*).
//   - micro_decoder::DecoderListener: the underlying decoder calls back *into* us with decoded
//     audio and state changes (we call decoder_->set_listener(this) in setup()).
class AudioFileMediaSource : public Component, public media_source::MediaSource, public micro_decoder::DecoderListener {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->decoder_task_stack_in_psram_ = task_stack_in_psram; }

  // MediaSource interface implementation
  bool play_uri(const std::string &uri) override;
  void handle_command(media_source::MediaSourceCommand command) override;
  bool can_handle(const std::string &uri) const override;

  // DecoderListener interface implementation
  size_t on_audio_write(const uint8_t *data, size_t length, uint32_t timeout_ms) override;
  void on_stream_info(const micro_decoder::AudioStreamInfo &info) override;
  void on_state_change(micro_decoder::DecoderState state) override;

 protected:
  std::unique_ptr<micro_decoder::DecoderSource> decoder_;
  audio::AudioStreamInfo stream_info_;
  audio::AudioFile *current_file_{nullptr};

  // Written from the main loop in handle_command(), read from the decoder task in
  // on_audio_write(). Must be atomic to avoid a data race.
  std::atomic<bool> pause_{false};
  bool decoder_task_stack_in_psram_{false};
};

}  // namespace esphome::audio_file

#endif  // USE_ESP32
