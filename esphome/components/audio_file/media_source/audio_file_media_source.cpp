#include "audio_file_media_source.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

namespace esphome::audio_file {

static const char *const TAG = "audio_file_media_source";

static constexpr uint32_t AUDIO_WRITE_TIMEOUT_MS = 50;
static constexpr size_t DECODER_TASK_STACK_SIZE = 5120;
static constexpr uint8_t DECODER_TASK_PRIORITY = 2;
static constexpr uint32_t PAUSE_POLL_DELAY_MS = 20;
static constexpr char URI_PREFIX[] = "audio-file://";

namespace {  // anonymous namespace for internal linkage

// audio::AudioFileType and micro_decoder::AudioFileType use different numeric layouts (audio's
// values shift with USE_AUDIO_*_SUPPORT defines; micro_decoder's are fixed and guarded by
// MICRO_DECODER_CODEC_*). The codec request flow in audio/__init__.py keeps the two sets of
// guards aligned, so a switch with matching #ifdefs covers all reachable cases.
micro_decoder::AudioFileType to_micro_decoder_type(audio::AudioFileType type) {
  switch (type) {
#ifdef USE_AUDIO_FLAC_SUPPORT
    case audio::AudioFileType::FLAC:
      return micro_decoder::AudioFileType::FLAC;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
    case audio::AudioFileType::MP3:
      return micro_decoder::AudioFileType::MP3;
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
    case audio::AudioFileType::OPUS:
      return micro_decoder::AudioFileType::OPUS;
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
    case audio::AudioFileType::WAV:
      return micro_decoder::AudioFileType::WAV;
#endif
    default:
      return micro_decoder::AudioFileType::NONE;
  }
}

}  // namespace

void AudioFileMediaSource::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Audio File Media Source:\n"
                "  Decoder Task Stack in PSRAM: %s",
                YESNO(this->decoder_task_stack_in_psram_));
}

void AudioFileMediaSource::setup() {
  this->disable_loop();

  micro_decoder::DecoderConfig config;
  config.audio_write_timeout_ms = AUDIO_WRITE_TIMEOUT_MS;
  config.decoder_priority = DECODER_TASK_PRIORITY;
  config.decoder_stack_size = DECODER_TASK_STACK_SIZE;
  config.decoder_stack_in_psram = this->decoder_task_stack_in_psram_;

  this->decoder_ = std::make_unique<micro_decoder::DecoderSource>(config);
  if (this->decoder_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate decoder");
    this->mark_failed();
    return;
  }
  this->decoder_->set_listener(this);
}

void AudioFileMediaSource::loop() { this->decoder_->loop(); }

bool AudioFileMediaSource::can_handle(const std::string &uri) const { return uri.starts_with(URI_PREFIX); }

// Called from the orchestrator's main loop, so no synchronization needed with loop()
bool AudioFileMediaSource::play_uri(const std::string &uri) {
  if (!this->is_ready() || this->is_failed() || this->status_has_error() || !this->has_listener()) {
    return false;
  }

  if (this->get_state() != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s': source is busy", uri.c_str());
    return false;
  }

  if (!uri.starts_with(URI_PREFIX)) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  const char *file_id = uri.c_str() + sizeof(URI_PREFIX) - 1;
  this->current_file_ = nullptr;
  for (const auto &named_file : get_named_audio_files()) {
    if (strcmp(named_file.file_id, file_id) == 0) {
      this->current_file_ = named_file.file;
      break;
    }
  }

  if (this->current_file_ == nullptr) {
    ESP_LOGE(TAG, "Unknown file: '%s'", file_id);
    return false;
  }

  micro_decoder::AudioFileType type = to_micro_decoder_type(this->current_file_->file_type);
  if (this->decoder_->play_buffer(this->current_file_->data, this->current_file_->length, type)) {
    this->pause_.store(false, std::memory_order_relaxed);
    this->enable_loop();
    return true;
  }

  ESP_LOGE(TAG, "Failed to start playback of '%s'", file_id);
  return false;
}

// Called from the orchestrator's main loop, so no synchronization needed with loop()
void AudioFileMediaSource::handle_command(media_source::MediaSourceCommand command) {
  switch (command) {
    case media_source::MediaSourceCommand::STOP:
      this->decoder_->stop();
      break;
    case media_source::MediaSourceCommand::PAUSE:
      // Only valid while actively playing; ignoring from IDLE/ERROR/PAUSED prevents the state
      // machine from getting stuck in PAUSED when no playback is active (which would block the
      // next play_uri() call via its IDLE-state precondition).
      if (this->get_state() != media_source::MediaSourceState::PLAYING)
        break;
      // PAUSE does not stop the decoder task. Instead, on_audio_write() returns 0 and temporarily
      // yields, which fills any internal buffering and applies back pressure that effectively
      // pauses the decoder task.
      this->set_state_(media_source::MediaSourceState::PAUSED);
      this->pause_.store(true, std::memory_order_relaxed);
      break;
    case media_source::MediaSourceCommand::PLAY:
      if (this->get_state() != media_source::MediaSourceState::PAUSED)
        break;
      this->set_state_(media_source::MediaSourceState::PLAYING);
      this->pause_.store(false, std::memory_order_relaxed);
      break;
    default:
      break;
  }
}

// Called from the decoder task. Forwards to the orchestrator's listener, which is responsible for
// being thread-safe with respect to its own audio writer.
size_t AudioFileMediaSource::on_audio_write(const uint8_t *data, size_t length, uint32_t timeout_ms) {
  if (this->pause_.load(std::memory_order_relaxed)) {
    vTaskDelay(pdMS_TO_TICKS(PAUSE_POLL_DELAY_MS));
    return 0;
  }
  return this->write_output(data, length, timeout_ms, this->stream_info_);
}

// Called from the decoder task before the first on_audio_write().
void AudioFileMediaSource::on_stream_info(const micro_decoder::AudioStreamInfo &info) {
  this->stream_info_ = audio::AudioStreamInfo(info.get_bits_per_sample(), info.get_channels(), info.get_sample_rate());
}

// microDecoder invokes on_state_change() from inside decoder_->loop(), so this runs on the main
// loop thread and it's safe to call set_state_() directly.
void AudioFileMediaSource::on_state_change(micro_decoder::DecoderState state) {
  switch (state) {
    case micro_decoder::DecoderState::IDLE:
      this->set_state_(media_source::MediaSourceState::IDLE);
      this->disable_loop();
      break;
    case micro_decoder::DecoderState::PLAYING:
      this->set_state_(media_source::MediaSourceState::PLAYING);
      break;
    case micro_decoder::DecoderState::FAILED:
      this->set_state_(media_source::MediaSourceState::ERROR);
      break;
    default:
      break;
  }
}

}  // namespace esphome::audio_file

#endif  // USE_ESP32
