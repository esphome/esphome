#ifdef USE_ESP_IDF

#include "audio_mixer.h"

#include <dsp.h>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace nabu {

static const size_t INPUT_RING_BUFFER_SAMPLES = 24000;
static const size_t OUTPUT_BUFFER_SAMPLES = 8192;
static const size_t QUEUE_COUNT = 20;

static const uint32_t TASK_STACK_SIZE = 3072;
static const size_t TASK_DELAY_MS = 25;

static const int16_t MAX_AUDIO_SAMPLE_VALUE = INT16_MAX;
static const int16_t MIN_AUDIO_SAMPLE_VALUE = INT16_MIN;

esp_err_t AudioMixer::start(speaker::Speaker *speaker, const std::string &task_name, UBaseType_t priority) {
  esp_err_t err = this->allocate_buffers_();

  if (err != ESP_OK) {
    return err;
  }

  if (this->task_handle_ == nullptr) {
    this->task_handle_ = xTaskCreateStatic(AudioMixer::audio_mixer_task_, task_name.c_str(), TASK_STACK_SIZE,
                                           (void *) this, priority, this->stack_buffer_, &this->task_stack_);
  }

  if (this->task_handle_ == nullptr) {
    return ESP_FAIL;
  }

  this->speaker_ = speaker;

  return ESP_OK;
}

void AudioMixer::stop() {
  vTaskDelete(this->task_handle_);
  this->task_handle_ = nullptr;

  xQueueReset(this->event_queue_);
  xQueueReset(this->command_queue_);
}

void AudioMixer::suspend_task() {
  if (this->task_handle_ != nullptr) {
    vTaskSuspend(this->task_handle_);
  }
}

void AudioMixer::resume_task() {
  if (this->task_handle_ != nullptr) {
    vTaskResume(task_handle_);
  }
}

void AudioMixer::audio_mixer_task_(void *params) {
  AudioMixer *this_mixer = (AudioMixer *) params;

  TaskEvent event;
  CommandEvent command_event;

  ExternalRAMAllocator<int16_t> allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);
  int16_t *media_buffer = allocator.allocate(OUTPUT_BUFFER_SAMPLES);
  int16_t *announcement_buffer = allocator.allocate(OUTPUT_BUFFER_SAMPLES);
  int16_t *combination_buffer = allocator.allocate(OUTPUT_BUFFER_SAMPLES);

  int16_t *combination_buffer_current = combination_buffer;
  size_t combination_buffer_length = 0;

  if ((media_buffer == nullptr) || (announcement_buffer == nullptr)) {
    event.type = EventType::WARNING;
    event.err = ESP_ERR_NO_MEM;
    xQueueSend(this_mixer->event_queue_, &event, portMAX_DELAY);

    event.type = EventType::STOPPED;
    event.err = ESP_OK;
    xQueueSend(this_mixer->event_queue_, &event, portMAX_DELAY);

    while (true) {
      delay(TASK_DELAY_MS);
    }

    return;
  }

  // Handles media stream pausing
  bool transfer_media = true;

  // Parameters to control the ducking dB reduction and its transitions
  // There is a built in negative sign; e.g., reducing by 5 dB is changing the gain by -5 dB
  int8_t target_ducking_db_reduction = 0;
  int8_t current_ducking_db_reduction = 0;

  // Each step represents a change in 1 dB. Positive 1 means the dB reduction is increasing. Negative 1 means the dB
  // reduction is decreasing.
  int8_t db_change_per_ducking_step = 1;

  size_t ducking_transition_samples_remaining = 0;
  size_t samples_per_ducking_step = 0;

  event.type = EventType::STARTED;
  xQueueSend(this_mixer->event_queue_, &event, portMAX_DELAY);

  while (true) {
    if (xQueueReceive(this_mixer->command_queue_, &command_event, 0) == pdTRUE) {
      if (command_event.command == CommandEventType::STOP) {
        break;
      } else if (command_event.command == CommandEventType::DUCK) {
        if (target_ducking_db_reduction != command_event.decibel_reduction) {
          current_ducking_db_reduction = target_ducking_db_reduction;

          target_ducking_db_reduction = command_event.decibel_reduction;

          uint8_t total_ducking_steps = 0;
          if (target_ducking_db_reduction > current_ducking_db_reduction) {
            // The dB reduction level is increasing (which results in quieter audio)
            total_ducking_steps = target_ducking_db_reduction - current_ducking_db_reduction - 1;
            db_change_per_ducking_step = 1;
          } else {
            // The dB reduction level is decreasing (which results in louder audio)
            total_ducking_steps = current_ducking_db_reduction - target_ducking_db_reduction - 1;
            db_change_per_ducking_step = -1;
          }
          if (total_ducking_steps > 0) {
            ducking_transition_samples_remaining = command_event.transition_samples;

            samples_per_ducking_step = ducking_transition_samples_remaining / total_ducking_steps;
          } else {
            ducking_transition_samples_remaining = 0;
          }
        }
      } else if (command_event.command == CommandEventType::PAUSE_MEDIA) {
        transfer_media = false;
      } else if (command_event.command == CommandEventType::RESUME_MEDIA) {
        transfer_media = true;
      } else if (command_event.command == CommandEventType::CLEAR_MEDIA) {
        this_mixer->media_ring_buffer_->reset();
      } else if (command_event.command == CommandEventType::CLEAR_ANNOUNCEMENT) {
        this_mixer->announcement_ring_buffer_->reset();
      }
    }

    if (combination_buffer_length > 0) {
      size_t output_bytes_written = this_mixer->speaker_->play((uint8_t *) combination_buffer,
                                                               combination_buffer_length, pdMS_TO_TICKS(TASK_DELAY_MS));
      combination_buffer_length -= output_bytes_written;
      if ((combination_buffer_length > 0) && (output_bytes_written > 0)) {
        memmove(combination_buffer, combination_buffer + output_bytes_written / sizeof(int16_t),
                combination_buffer_length);
      }
    } else {
      size_t media_available = this_mixer->media_ring_buffer_->available();
      size_t announcement_available = this_mixer->announcement_ring_buffer_->available();

      if (media_available * transfer_media + announcement_available > 0) {
        size_t bytes_to_read = OUTPUT_BUFFER_SAMPLES * sizeof(int16_t);

        if (media_available * transfer_media > 0) {
          bytes_to_read = std::min(bytes_to_read, media_available);
        }

        if (announcement_available > 0) {
          bytes_to_read = std::min(bytes_to_read, announcement_available);
        }

        if (bytes_to_read > 0) {
          size_t media_bytes_read = 0;
          if (media_available * transfer_media > 0) {
            media_bytes_read = this_mixer->media_ring_buffer_->read((void *) media_buffer, bytes_to_read, 0);
            if (media_bytes_read > 0) {
              size_t samples_read = media_bytes_read / sizeof(int16_t);
              if (ducking_transition_samples_remaining > 0) {
                // Ducking level is still transitioning

                size_t samples_left = ducking_transition_samples_remaining;

                // There may be more than one step worth of samples to duck in the buffers, so manage positions
                int16_t *current_media_buffer = media_buffer;

                size_t samples_left_in_step = samples_left % samples_per_ducking_step;
                if (samples_left_in_step == 0) {
                  // Start of a new ducking step

                  current_ducking_db_reduction += db_change_per_ducking_step;
                  samples_left_in_step = samples_per_ducking_step;
                }
                size_t samples_left_to_duck = std::min(samples_left_in_step, samples_read);

                size_t total_samples_ducked = 0;

                while (samples_left_to_duck > 0) {
                  // Ensure we only point to valid index in the Q15 scaling factor table
                  uint8_t safe_db_reduction_index =
                      clamp<uint8_t>(current_ducking_db_reduction, 0, decibel_reduction_table.size() - 1);

                  int16_t q15_scale_factor = decibel_reduction_table[safe_db_reduction_index];
                  this_mixer->scale_audio_samples_(current_media_buffer, current_media_buffer, q15_scale_factor,
                                                   samples_left_to_duck);

                  current_media_buffer += samples_left_to_duck;

                  samples_read -= samples_left_to_duck;
                  samples_left -= samples_left_to_duck;

                  total_samples_ducked += samples_left_to_duck;

                  samples_left_in_step = samples_left % samples_per_ducking_step;
                  if (samples_left_in_step == 0) {
                    // Start of a new step

                    current_ducking_db_reduction += db_change_per_ducking_step;
                    samples_left_in_step = samples_per_ducking_step;
                  }
                  samples_left_to_duck = std::min(samples_left_in_step, samples_read);
                }
              } else if (target_ducking_db_reduction > 0) {
                // We still need to apply a ducking scaling, but we are done transitioning

                uint8_t safe_db_reduction_index =
                    clamp<uint8_t>(target_ducking_db_reduction, 0, decibel_reduction_table.size() - 1);

                int16_t q15_scale_factor = decibel_reduction_table[safe_db_reduction_index];
                this_mixer->scale_audio_samples_(media_buffer, media_buffer, q15_scale_factor, samples_read);
              }
            }
          }

          size_t announcement_bytes_read = 0;
          if (announcement_available > 0) {
            announcement_bytes_read =
                this_mixer->announcement_ring_buffer_->read((void *) announcement_buffer, bytes_to_read, 0);
          }

          if ((media_bytes_read > 0) && (announcement_bytes_read > 0)) {
            // We have both a media and an announcement stream, so mix them together

            size_t samples_read = bytes_to_read / sizeof(int16_t);

            this_mixer->mix_audio_samples_without_clipping_(media_buffer, announcement_buffer, combination_buffer,
                                                            samples_read);

            combination_buffer_length = samples_read * sizeof(int16_t);
          } else if (media_bytes_read > 0) {
            memcpy(combination_buffer, media_buffer, media_bytes_read);
            combination_buffer_length = media_bytes_read;
          } else if (announcement_bytes_read > 0) {
            memcpy(combination_buffer, announcement_buffer, announcement_bytes_read);
            combination_buffer_length = announcement_bytes_read;
          }

          size_t samples_written = combination_buffer_length / sizeof(int16_t);
          if (ducking_transition_samples_remaining > 0) {
            ducking_transition_samples_remaining -= std::min(samples_written, ducking_transition_samples_remaining);
          }
        }
      } else {
        // No audio data available in either buffer

        delay(TASK_DELAY_MS);
      }
    }
  }

  event.type = EventType::STOPPING;
  xQueueSend(this_mixer->event_queue_, &event, portMAX_DELAY);

  this_mixer->reset_ring_buffers_();
  allocator.deallocate(media_buffer, OUTPUT_BUFFER_SAMPLES);
  allocator.deallocate(announcement_buffer, OUTPUT_BUFFER_SAMPLES);
  allocator.deallocate(combination_buffer, OUTPUT_BUFFER_SAMPLES);

  event.type = EventType::STOPPED;
  xQueueSend(this_mixer->event_queue_, &event, portMAX_DELAY);

  while (true) {
    delay(TASK_DELAY_MS);
  }
}

esp_err_t AudioMixer::allocate_buffers_() {
  if (this->media_ring_buffer_ == nullptr)
    this->media_ring_buffer_ = RingBuffer::create(INPUT_RING_BUFFER_SAMPLES * sizeof(int16_t));

  if (this->announcement_ring_buffer_ == nullptr)
    this->announcement_ring_buffer_ = RingBuffer::create(INPUT_RING_BUFFER_SAMPLES * sizeof(int16_t));

  if ((this->announcement_ring_buffer_ == nullptr) || (this->media_ring_buffer_ == nullptr)) {
    return ESP_ERR_NO_MEM;
  }

  if (this->stack_buffer_ == nullptr)
    this->stack_buffer_ = (StackType_t *) malloc(TASK_STACK_SIZE);

  if (this->stack_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  if (this->event_queue_ == nullptr)
    this->event_queue_ = xQueueCreate(QUEUE_COUNT, sizeof(TaskEvent));

  if (this->command_queue_ == nullptr)
    this->command_queue_ = xQueueCreate(QUEUE_COUNT, sizeof(CommandEvent));

  if ((this->event_queue_ == nullptr) || (this->command_queue_ == nullptr)) {
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

void AudioMixer::reset_ring_buffers_() {
  this->media_ring_buffer_->reset();
  this->announcement_ring_buffer_->reset();
}

void AudioMixer::mix_audio_samples_without_clipping_(int16_t *media_buffer, int16_t *announcement_buffer,
                                                     int16_t *combination_buffer, size_t samples_to_mix) {
  // We first test adding the two clips samples together and check for any clipping
  // We want the announcement volume to be consistent, regardless if media is playing or not
  // If there is clipping, we determine what factor we need to multiply that media sample by to avoid it
  // We take the smallest factor necessary for all the samples so the media volume is consistent on this batch
  // of samples
  // Note: This may not be the best approach. Adding 2 audio samples together makes both sound louder, even if
  // we are not clipping. As a result, the mixed announcement will sound louder (by around 3dB if the audio
  // streams are independent?) than if it were by itself.

  int16_t q15_scaling_factor = MAX_AUDIO_SAMPLE_VALUE;

  for (int i = 0; i < samples_to_mix; ++i) {
    int32_t added_sample = static_cast<int32_t>(media_buffer[i]) + static_cast<int32_t>(announcement_buffer[i]);

    if ((added_sample > MAX_AUDIO_SAMPLE_VALUE) || (added_sample < MIN_AUDIO_SAMPLE_VALUE)) {
      // The largest magnitude the media sample can be to avoid clipping (converted to Q30 fixed point)
      int32_t q30_media_sample_safe_max =
          static_cast<int32_t>(std::abs(MIN_AUDIO_SAMPLE_VALUE) - std::abs(announcement_buffer[i])) << 15;

      // Actual media sample value (Q15 number stored in an int32 for future division)
      int32_t media_sample_value = abs(media_buffer[i]);

      // Calculation to perform the Q15 division for media_sample_safe_max/media_sample_value
      // Reference: https://sestevenson.wordpress.com/2010/09/20/fixed-point-division-2/ (accessed August 15,
      // 2024)
      int16_t necessary_q15_factor = static_cast<int16_t>(q30_media_sample_safe_max / media_sample_value);
      // Take the minimum scaling factor (the smaller the factor, the more it needs to be scaled down)
      q15_scaling_factor = std::min(necessary_q15_factor, q15_scaling_factor);
    } else {
      // Store the combined samples in the combination buffer. If we do not need to scale, then the samples are already
      // mixed.
      combination_buffer[i] = added_sample;
    }
  }

  if (q15_scaling_factor < MAX_AUDIO_SAMPLE_VALUE) {
    // Need to scale to avoid clipping

    this->scale_audio_samples_(media_buffer, media_buffer, q15_scaling_factor, samples_to_mix);

    // Mix both stream by adding them together with no bitshift
    // The dsps_add functions have the following inputs:
    // (buffer 1, buffer 2, output buffer, number of samples, buffer 1 step, buffer 2 step, output, buffer step,
    // bitshift)
    dsps_add_s16(media_buffer, announcement_buffer, combination_buffer, samples_to_mix, 1, 1, 1, 0);
  }
}

void AudioMixer::scale_audio_samples_(int16_t *audio_samples, int16_t *output_buffer, int16_t scale_factor,
                                      size_t samples_to_scale) {
  // Scale the audio samples and store them in the output buffer
  dsps_mulc_s16(audio_samples, output_buffer, samples_to_scale, scale_factor, 1, 1);
}

}  // namespace nabu
}  // namespace esphome
#endif
