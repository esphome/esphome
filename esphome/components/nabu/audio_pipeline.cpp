#ifdef USE_ESP_IDF

#include "audio_pipeline.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nabu {

static const size_t FILE_BUFFER_SIZE = 32 * 1024;
static const size_t FILE_RING_BUFFER_SIZE = 64 * 1024;
static const size_t BUFFER_SIZE_SAMPLES = 32768;
static const size_t BUFFER_SIZE_BYTES = BUFFER_SIZE_SAMPLES * sizeof(int16_t);

static const uint32_t READER_TASK_STACK_SIZE = 5 * 1024;
static const uint32_t DECODER_TASK_STACK_SIZE = 3 * 1024;
static const uint32_t RESAMPLER_TASK_STACK_SIZE = 3 * 1024;

static const size_t INFO_ERROR_QUEUE_COUNT = 5;

static const char *const TAG = "nabu_media_player.pipeline";

enum EventGroupBits : uint32_t {
  // The stop() function clears all unfinished bits
  // MESSAGE_* bits are only set by their respective tasks

  // Stops all activity in the pipeline elements and set by stop() or by each task
  PIPELINE_COMMAND_STOP = (1 << 0),

  // Read audio from an HTTP source; cleared by reader task and set by start(uri,...)
  READER_COMMAND_INIT_HTTP = (1 << 4),
  // Read audio from an audio file from the flash; cleared by reader task and set by start(media_file,...)
  READER_COMMAND_INIT_FILE = (1 << 5),

  // Audio file type is read after checking it is supported; cleared by decoder task
  READER_MESSAGE_LOADED_MEDIA_TYPE = (1 << 6),
  // Reader is done (either through a failure or just end of the stream); cleared by reader task
  READER_MESSAGE_FINISHED = (1 << 7),
  // Error reading the file; cleared by get_state()
  READER_MESSAGE_ERROR = (1 << 8),

  // Decoder has determined the stream information; cleared by resampler
  DECODER_MESSAGE_LOADED_STREAM_INFO = (1 << 11),
  // Decoder is done (either through a faiilure or the end of the stream); cleared by decoder task
  DECODER_MESSAGE_FINISHED = (1 << 12),
  // Error decoding the file; cleared by get_state() by decoder task
  DECODER_MESSAGE_ERROR = (1 << 13),

  // Resampler is done (either through a failure or the end of the stream); cleared by resampler task
  RESAMPLER_MESSAGE_FINISHED = (1 << 17),
  // Error resampling the file; cleared by get_state()
  RESAMPLER_MESSAGE_ERROR = (1 << 18),

  // Cleared by respective tasks
  FINISHED_BITS = READER_MESSAGE_FINISHED | DECODER_MESSAGE_FINISHED | RESAMPLER_MESSAGE_FINISHED,
  UNFINISHED_BITS = ~(FINISHED_BITS | 0xff000000),  // Only 24 bits are valid for the event group, so make sure first 8
                                                    // bits of uint32 are not set; cleared by stop()
};

AudioPipeline::AudioPipeline(AudioMixer *mixer, AudioPipelineType pipeline_type) {
  this->mixer_ = mixer;
  this->pipeline_type_ = pipeline_type;
}

esp_err_t AudioPipeline::start(const std::string &uri, uint32_t target_sample_rate, const std::string &task_name,
                               UBaseType_t priority) {
  esp_err_t err = this->common_start_(target_sample_rate, task_name, priority);

  if (err == ESP_OK) {
    this->current_uri_ = uri;
    xEventGroupSetBits(this->event_group_, READER_COMMAND_INIT_HTTP);
  }

  return err;
}

esp_err_t AudioPipeline::start(MediaFile *media_file, uint32_t target_sample_rate, const std::string &task_name,
                               UBaseType_t priority) {
  esp_err_t err = this->common_start_(target_sample_rate, task_name, priority);

  if (err == ESP_OK) {
    this->current_media_file_ = media_file;
    xEventGroupSetBits(this->event_group_, READER_COMMAND_INIT_FILE);
  }

  return err;
}

esp_err_t AudioPipeline::allocate_buffers_() {
  if (this->raw_file_ring_buffer_ == nullptr)
    this->raw_file_ring_buffer_ = RingBuffer::create(FILE_RING_BUFFER_SIZE);

  if (this->decoded_ring_buffer_ == nullptr)
    this->decoded_ring_buffer_ = RingBuffer::create(BUFFER_SIZE_BYTES);

  if ((this->raw_file_ring_buffer_ == nullptr) || (this->decoded_ring_buffer_ == nullptr)) {
    return ESP_ERR_NO_MEM;
  }

  if (this->read_task_stack_buffer_ == nullptr)
    this->read_task_stack_buffer_ = (StackType_t *) malloc(READER_TASK_STACK_SIZE);

  if (this->decode_task_stack_buffer_ == nullptr)
    this->decode_task_stack_buffer_ = (StackType_t *) malloc(DECODER_TASK_STACK_SIZE);

  if (this->resample_task_stack_buffer_ == nullptr)
    this->resample_task_stack_buffer_ = (StackType_t *) malloc(RESAMPLER_TASK_STACK_SIZE);

  if ((this->read_task_stack_buffer_ == nullptr) || (this->decode_task_stack_buffer_ == nullptr) ||
      (this->resample_task_stack_buffer_ == nullptr)) {
    return ESP_ERR_NO_MEM;
  }

  if (this->event_group_ == nullptr)
    this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  if (this->info_error_queue_ == nullptr)
    this->info_error_queue_ = xQueueCreate(INFO_ERROR_QUEUE_COUNT, sizeof(InfoErrorEvent));

  if (this->info_error_queue_ == nullptr)
    return ESP_ERR_NO_MEM;

  return ESP_OK;
}

esp_err_t AudioPipeline::common_start_(uint32_t target_sample_rate, const std::string &task_name,
                                       UBaseType_t priority) {
  esp_err_t err = this->allocate_buffers_();
  if (err != ESP_OK) {
    return err;
  }

  if (this->read_task_handle_ == nullptr) {
    this->read_task_handle_ =
        xTaskCreateStatic(AudioPipeline::read_task_, (task_name + "_read").c_str(), READER_TASK_STACK_SIZE,
                          (void *) this, priority, this->read_task_stack_buffer_, &this->read_task_stack_);
  }
  if (this->decode_task_handle_ == nullptr) {
    this->decode_task_handle_ =
        xTaskCreateStatic(AudioPipeline::decode_task_, (task_name + "_decode").c_str(), DECODER_TASK_STACK_SIZE,
                          (void *) this, priority, this->decode_task_stack_buffer_, &this->decode_task_stack_);
  }
  if (this->resample_task_handle_ == nullptr) {
    this->resample_task_handle_ =
        xTaskCreateStatic(AudioPipeline::resample_task_, (task_name + "_resample").c_str(), RESAMPLER_TASK_STACK_SIZE,
                          (void *) this, priority, this->resample_task_stack_buffer_, &this->resample_task_stack_);
  }

  if ((this->read_task_handle_ == nullptr) || (this->decode_task_handle_ == nullptr) ||
      (this->resample_task_handle_ == nullptr)) {
    return ESP_FAIL;
  }

  this->target_sample_rate_ = target_sample_rate;

  return this->stop();
}

AudioPipelineState AudioPipeline::get_state() {
  InfoErrorEvent event;
  if (this->info_error_queue_ != nullptr) {
    while (xQueueReceive(this->info_error_queue_, &event, 0)) {
      switch (event.source) {
        case InfoErrorSource::READER:
          if (event.err.has_value()) {
            ESP_LOGE(TAG, "Media reader encountered an error: %s", esp_err_to_name(event.err.value()));
          } else if (event.file_type.has_value()) {
            ESP_LOGD(TAG, "Reading %s file type", media_player_file_type_to_string(event.file_type.value()));
          }

          break;
        case InfoErrorSource::DECODER:
          if (event.err.has_value()) {
            ESP_LOGE(TAG, "Decoder encountered an error: %s", esp_err_to_name(event.err.value()));
          }

          if (event.audio_stream_info.has_value()) {
            ESP_LOGD(TAG, "Decoded audio has %d channels, %" PRId32 " Hz sample rate, and %d bits per sample",
                     event.audio_stream_info.value().channels, event.audio_stream_info.value().sample_rate,
                     event.audio_stream_info.value().bits_per_sample);
          }

          if (event.decoding_err.has_value()) {
            switch (event.decoding_err.value()) {
              case DecodingError::FAILED_HEADER:
                ESP_LOGE(TAG, "Failed to parse the file's header.");
                break;
              case DecodingError::INCOMPATIBLE_BITS_PER_SAMPLE:
                ESP_LOGE(TAG, "Incompatible bits per sample. Only 16 bits per sample is supported");
                break;
              case DecodingError::INCOMPATIBLE_CHANNELS:
                ESP_LOGE(TAG, "Incompatible number of channels. Only 1 or 2 channel audio is supported.");
                break;
            }
          }
          break;
        case InfoErrorSource::RESAMPLER:
          if (event.err.has_value()) {
            ESP_LOGE(TAG, "Resampler encountered an error: %s", esp_err_to_name(event.err.has_value()));
          } else if (event.resample_info.has_value()) {
            if (event.resample_info.value().resample) {
              ESP_LOGD(TAG, "Converting the audio sample rate");
            }
            if (event.resample_info.value().mono_to_stereo) {
              ESP_LOGD(TAG, "Converting mono channel audio to stereo channel audio");
            }
          }
          break;
      }
    }
  }

  EventBits_t event_bits = xEventGroupGetBits(this->event_group_);
  if (!this->read_task_handle_ && !this->decode_task_handle_ && !this->resample_task_handle_) {
    return AudioPipelineState::STOPPED;
  }

  if ((event_bits & READER_MESSAGE_ERROR)) {
    xEventGroupClearBits(this->event_group_, READER_MESSAGE_ERROR);
    return AudioPipelineState::ERROR_READING;
  }

  if ((event_bits & DECODER_MESSAGE_ERROR)) {
    xEventGroupClearBits(this->event_group_, DECODER_MESSAGE_ERROR);
    return AudioPipelineState::ERROR_DECODING;
  }

  if ((event_bits & RESAMPLER_MESSAGE_ERROR)) {
    xEventGroupClearBits(this->event_group_, RESAMPLER_MESSAGE_ERROR);
    return AudioPipelineState::ERROR_RESAMPLING;
  }

  if ((event_bits & READER_MESSAGE_FINISHED) && (event_bits & DECODER_MESSAGE_FINISHED) &&
      (event_bits & RESAMPLER_MESSAGE_FINISHED)) {
    return AudioPipelineState::STOPPED;
  }

  return AudioPipelineState::PLAYING;
}

esp_err_t AudioPipeline::stop() {
  xEventGroupSetBits(this->event_group_, PIPELINE_COMMAND_STOP);

  uint32_t event_group_bits = xEventGroupWaitBits(this->event_group_,
                                                  FINISHED_BITS,        // Bit message to read
                                                  pdFALSE,              // Clear the bits on exit
                                                  pdTRUE,               // Wait for all the bits,
                                                  pdMS_TO_TICKS(300));  // Duration to block/wait

  if (!(event_group_bits & READER_MESSAGE_FINISHED)) {
    // Reader failed to stop
    xEventGroupSetBits(this->event_group_, EventGroupBits::READER_MESSAGE_ERROR);
  }
  if (!(event_group_bits & DECODER_MESSAGE_FINISHED)) {
    // Ddecoder failed to stop
    xEventGroupSetBits(this->event_group_, EventGroupBits::DECODER_MESSAGE_ERROR);
  }
  if (!(event_group_bits & RESAMPLER_MESSAGE_FINISHED)) {
    // Resampler failed to stop
    xEventGroupSetBits(this->event_group_, EventGroupBits::RESAMPLER_MESSAGE_ERROR);
  }

  if ((event_group_bits & FINISHED_BITS) != FINISHED_BITS) {
    // Not all bits were set, so it timed out
    return ESP_ERR_TIMEOUT;
  }

  // Clear the ring buffer in the mixer; avoids playing incorrect audio when starting a new file while paused
  CommandEvent command_event;
  if (this->pipeline_type_ == AudioPipelineType::MEDIA) {
    command_event.command = CommandEventType::CLEAR_MEDIA;
  } else {
    command_event.command = CommandEventType::CLEAR_ANNOUNCEMENT;
  }
  this->mixer_->send_command(&command_event);

  xEventGroupClearBits(this->event_group_, UNFINISHED_BITS);
  this->reset_ring_buffers();

  return ESP_OK;
}

void AudioPipeline::reset_ring_buffers() {
  this->raw_file_ring_buffer_->reset();
  this->decoded_ring_buffer_->reset();
}

void AudioPipeline::suspend_tasks() {
  if (this->read_task_handle_ != nullptr) {
    vTaskSuspend(this->read_task_handle_);
  }
  if (this->decode_task_handle_ != nullptr) {
    vTaskSuspend(this->decode_task_handle_);
  }
  if (this->resample_task_handle_ != nullptr) {
    vTaskSuspend(this->resample_task_handle_);
  }
}

void AudioPipeline::resume_tasks() {
  if (this->read_task_handle_ != nullptr) {
    vTaskResume(this->read_task_handle_);
  }
  if (this->decode_task_handle_ != nullptr) {
    vTaskResume(this->decode_task_handle_);
  }
  if (this->resample_task_handle_ != nullptr) {
    vTaskResume(this->resample_task_handle_);
  }
}

void AudioPipeline::read_task_(void *params) {
  AudioPipeline *this_pipeline = (AudioPipeline *) params;

  while (true) {
    xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::READER_MESSAGE_FINISHED);

    // Wait until the pipeline notifies us the source of the media file
    EventBits_t event_bits =
        xEventGroupWaitBits(this_pipeline->event_group_,
                            READER_COMMAND_INIT_FILE | READER_COMMAND_INIT_HTTP,  // Bit message to read
                            pdTRUE,                                               // Clear the bit on exit
                            pdFALSE,                                              // Wait for all the bits,
                            portMAX_DELAY);                                       // Block indefinitely until bit is set

    xEventGroupClearBits(this_pipeline->event_group_, EventGroupBits::READER_MESSAGE_FINISHED);

    {
      InfoErrorEvent event;
      event.source = InfoErrorSource::READER;
      esp_err_t err = ESP_OK;

      AudioReader reader = AudioReader(this_pipeline->raw_file_ring_buffer_.get(), FILE_BUFFER_SIZE);

      if (event_bits & READER_COMMAND_INIT_FILE) {
        err = reader.start(this_pipeline->current_media_file_, this_pipeline->current_media_file_type_);
      } else {
        err = reader.start(this_pipeline->current_uri_, this_pipeline->current_media_file_type_);
      }
      if (err != ESP_OK) {
        // Send specific error message
        event.err = err;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);

        // Setting up the reader failed, stop the pipeline
        xEventGroupSetBits(this_pipeline->event_group_,
                           EventGroupBits::READER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
      } else {
        // Send the file type to the pipeline
        event.file_type = this_pipeline->current_media_file_type_;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);

        // Inform the decoder that the media type is available
        xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::READER_MESSAGE_LOADED_MEDIA_TYPE);
      }

      while (true) {
        event_bits = xEventGroupGetBits(this_pipeline->event_group_);

        if (event_bits & PIPELINE_COMMAND_STOP) {
          break;
        }

        AudioReaderState reader_state = reader.read();

        if (reader_state == AudioReaderState::FINISHED) {
          break;
        } else if (reader_state == AudioReaderState::FAILED) {
          xEventGroupSetBits(this_pipeline->event_group_,
                             EventGroupBits::READER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          break;
        }
      }
    }
  }
}

void AudioPipeline::decode_task_(void *params) {
  AudioPipeline *this_pipeline = (AudioPipeline *) params;

  while (true) {
    xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::DECODER_MESSAGE_FINISHED);

    // Wait until the reader notifies us that the media type is available
    EventBits_t event_bits = xEventGroupWaitBits(this_pipeline->event_group_,
                                                 READER_MESSAGE_LOADED_MEDIA_TYPE,  // Bit message to read
                                                 pdTRUE,                            // Clear the bit on exit
                                                 pdFALSE,                           // Wait for all the bits,
                                                 portMAX_DELAY);  // Block indefinitely until bit is set

    xEventGroupClearBits(this_pipeline->event_group_, EventGroupBits::DECODER_MESSAGE_FINISHED);

    {
      InfoErrorEvent event;
      event.source = InfoErrorSource::DECODER;

      std::unique_ptr<AudioDecoder> decoder = make_unique<AudioDecoder>(
          this_pipeline->raw_file_ring_buffer_.get(), this_pipeline->decoded_ring_buffer_.get(), FILE_BUFFER_SIZE);
      esp_err_t err = decoder->start(this_pipeline->current_media_file_type_);

      if (err != ESP_OK) {
        // Send specific error message
        event.err = err;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);

        // Setting up the decoder failed, stop the pipeline
        xEventGroupSetBits(this_pipeline->event_group_,
                           EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
      }

      bool has_stream_info = false;

      while (true) {
        event_bits = xEventGroupGetBits(this_pipeline->event_group_);

        if (event_bits & PIPELINE_COMMAND_STOP) {
          break;
        }

        // Stop gracefully if the reader has finished
        AudioDecoderState decoder_state = decoder->decode(event_bits & READER_MESSAGE_FINISHED);

        if (decoder_state == AudioDecoderState::FINISHED) {
          break;
        } else if (decoder_state == AudioDecoderState::FAILED) {
          if (!has_stream_info) {
            event.decoding_err = DecodingError::FAILED_HEADER;
            xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);
          }
          xEventGroupSetBits(this_pipeline->event_group_,
                             EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          break;
        }

        if (!has_stream_info && decoder->get_audio_stream_info().has_value()) {
          has_stream_info = true;

          this_pipeline->current_audio_stream_info_ = decoder->get_audio_stream_info().value();

          // Send the stream information to the pipeline
          event.audio_stream_info = this_pipeline->current_audio_stream_info_;

          if (this_pipeline->current_audio_stream_info_.bits_per_sample != 16) {
            // Error state, incompatible bits per sample
            event.decoding_err = DecodingError::INCOMPATIBLE_BITS_PER_SAMPLE;
            xEventGroupSetBits(this_pipeline->event_group_,
                               EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          } else if ((this_pipeline->current_audio_stream_info_.channels > 2)) {
            // Error state, incompatible number of channels
            event.decoding_err = DecodingError::INCOMPATIBLE_CHANNELS;
            xEventGroupSetBits(this_pipeline->event_group_,
                               EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          } else {
            // Inform the resampler that the stream information is available
            xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::DECODER_MESSAGE_LOADED_STREAM_INFO);
          }

          xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);
        }
      }
    }
  }
}

void AudioPipeline::resample_task_(void *params) {
  AudioPipeline *this_pipeline = (AudioPipeline *) params;

  while (true) {
    xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::RESAMPLER_MESSAGE_FINISHED);

    // Wait until the decoder notifies us that the stream information is available
    EventBits_t event_bits = xEventGroupWaitBits(this_pipeline->event_group_,
                                                 DECODER_MESSAGE_LOADED_STREAM_INFO,  // Bit message to read
                                                 pdTRUE,                              // Clear the bit on exit
                                                 pdFALSE,                             // Wait for all the bits,
                                                 portMAX_DELAY);  // Block indefinitely until bit is set

    xEventGroupClearBits(this_pipeline->event_group_, EventGroupBits::RESAMPLER_MESSAGE_FINISHED);

    {
      InfoErrorEvent event;
      event.source = InfoErrorSource::RESAMPLER;

      RingBuffer *output_ring_buffer = nullptr;

      if (this_pipeline->pipeline_type_ == AudioPipelineType::MEDIA) {
        output_ring_buffer = this_pipeline->mixer_->get_media_ring_buffer();
      } else {
        output_ring_buffer = this_pipeline->mixer_->get_announcement_ring_buffer();
      }

      AudioResampler resampler =
          AudioResampler(this_pipeline->decoded_ring_buffer_.get(), output_ring_buffer, BUFFER_SIZE_SAMPLES);

      esp_err_t err = resampler.start(this_pipeline->current_audio_stream_info_, this_pipeline->target_sample_rate_,
                                      this_pipeline->current_resample_info_);

      if (err != ESP_OK) {
        // Send specific error message
        event.err = err;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);

        // Setting up the resampler failed, stop the pipeline
        xEventGroupSetBits(this_pipeline->event_group_,
                           EventGroupBits::RESAMPLER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
      } else {
        event.resample_info = this_pipeline->current_resample_info_;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);
      }

      while (true) {
        event_bits = xEventGroupGetBits(this_pipeline->event_group_);

        if (event_bits & PIPELINE_COMMAND_STOP) {
          break;
        }

        // Stop gracefully if the decoder is done
        AudioResamplerState resampler_state = resampler.resample(event_bits & DECODER_MESSAGE_FINISHED);

        if (resampler_state == AudioResamplerState::FINISHED) {
          break;
        } else if (resampler_state == AudioResamplerState::FAILED) {
          xEventGroupSetBits(this_pipeline->event_group_,
                             EventGroupBits::RESAMPLER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          break;
        }
      }
    }
  }
}

}  // namespace nabu
}  // namespace esphome
#endif
