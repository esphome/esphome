#include "http_request_media_source_internal.h"

#ifdef USE_ESP32

#include "http_request_media_source.h"

#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/core/log.h"

namespace esphome::http_request {

/// @brief Open an HTTP connection with retries on transient failures
/// Sets READER_ERROR and COMMAND_STOP event bits on failure.
/// @param client The HTTP client to use for requests
/// @param uri The URI to fetch
/// @param event_group Event group to check for stop commands between retries; error bits set on failure
/// @return A valid HttpContainer on success, or nullptr on failure
static std::shared_ptr<HttpContainer> open_connection(HttpRequestComponent *client, const std::string &uri,
                                                      EventGroupHandle_t event_group) {
  static const std::vector<std::string> COLLECT_HEADERS = {"content-type"};
  std::vector<Header> headers = {};

  std::shared_ptr<HttpContainer> container;
  for (uint8_t attempt = 0; attempt < MAX_CONNECTION_ATTEMPTS; ++attempt) {
    if (xEventGroupGetBits(event_group) & EventGroupBits::COMMAND_STOP) {
      return nullptr;
    }

    container = client->get(uri, headers, COLLECT_HEADERS);

    if (container != nullptr && is_success(container->status_code)) {
      return container;
    }

    // Clean up failed attempt
    if (container != nullptr) {
      ESP_LOGW(TAG, "Attempt %u failed with status %d", attempt + 1, container->status_code);
      container->end();
      container.reset();
    } else {
      ESP_LOGW(TAG, "Attempt %u failed to connect", attempt + 1);
    }

    if (attempt + 1 < MAX_CONNECTION_ATTEMPTS) {
      vTaskDelay(pdMS_TO_TICKS(1000));  // wait before retry
    }
  }

  ESP_LOGE(TAG, "Request failed after %u attempts", MAX_CONNECTION_ATTEMPTS);
  xEventGroupSetBits(event_group, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
  return nullptr;
}

/// @brief Stream HTTP response data into a ring buffer via a transfer buffer
/// Sets READER_ERROR and COMMAND_STOP event bits on timeout.
/// @param container The active HTTP connection to read from
/// @param transfer_buffer The transfer buffer to use for batching writes
/// @param event_group Event group to check for stop commands; error bits set on timeout
static void stream_to_ring_buffer(std::shared_ptr<HttpContainer> &container,
                                  std::unique_ptr<audio::AudioSinkTransferBuffer> &transfer_buffer,
                                  EventGroupHandle_t event_group) {
  uint32_t last_data_time = millis();

  // We don't use http_read_loop_result() here because:
  // - We're on a dedicated FreeRTOS task, not the main loop
  // - esp_http_client_read() blocks, so tight-spinning isn't a concern
  // - Transient errors; e.g., -ESP_ERR_HTTP_EAGAIN, should retry, not fail immediately
  while (true) {
    if (xEventGroupGetBits(event_group) & EventGroupBits::COMMAND_STOP) {
      return;
    }

    // Transfer any buffered data to the ring buffer
    transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);

    if (transfer_buffer->available() == 0) {
      int received_len = container->read(transfer_buffer->get_buffer_end(), transfer_buffer->free());

      if (received_len > 0) {
        last_data_time = millis();
        transfer_buffer->increase_buffer_length(received_len);
        continue;
      }

      if (received_len == 0 && container->is_read_complete()) {
        // Flush remaining buffered data to the ring buffer, retrying until empty
        while (transfer_buffer->available() > 0) {
          if (xEventGroupGetBits(event_group) & EventGroupBits::COMMAND_STOP) {
            return;
          }
          transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);
        }
        ESP_LOGD(TAG, "Reader finished");
        return;
      }

      // Connection closed prematurely; fail immediately
      if (received_len == HTTP_ERROR_CONNECTION_CLOSED) {
        xEventGroupSetBits(event_group, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
        return;
      }

      // No data yet or transient transport timeout (e.g., EAGAIN)
      if (millis() - last_data_time >= CONNECTION_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Reader timed out");
        xEventGroupSetBits(event_group, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
        return;
      }

      vTaskDelay(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));
    }
  }
}

void read_task(void *params) {
  HTTPRequestMediaSource *this_source = static_cast<HTTPRequestMediaSource *>(params);

  // Holds ring buffer alive until the decode task acquires its own shared_ptr reference.
  // Declared outside the inner scope so it survives past transfer_buffer cleanup.
  std::shared_ptr<RingBuffer> ring_buffer_guard;

  do {  // do-while(false) ensures RAII objects are destroyed on all exit paths via break
    // Open HTTP connection with retries (sets error bits on failure)
    std::shared_ptr<HttpContainer> container =
        open_connection(this_source->get_parent(), this_source->current_uri_, this_source->event_group_);

    if (container == nullptr) {
      break;
    }

    // Detect audio file type from Content-Type header or URL
    std::string content_type = container->get_response_header("content-type");
    this_source->current_audio_file_type_ =
        audio::detect_audio_file_type(content_type.c_str(), this_source->current_uri_.c_str());

    if (this_source->current_audio_file_type_ == audio::AudioFileType::NONE) {
      ESP_LOGE(TAG, "Unable to determine file type");
      container->end();
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
      break;
    }

    // Create transfer buffer for efficient writes to ring buffer
    size_t transfer_buffer_size = std::min(this_source->buffer_size_ / 4, DEFAULT_TRANSFER_BUFFER_SIZE);
    std::unique_ptr<audio::AudioSinkTransferBuffer> transfer_buffer =
        audio::AudioSinkTransferBuffer::create(transfer_buffer_size);

    if (transfer_buffer == nullptr) {
      ESP_LOGE(TAG, "Failed to create transfer buffer");
      container->end();
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
      break;
    }

    {  // Ensures temp_ring_buffer falls out of scope and deallocates
      std::shared_ptr<RingBuffer> temp_ring_buffer;
      if (this_source->raw_file_ring_buffer_.expired()) {
        temp_ring_buffer = RingBuffer::create(this_source->buffer_size_);
        this_source->raw_file_ring_buffer_ = temp_ring_buffer;
      }

      if (this_source->raw_file_ring_buffer_.expired()) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        container->end();
        xEventGroupSetBits(this_source->event_group_, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
        break;
      }

      transfer_buffer->set_sink(this_source->raw_file_ring_buffer_);
      ring_buffer_guard = temp_ring_buffer;
    }

    // Signal that reader is ready
    xEventGroupSetBits(this_source->event_group_, EventGroupBits::READER_READY);

    // Stream HTTP data into the ring buffer (sets error bits on timeout)
    stream_to_ring_buffer(container, transfer_buffer, this_source->event_group_);

    // Clean up HTTP connection (transfer_buffer and container destructors handle the rest)
    container->end();
  } while (false);

  // All RAII objects from the do-while block are now destroyed.

  // Signal we're done reading data (on error, COMMAND_STOP is already set so decode task won't act on this)
  xEventGroupSetBits(this_source->event_group_, EventGroupBits::READER_FINISHED);

  // Wait for decode task to acquire the ring buffer shared_ptr before we release ours.
  // On error paths, COMMAND_STOP is already set so this returns immediately.
  xEventGroupWaitBits(this_source->event_group_,
                      EventGroupBits::DECODER_RINGBUF_ACQUIRED | EventGroupBits::COMMAND_STOP, pdFALSE, pdFALSE,
                      portMAX_DELAY);

  // Safe to release now as decode task has acquired its own shared_ptr (or exited).
  // On error paths where ring_buffer_guard was never assigned, this is a no-op.
  ring_buffer_guard.reset();

  xEventGroupSetBits(this_source->event_group_, EventGroupBits::READER_STOPPED);
  vTaskSuspend(nullptr);  // Suspend this task indefinitely until the loop method deletes it
}

}  // namespace esphome::http_request

#endif  // USE_ESP32
