#include "audio_pipeline.h"

#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace speaker {

static const uint32_t INITIAL_BUFFER_MS = 1000;  // Start playback after buffering this duration of the file

static const uint32_t READ_TASK_STACK_SIZE = 5 * 1024;
static const uint32_t DECODE_TASK_STACK_SIZE = 3 * 1024;

static const uint32_t INFO_ERROR_QUEUE_COUNT = 5;

static const char *const TAG = "speaker_media_player.pipeline";

enum EventGroupBits : uint32_t {
  // MESSAGE_* bits are only set by their respective tasks

  // Stops all activity in the pipeline elements; cleared by process_state() and set by stop() or by each task
  PIPELINE_COMMAND_STOP = (1 << 0),

  // Read audio from an HTTP source; cleared by reader task and set by start_url
  READER_COMMAND_INIT_HTTP = (1 << 4),
  // Read audio from an audio file from the flash; cleared by reader task and set by start_file
  READER_COMMAND_INIT_FILE = (1 << 5),

  // Audio file type is read after checking it is supported; cleared by decoder task
  READER_MESSAGE_LOADED_MEDIA_TYPE = (1 << 6),
  // Reader is done (either through a failure or just end of the stream); cleared by reader task
  READER_MESSAGE_FINISHED = (1 << 7),
  // Error reading the file; cleared by process_state()
  READER_MESSAGE_ERROR = (1 << 8),

  // Decoder is done (either through a faiilure or the end of the stream); cleared by decoder task
  DECODER_MESSAGE_FINISHED = (1 << 12),
  // Error decoding the file; cleared by process_state() by decoder task
  DECODER_MESSAGE_ERROR = (1 << 13),
};

AudioPipeline::AudioPipeline(speaker::Speaker *speaker, size_t buffer_size, bool task_stack_in_psram,
                             std::string base_name, UBaseType_t priority)
    : base_name_(std::move(base_name)),
      priority_(priority),
      task_stack_in_psram_(task_stack_in_psram),
      speaker_(speaker),
      buffer_size_(buffer_size) {
  this->allocate_communications_();
  this->transfer_buffer_size_ = std::min(buffer_size_ / 4, DEFAULT_TRANSFER_BUFFER_SIZE);
}

void AudioPipeline::start_url(const std::string &uri) {
  if (this->is_playing_) {
    xEventGroupSetBits(this->event_group_, PIPELINE_COMMAND_STOP);
  }
  this->current_uri_ = uri;
  this->pending_url_ = true;
}

void AudioPipeline::start_file(audio::AudioFile *audio_file) {
  if (this->is_playing_) {
    xEventGroupSetBits(this->event_group_, PIPELINE_COMMAND_STOP);
  }
  this->current_audio_file_ = audio_file;
  this->pending_file_ = true;
}

esp_err_t AudioPipeline::stop() {
  xEventGroupSetBits(this->event_group_, EventGroupBits::PIPELINE_COMMAND_STOP);

  return ESP_OK;
}
void AudioPipeline::set_pause_state(bool pause_state) {
  this->speaker_->set_pause_state(pause_state);

  this->pause_state_ = pause_state;
}

void AudioPipeline::suspend_tasks() {
  if (this->read_task_handle_ != nullptr) {
    vTaskSuspend(this->read_task_handle_);
  }
  if (this->decode_task_handle_ != nullptr) {
    vTaskSuspend(this->decode_task_handle_);
  }
}

void AudioPipeline::resume_tasks() {
  if (this->read_task_handle_ != nullptr) {
    vTaskResume(this->read_task_handle_);
  }
  if (this->decode_task_handle_ != nullptr) {
    vTaskResume(this->decode_task_handle_);
  }
}

AudioPipelineState AudioPipeline::process_state() {
  /*
   * Log items from info error queue
   */
  InfoErrorEvent event;
  if (this->info_error_queue_ != nullptr) {
    while (xQueueReceive(this->info_error_queue_, &event, 0)) {
      switch (event.source) {
        case InfoErrorSource::READER:
          if (event.err.has_value()) {
            ESP_LOGE(TAG, "Media reader encountered an error: %s", esp_err_to_name(event.err.value()));
          } else if (event.file_type.has_value()) {
            ESP_LOGD(TAG, "Reading %s file type", audio_file_type_to_string(event.file_type.value()));
          }

          break;
        case InfoErrorSource::DECODER:
          if (event.err.has_value()) {
            ESP_LOGE(TAG, "Decoder encountered an error: %s", esp_err_to_name(event.err.value()));
          }

          if (event.audio_stream_info.has_value()) {
            ESP_LOGD(TAG, "Decoded audio has %d channels, %" PRId32 " Hz sample rate, and %d bits per sample",
                     event.audio_stream_info.value().get_channels(), event.audio_stream_info.value().get_sample_rate(),
                     event.audio_stream_info.value().get_bits_per_sample());
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
      }
    }
  }

  /*
   * Determine the current state based on the event group bits and tasks' status
   */

  EventBits_t event_bits = xEventGroupGetBits(this->event_group_);

  if (this->pending_url_ || this->pending_file_) {
    // Init command pending
    if (!(event_bits & EventGroupBits::PIPELINE_COMMAND_STOP)) {
      // Only start if there is no pending stop command
      if ((this->read_task_handle_ == nullptr) || (this->decode_task_handle_ == nullptr)) {
        // At least one task isn't running
        this->start_tasks_();
      }

      if (this->pending_url_) {
        xEventGroupSetBits(this->event_group_, EventGroupBits::READER_COMMAND_INIT_HTTP);
        this->playback_ms_ = 0;
        this->pending_url_ = false;
      } else if (this->pending_file_) {
        xEventGroupSetBits(this->event_group_, EventGroupBits::READER_COMMAND_INIT_FILE);
        this->playback_ms_ = 0;
        this->pending_file_ = false;
      }

      this->is_playing_ = true;
      return AudioPipelineState::PLAYING;
    }
  }

  if ((event_bits & EventGroupBits::READER_MESSAGE_FINISHED) &&
      (!(event_bits & EventGroupBits::READER_MESSAGE_LOADED_MEDIA_TYPE) &&
       (event_bits & EventGroupBits::DECODER_MESSAGE_FINISHED))) {
    // Tasks are finished and there's no media in between the reader and decoder

    if (event_bits & EventGroupBits::PIPELINE_COMMAND_STOP) {
      // Stop command is fully processed, so clear the command bit
      xEventGroupClearBits(this->event_group_, EventGroupBits::PIPELINE_COMMAND_STOP);
    }

    if (!this->is_playing_) {
      // The tasks have been stopped for two ``process_state`` calls in a row, so delete the tasks
      if ((this->read_task_handle_ != nullptr) || (this->decode_task_handle_ != nullptr)) {
        this->delete_tasks_();
        this->speaker_->stop();
      }
    }
    this->is_playing_ = false;
    return AudioPipelineState::STOPPED;
  }

  if ((event_bits & EventGroupBits::READER_MESSAGE_ERROR)) {
    xEventGroupClearBits(this->event_group_, EventGroupBits::READER_MESSAGE_ERROR);
    return AudioPipelineState::ERROR_READING;
  }

  if ((event_bits & EventGroupBits::DECODER_MESSAGE_ERROR)) {
    xEventGroupClearBits(this->event_group_, EventGroupBits::DECODER_MESSAGE_ERROR);
    return AudioPipelineState::ERROR_DECODING;
  }

  if (this->pause_state_) {
    return AudioPipelineState::PAUSED;
  }

  if ((this->read_task_handle_ == nullptr) && (this->decode_task_handle_ == nullptr)) {
    // No tasks are running, so the pipeline is stopped.
    xEventGroupClearBits(this->event_group_, EventGroupBits::PIPELINE_COMMAND_STOP);
    return AudioPipelineState::STOPPED;
  }

  this->is_playing_ = true;
  return AudioPipelineState::PLAYING;
}

esp_err_t AudioPipeline::allocate_communications_() {
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

esp_err_t AudioPipeline::start_tasks_() {
  if (this->read_task_handle_ == nullptr) {
    if (this->read_task_stack_buffer_ == nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        this->read_task_stack_buffer_ = stack_allocator.allocate(READ_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        this->read_task_stack_buffer_ = stack_allocator.allocate(READ_TASK_STACK_SIZE);
      }
    }

    if (this->read_task_stack_buffer_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }

    if (this->read_task_handle_ == nullptr) {
      this->read_task_handle_ =
          xTaskCreateStatic(read_task, (this->base_name_ + "_read").c_str(), READ_TASK_STACK_SIZE, (void *) this,
                            this->priority_, this->read_task_stack_buffer_, &this->read_task_stack_);
    }

    if (this->read_task_handle_ == nullptr) {
      return ESP_ERR_INVALID_STATE;
    }
  }

  if (this->decode_task_handle_ == nullptr) {
    if (this->decode_task_stack_buffer_ == nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
      }
    }

    if (this->decode_task_stack_buffer_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }

    if (this->decode_task_handle_ == nullptr) {
      this->decode_task_handle_ =
          xTaskCreateStatic(decode_task, (this->base_name_ + "_decode").c_str(), DECODE_TASK_STACK_SIZE, (void *) this,
                            this->priority_, this->decode_task_stack_buffer_, &this->decode_task_stack_);
    }

    if (this->decode_task_handle_ == nullptr) {
      return ESP_ERR_INVALID_STATE;
    }
  }

  return ESP_OK;
}

void AudioPipeline::delete_tasks_() {
  if (this->read_task_handle_ != nullptr) {
    vTaskDelete(this->read_task_handle_);

    if (this->read_task_stack_buffer_ != nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        stack_allocator.deallocate(this->read_task_stack_buffer_, READ_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        stack_allocator.deallocate(this->read_task_stack_buffer_, READ_TASK_STACK_SIZE);
      }

      this->read_task_stack_buffer_ = nullptr;
      this->read_task_handle_ = nullptr;
    }
  }

  if (this->decode_task_handle_ != nullptr) {
    vTaskDelete(this->decode_task_handle_);

    if (this->decode_task_stack_buffer_ != nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        stack_allocator.deallocate(this->decode_task_stack_buffer_, DECODE_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        stack_allocator.deallocate(this->decode_task_stack_buffer_, DECODE_TASK_STACK_SIZE);
      }

      this->decode_task_stack_buffer_ = nullptr;
      this->decode_task_handle_ = nullptr;
    }
  }
}

void AudioPipeline::read_task(void *params) {
  AudioPipeline *this_pipeline = (AudioPipeline *) params;

  while (true) {
    xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::READER_MESSAGE_FINISHED);

    // Wait until the pipeline notifies us the source of the media file
    EventBits_t event_bits =
        xEventGroupWaitBits(this_pipeline->event_group_,
                            EventGroupBits::READER_COMMAND_INIT_FILE | EventGroupBits::READER_COMMAND_INIT_HTTP |
                                EventGroupBits::PIPELINE_COMMAND_STOP,  // Bit message to read
                            pdFALSE,                                    // Clear the bit on exit
                            pdFALSE,                                    // Wait for all the bits,
                            portMAX_DELAY);                             // Block indefinitely until bit is set

    if (!(event_bits & EventGroupBits::PIPELINE_COMMAND_STOP)) {
      xEventGroupClearBits(this_pipeline->event_group_, EventGroupBits::READER_MESSAGE_FINISHED |
                                                            EventGroupBits::READER_COMMAND_INIT_FILE |
                                                            EventGroupBits::READER_COMMAND_INIT_HTTP);
      InfoErrorEvent event;
      event.source = InfoErrorSource::READER;
      esp_err_t err = ESP_OK;

      std::unique_ptr<audio::AudioReader> reader =
          make_unique<audio::AudioReader>(this_pipeline->transfer_buffer_size_);

      if (event_bits & EventGroupBits::READER_COMMAND_INIT_FILE) {
        err = reader->start(this_pipeline->current_audio_file_, this_pipeline->current_audio_file_type_);
      } else {
        err = reader->start(this_pipeline->current_uri_, this_pipeline->current_audio_file_type_);
      }

      if (err == ESP_OK) {
        size_t file_ring_buffer_size = this_pipeline->buffer_size_;

        std::shared_ptr<RingBuffer> temp_ring_buffer;

        if (!this_pipeline->raw_file_ring_buffer_.use_count()) {
          temp_ring_buffer = RingBuffer::create(file_ring_buffer_size);
          this_pipeline->raw_file_ring_buffer_ = temp_ring_buffer;
        }

        if (!this_pipeline->raw_file_ring_buffer_.use_count()) {
          err = ESP_ERR_NO_MEM;
        } else {
          reader->add_sink(this_pipeline->raw_file_ring_buffer_);
        }
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
        event.file_type = this_pipeline->current_audio_file_type_;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);
        xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::READER_MESSAGE_LOADED_MEDIA_TYPE);
      }

      while (true) {
        event_bits = xEventGroupGetBits(this_pipeline->event_group_);

        if (event_bits & EventGroupBits::PIPELINE_COMMAND_STOP) {
          break;
        }

        audio::AudioReaderState reader_state = reader->read();

        if (reader_state == audio::AudioReaderState::FINISHED) {
          break;
        } else if (reader_state == audio::AudioReaderState::FAILED) {
          xEventGroupSetBits(this_pipeline->event_group_,
                             EventGroupBits::READER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          break;
        }
      }
      event_bits = xEventGroupGetBits(this_pipeline->event_group_);
      if ((event_bits & EventGroupBits::READER_MESSAGE_LOADED_MEDIA_TYPE) ||
          (this_pipeline->raw_file_ring_buffer_.use_count() == 1)) {
        // Decoder task hasn't started yet, so delay a bit before releasing ownership of the ring buffer
        delay(10);
      }
    }
  }
}

void AudioPipeline::decode_task(void *params) {
  AudioPipeline *this_pipeline = (AudioPipeline *) params;

  while (true) {
    xEventGroupSetBits(this_pipeline->event_group_, EventGroupBits::DECODER_MESSAGE_FINISHED);

    // Wait until the reader notifies us that the media type is available
    EventBits_t event_bits = xEventGroupWaitBits(this_pipeline->event_group_,
                                                 EventGroupBits::READER_MESSAGE_LOADED_MEDIA_TYPE |
                                                     EventGroupBits::PIPELINE_COMMAND_STOP,  // Bit message to read
                                                 pdFALSE,                                    // Clear the bit on exit
                                                 pdFALSE,                                    // Wait for all the bits,
                                                 portMAX_DELAY);  // Block indefinitely until bit is set

    if (!(event_bits & EventGroupBits::PIPELINE_COMMAND_STOP)) {
      xEventGroupClearBits(this_pipeline->event_group_,
                           EventGroupBits::DECODER_MESSAGE_FINISHED | EventGroupBits::READER_MESSAGE_LOADED_MEDIA_TYPE);
      InfoErrorEvent event;
      event.source = InfoErrorSource::DECODER;

      std::unique_ptr<audio::AudioDecoder> decoder =
          make_unique<audio::AudioDecoder>(this_pipeline->transfer_buffer_size_, this_pipeline->transfer_buffer_size_);

      esp_err_t err = decoder->start(this_pipeline->current_audio_file_type_);
      decoder->add_source(this_pipeline->raw_file_ring_buffer_);

      if (err != ESP_OK) {
        // Send specific error message
        event.err = err;
        xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);

        // Setting up the decoder failed, stop the pipeline
        xEventGroupSetBits(this_pipeline->event_group_,
                           EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
      }

      bool has_stream_info = false;
      bool started_playback = false;

      size_t initial_bytes_to_buffer = 0;

      while (true) {
        event_bits = xEventGroupGetBits(this_pipeline->event_group_);

        if (event_bits & EventGroupBits::PIPELINE_COMMAND_STOP) {
          break;
        }

        // Update pause state
        if (!started_playback) {
          if (!(event_bits & EventGroupBits::READER_MESSAGE_FINISHED)) {
            decoder->set_pause_output_state(true);
          } else {
            started_playback = true;
          }
        } else {
          decoder->set_pause_output_state(this_pipeline->pause_state_);
        }

        // Stop gracefully if the reader has finished
        audio::AudioDecoderState decoder_state = decoder->decode(event_bits & EventGroupBits::READER_MESSAGE_FINISHED);

        if ((decoder_state == audio::AudioDecoderState::DECODING) ||
            (decoder_state == audio::AudioDecoderState::FINISHED)) {
          this_pipeline->playback_ms_ = decoder->get_playback_ms();
        }

        if (decoder_state == audio::AudioDecoderState::FINISHED) {
          break;
        } else if (decoder_state == audio::AudioDecoderState::FAILED) {
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

          if (this_pipeline->current_audio_stream_info_.get_bits_per_sample() != 16) {
            // Error state, incompatible bits per sample
            event.decoding_err = DecodingError::INCOMPATIBLE_BITS_PER_SAMPLE;
            xEventGroupSetBits(this_pipeline->event_group_,
                               EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          } else if ((this_pipeline->current_audio_stream_info_.get_channels() > 2)) {
            // Error state, incompatible number of channels
            event.decoding_err = DecodingError::INCOMPATIBLE_CHANNELS;
            xEventGroupSetBits(this_pipeline->event_group_,
                               EventGroupBits::DECODER_MESSAGE_ERROR | EventGroupBits::PIPELINE_COMMAND_STOP);
          } else {
            // Send audio directly to the speaker
            this_pipeline->speaker_->set_audio_stream_info(this_pipeline->current_audio_stream_info_);
            decoder->add_sink(this_pipeline->speaker_);
          }

          initial_bytes_to_buffer = std::min(this_pipeline->current_audio_stream_info_.ms_to_bytes(INITIAL_BUFFER_MS),
                                             this_pipeline->buffer_size_ * 3 / 4);

          switch (this_pipeline->current_audio_file_type_) {
#ifdef USE_AUDIO_MP3_SUPPORT
            case audio::AudioFileType::MP3:
              initial_bytes_to_buffer /= 8;  // Estimate the MP3 compression factor is 8
              break;
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
            case audio::AudioFileType::FLAC:
              initial_bytes_to_buffer /= 2;  // Estimate the FLAC compression factor is 2
              break;
#endif
            default:
              break;
          }
          xQueueSend(this_pipeline->info_error_queue_, &event, portMAX_DELAY);
        }

        if (!started_playback && has_stream_info) {
          // Verify enough data is available before starting playback
          std::shared_ptr<RingBuffer> temp_ring_buffer = this_pipeline->raw_file_ring_buffer_.lock();
          if (temp_ring_buffer->available() >= initial_bytes_to_buffer) {
            started_playback = true;
          }
        }
      }
    }
  }
}

}  // namespace speaker
}  // namespace esphome

#endif
