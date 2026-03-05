#include "http_request_media_source_internal.h"

#ifdef USE_ESP32

#include "http_request_media_source.h"

#include "esphome/components/audio/audio_decoder.h"
#include "esphome/core/log.h"

namespace esphome::http_request {

namespace {  // anonymous namespace for internal linkage
struct AudioSinkAdapter : public audio::AudioSinkCallback {
  media_source::MediaSource *source;
  audio::AudioStreamInfo stream_info;

  size_t audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) override {
    return this->source->write_output(data, length, pdTICKS_TO_MS(ticks_to_wait), this->stream_info);
  }
};
}  // namespace

void decode_task(void *params) {
  HTTPRequestMediaSource *this_source = static_cast<HTTPRequestMediaSource *>(params);

  do {  // do-while(false) ensures RAII objects are destroyed on all exit paths via break

    xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_STARTING);

    // Wait until the reader notifies us that it's ready or receive a stop command
    EventBits_t event_bits =
        xEventGroupWaitBits(this_source->event_group_,
                            EventGroupBits::READER_READY | EventGroupBits::COMMAND_STOP,  // Bit message to read
                            pdFALSE,                                                      // Don't clear the bit on exit
                            pdFALSE,                                                      // Wait for any bit
                            portMAX_DELAY);  // Wait indefinitely; reader sets COMMAND_STOP on failure
    xEventGroupClearBits(this_source->event_group_, EventGroupBits::READER_READY);

    // Exit if stop was requested
    if (event_bits & EventGroupBits::COMMAND_STOP) {
      // Signal reader task so it doesn't wait forever for us to acquire the ring buffer.
      // Set COMMAND_STOP so the read task stops promptly; e.g., on timeout.
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_RINGBUF_ACQUIRED |
                                                        EventGroupBits::COMMAND_STOP |
                                                        EventGroupBits::DECODER_STOPPING);
      break;
    }

    size_t transfer_buffer_size = std::min(this_source->buffer_size_ / 4, DEFAULT_TRANSFER_BUFFER_SIZE);
    std::unique_ptr<audio::AudioDecoder> decoder =
        make_unique<audio::AudioDecoder>(transfer_buffer_size, transfer_buffer_size);

    esp_err_t err = decoder->start(this_source->current_audio_file_type_);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to start decoder: %s", esp_err_to_name(err));
      // Signal reader task so it doesn't wait forever for us to acquire the ring buffer
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_RINGBUF_ACQUIRED);
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP |
                                                        EventGroupBits::DECODER_STOPPING);
      break;
    }

    err = decoder->add_source(this_source->raw_file_ring_buffer_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add decoder source: %s", esp_err_to_name(err));
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_RINGBUF_ACQUIRED);
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP |
                                                        EventGroupBits::DECODER_STOPPING);
      break;
    }

    // Signal reader task that we've acquired the ring buffer shared_ptr
    xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_RINGBUF_ACQUIRED);

    xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_RUNNING);

    AudioSinkAdapter audio_sink;
    bool has_stream_info = false;

    while (true) {
      event_bits = xEventGroupGetBits(this_source->event_group_);

      if (event_bits & EventGroupBits::COMMAND_STOP) {
        break;
      }

      bool paused = (event_bits & EventGroupBits::COMMAND_PAUSE) != 0;
      decoder->set_pause_output_state(paused);
      if (paused) {
        xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_PAUSED);
        vTaskDelay(pdMS_TO_TICKS(20));
      } else {
        xEventGroupClearBits(this_source->event_group_, EventGroupBits::DECODER_PAUSED);
      }

      // Will stop gracefully once reader finished
      audio::AudioDecoderState decoder_state = decoder->decode(event_bits & EventGroupBits::READER_FINISHED);

      if (decoder_state == audio::AudioDecoderState::FINISHED) {
        ESP_LOGD(TAG, "Decoding finished");
        break;
      } else if (decoder_state == audio::AudioDecoderState::FAILED) {
        ESP_LOGE(TAG, "Decoding failed");
        xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
        break;
      }

      if (!has_stream_info && decoder->get_audio_stream_info().has_value()) {
        has_stream_info = true;

        audio::AudioStreamInfo stream_info = decoder->get_audio_stream_info().value();

        ESP_LOGD(TAG, "Bits per sample: %d, Channels: %d, Sample rate: %d", stream_info.get_bits_per_sample(),
                 stream_info.get_channels(), stream_info.get_sample_rate());

        if (stream_info.get_bits_per_sample() != 16 || stream_info.get_channels() > 2) {
          ESP_LOGE(TAG, "Incompatible audio stream. Only 16 bits per sample and 1 or 2 channels are supported");
          xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
          break;
        }

        audio_sink.source = this_source;
        audio_sink.stream_info = stream_info;
        esp_err_t err = decoder->add_sink(&audio_sink);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Failed to add sink: %s", esp_err_to_name(err));
          xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
          break;
        }
      }
    }

    xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_STOPPING);
  } while (false);

  // All RAII objects from the do-while block (decoder, audio_sink, etc.) are now destroyed.

  xEventGroupSetBits(this_source->event_group_, EventGroupBits::DECODER_STOPPED);
  vTaskSuspend(nullptr);  // Suspend this task indefinitely until the loop method deletes it
}

}  // namespace esphome::http_request

#endif  // USE_ESP32
