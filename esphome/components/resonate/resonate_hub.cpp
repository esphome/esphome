#include "resonate_hub.h"

#if defined(USE_ESP_IDF)
#ifdef USE_RESONATE_AUDIO
#include "resonate_decoder.h"
#include "esphome/components/audio/audio.h"
#endif

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

#include "esphome/core/application.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/log.h"

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.hub";

#ifdef USE_RESONATE_AUDIO
static const uint32_t ENCODED_CHUNK_QUEUE_SIZE = 200;

static const size_t DECODE_TASK_STACK_SIZE = 6 * 1024;
static const UBaseType_t DECODE_TASK_PRIORITY = 2;
static const int64_t MINIMUM_TIME_SYNC_ERROR_US = 20000;
#endif

static const UBaseType_t WEBSOCKET_TASK_PRIORITY = 17;

struct TimeResponse {
  int64_t offset;
  int64_t delay;
};

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  CONTROL_START = (1 << 7),
  WARNING_ENCODED_CHUNK_FULL = (1 << 11),
};

void ResonateHub::setup() {
  this->resonate_websocket_ = make_unique<ResonateWebsocket>();
  if (this->resonate_websocket_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create resonate object.");
    this->mark_failed();
  }

  this->time_filter_ = make_unique<ResonateTimeFilter>(this->kalman_process_error_, this->kalman_forget_factor_);
  if (this->time_filter_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create resonate time filter.");
    this->mark_failed();
  }

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create event group.");
    this->mark_failed();
  }

#ifdef USE_RESONATE_AUDIO
  this->encoded_chunk_queue_ = ResonateChunkQueue::create(ENCODED_CHUNK_QUEUE_SIZE);
  if (this->encoded_chunk_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create encoded chunk data queue.");
    this->mark_failed();
  }
#endif
}

void ResonateHub::loop() {
  if (this->last_sent_time_message_ < std::numeric_limits<int64_t>::max() &&
      !this->resonate_websocket_->is_connected()) {
    // Websocket client disconnected

    this->last_sent_time_message_ = std::numeric_limits<int64_t>::max();  // block trying to send time messages
    this->time_filter_->reset();
  }

  int64_t delay_between_time_messages_ms = 200;
  const int64_t current_covariance = this->time_filter_->get_covariance();
  if (current_covariance < 5000LL * 5000LL) {
    delay_between_time_messages_ms = 500;
  }
  if (current_covariance < 2000LL * 2000LL) {
    delay_between_time_messages_ms = 1000;
  }
  if (current_covariance < 1000LL * 1000LL) {
    delay_between_time_messages_ms = 3000;
  }
  if (((esp_timer_get_time() - this->last_sent_time_message_) / 1000LL > delay_between_time_messages_ms) &&
      this->resonate_websocket_->is_connected()) {
    bool should_send_new = !this->pending_time_message_ || delay_between_time_messages_ms >= 1000;
    if (should_send_new) {
      this->resonate_websocket_->send_time_message();
      this->last_sent_time_message_ = esp_timer_get_time();

      this->pending_time_message_ = true;

#ifdef USE_RESONATE_SENSOR
      this->update_resonate_sensor(
          {.type = ResonateSensorTypes::KALMAN_ERROR, .value = static_cast<float>(this->time_filter_->get_error())});
#endif
    }
  }

  if (network::is_connected() && !this->resonate_websocket_->is_started()) {
    this->resonate_websocket_->start_server(websocket_server_handler, websocket_close_callback, (void *) this,
                                            this->task_stack_in_psram_, WEBSOCKET_TASK_PRIORITY);
#ifdef USE_RESONATE_AUDIO
    if (this->decode_task_stack_buffer_ == nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
      }
      this->decode_task_handle_ =
          xTaskCreateStatic(decode_task, "resonate_decode", DECODE_TASK_STACK_SIZE, (void *) this, DECODE_TASK_PRIORITY,
                            this->decode_task_stack_buffer_, &this->decode_task_stack_);
    }
#endif
  }
}

void ResonateHub::start() {
  // TODO: Don't hardcode supported settings, it should be configured in yaml
  ClientHelloMessage msg = {.client_id = get_mac_address_pretty(),
                            .name = App.get_friendly_name(),
                            .support_codecs = {"flac", "opus", "pcm"},
                            .support_channels = {2, 1},
                            .support_sample_rates = {48000},
                            .support_bit_depth = {16},
                            .buffer_capacity = 1000000,
                            .support_streams = {"media"},
                            .support_pictures_formats = {},
                            .media_display_size = "null"};
  this->resonate_websocket_->send_hello_message(&msg);
  this->last_sent_time_message_ = esp_timer_get_time();
}

#ifdef USE_MEDIA_PLAYER
void ResonateHub::send_stream_command(const media_player::MediaPlayerCall &call) {
  if (this->resonate_websocket_->is_connected()) {
    this->resonate_websocket_->send_stream_command_message(call);
  }
}
#endif

void ResonateHub::websocket_close_callback(void *context) {
  ResonateHub *this_resonate = (ResonateHub *) context;
  xEventGroupSetBits(this_resonate->event_group_, COMMAND_STOP);  // Handles stopping in the hub component
  this_resonate->controls_callbacks_.call(ResonateControls::STOP);

  ESP_LOGD(TAG, "Connection closed");
}

esp_err_t ResonateHub::websocket_server_handler(httpd_req_t *req) {
  int64_t timestamp = esp_timer_get_time();
  ResonateHub *this_resonate = (ResonateHub *) req->user_ctx;

  esp_err_t err = ESP_OK;

  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, a new connection was opened");
    delay(250);
    this_resonate->start();

    return err;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }

  // For some reason checking this later on after receiving it a second time resulted in every packet being classified
  // as text... weird
  bool is_text = (ws_pkt.type == HTTPD_WS_TYPE_TEXT);
  bool is_binary = (ws_pkt.type == HTTPD_WS_TYPE_BINARY);

  bool is_fin = ws_pkt.final;

  if (!is_fin) {
    ESP_LOGD(TAG, "FIN flag not set on packet, this may not be properly handled");
  }

  if (ws_pkt.len) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);

    size_t new_length = ws_pkt.len;

    if (this_resonate->websocket_offset_ == 0) {
      if (this_resonate->websocket_payload_ != nullptr) {
        ESP_LOGE(TAG, "websocket payload wasn't deallocated, closing connection");
        this_resonate->deallocate_websocket_payload_();
        return ESP_FAIL;
      }
      this_resonate->websocket_payload_ = allocator.allocate(this_resonate->websocket_len_ + new_length);
    } else {
      uint8_t *new_payload =
          allocator.reallocate(this_resonate->websocket_payload_, this_resonate->websocket_len_ + new_length);
      if (new_payload == nullptr) {
        this_resonate->deallocate_websocket_payload_();
      }
      this_resonate->websocket_payload_ = new_payload;
    }

    if (this_resonate->websocket_payload_ == nullptr) {
      ESP_LOGE(TAG, "Failed to calloc memory for buffer");
      return ESP_ERR_NO_MEM;
    }

    this_resonate->websocket_len_ += new_length;  // Successfully allocated, update length

    ws_pkt.payload = this_resonate->websocket_payload_ + this_resonate->websocket_offset_;

    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      this_resonate->deallocate_websocket_payload_();
      return ret;
    }
    this_resonate->websocket_offset_ += ws_pkt.len;

    if (is_fin) {
      if (is_text) {
        // Create string from payload for JSON processing
        const std::string message(this_resonate->websocket_payload_,
                                  this_resonate->websocket_payload_ + this_resonate->websocket_len_);
        this_resonate->process_json_message_(message, timestamp);
        // Always deallocate after JSON processing
        this_resonate->deallocate_websocket_payload_();
      } else if (is_binary) {
        if (this_resonate->process_binary_message_(this_resonate->websocket_payload_, this_resonate->websocket_len_)) {
          // Ownership was transferred, just clear the pointer
          this_resonate->websocket_payload_ = nullptr;
        } else {
          // Ownership not transferred, we must deallocate
          this_resonate->deallocate_websocket_payload_();
        }
      } else {
        // Unknown type - deallocate payload
        this_resonate->deallocate_websocket_payload_();
      }
      this_resonate->websocket_offset_ = 0;
      this_resonate->websocket_len_ = 0;
    }
  }

  return err;
}

bool ResonateHub::process_binary_message_(uint8_t *payload, size_t len) {
  ResonateBinaryType binary_type;
  std::memcpy((void *) &binary_type, (void *) payload, 1);

  switch (binary_type) {
    case RESONATE_AUDIO_BINARY: {
#ifdef USE_RESONATE_AUDIO
      if ((this->time_filter_->get_covariance() > MINIMUM_TIME_SYNC_ERROR_US * MINIMUM_TIME_SYNC_ERROR_US) ||
          (len < 13)) {
        // Time sync isn't accurate, don't forward chunk to decoder
        // Or the total packet length is too short to match the resonate binary audio chunk header size
        return false;  // deallocate payload
      } else {
        // Use the big endian datatype helpers for converting to host format
        int64_be_t server_timestamp;
        uint32_be_t frame_count;

        std::memcpy((void *) &server_timestamp, (void *) (payload + 1), sizeof(server_timestamp));
        std::memcpy((void *) &frame_count, (void *) (payload + 9), sizeof(frame_count));

        // Create a heap-allocated chunk that takes ownership of the payload
        AudioChunk *audio_chunk = create_audio_chunk_from_buffer(payload, len);
        if (audio_chunk == nullptr) {
          ESP_LOGE(TAG, "Failed to allocate AudioChunk");
          return false;  // deallocate payload
        }
        audio_chunk->offset = 13;
        audio_chunk->size = len - 13;
        audio_chunk->server_timestamp = server_timestamp;
        audio_chunk->frame_count = frame_count;
        audio_chunk->chunk_type = CHUNK_TYPE_ENCODED_AUDIO;

        // TODO: Remove this extra debug logging from final version
        static int64_t previous_timestamp = 0;
        if ((audio_chunk->server_timestamp - previous_timestamp) < 0) {
          printf("server corrected timestamps are not monotonic!\n");
        }
        previous_timestamp = audio_chunk->server_timestamp;

        if (!this->encoded_chunk_queue_->add_chunk(audio_chunk, 0)) {
          // Failed to add
          ESP_LOGE(TAG, "Failed to send audio chunk, clearing encoded chunk queue");
          this->encoded_chunk_queue_->reset();
        }
        // Release our reference, queue has its own if successful. If unsuccessful, then this will deallocate the
        // payload
        audio_chunk->release();
        return true;  // don't deallocate payload, just clear pointer
      }
#else
      // Not built with audio, so ownership not transferred
      return false;  // deallocate payload
#endif
      break;
    }
    case RESONATE_IMAGE_BINARY: {
#ifdef USE_RESONATE_IMAGE
      ResonateImageFormat image_format;
      std::memcpy((void *) &image_format, (void *) payload + 1, 1);

      runtime_image::ImageFormat runtime_image_format;
      switch (image_format) {
        case RESONATE_IMAGE_BMP:
          runtime_image_format = runtime_image::BMP;
          break;
        case RESONATE_IMAGE_JPG:
          runtime_image_format = runtime_image::JPEG;
          break;
        case RESONATE_IMAGE_PNG:
          runtime_image_format = runtime_image::PNG;
          break;
      }

      for (auto image = this->images_.begin(); image != this->images_.end(); ++image) {
        image->begin_decode(runtime_image_format, len - 2);
        image->feed_data(payload + 1, len - 2);
        if (!image->end_decode()) {
          ESP_LOGE(TAG, "Failed to decoded image");
        }
      }
#endif
      return false;  // deallocate payload
      break;
    }
    default: {
      ESP_LOGW(TAG, "Unknown binary type %d", binary_type);
      break;
    }
  }

  return false;  // default to deallocate payload
}

bool ResonateHub::process_json_message_(const std::string &message, int64_t timestamp) {
  ResonateServerToPlayerMessageType message_type = determine_message_type(message);

  switch (message_type) {
    case ResonateServerToPlayerMessageType::SESSION_START: {
      ESP_LOGD(TAG, "Session Started");
#ifdef USE_RESONATE_AUDIO
      audio::AudioStreamInfo session_audio_stream_info;
      ResonateCodecFormat codec_format;
      std::string codec_header;
      if (process_session_start_message(message, &session_audio_stream_info, &codec_format, &codec_header)) {
        AudioChunk *header_chunk = nullptr;

        if ((codec_format == ResonateCodecFormat::PCM) || (codec_format == ResonateCodecFormat::OPUS)) {
          header_chunk = create_audio_chunk(sizeof(DummyHeader));
          if (header_chunk == nullptr) {
            ESP_LOGE(TAG, "Memory allocation failed");
            return false;
          }
          DummyHeader *header = reinterpret_cast<DummyHeader *>(header_chunk->get_data());
          header->sample_rate = session_audio_stream_info.get_sample_rate();
          header->bits_per_sample = session_audio_stream_info.get_bits_per_sample();
          header->channels = session_audio_stream_info.get_channels();

          header_chunk->offset = 0;
          header_chunk->size = sizeof(DummyHeader);
          header_chunk->server_timestamp = 0;
          header_chunk->frame_count = 0;

          if (codec_format == ResonateCodecFormat::PCM) {
            header_chunk->chunk_type = CHUNK_TYPE_PCM_DUMMY_HEADER;
          } else if (codec_format == ResonateCodecFormat::OPUS) {
            header_chunk->chunk_type = CHUNK_TYPE_OPUS_DUMMY_HEADER;
          }
        } else if (codec_format == ResonateCodecFormat::FLAC) {
          std::vector<uint8_t> flac_header = base64_decode(codec_header);
          header_chunk = create_audio_chunk(flac_header.size());
          if (header_chunk == nullptr) {
            ESP_LOGE(TAG, "Memory allocation failed");
            return false;
          }
          std::memcpy((void *) header_chunk->get_data(), (void *) flac_header.data(), flac_header.size());
          header_chunk->offset = 0;
          header_chunk->size = flac_header.size();
          header_chunk->chunk_type = CHUNK_TYPE_FLAC_HEADER;
          header_chunk->server_timestamp = 0;
          header_chunk->frame_count = 0;
        }

        if (!this->encoded_chunk_queue_->add_chunk(header_chunk, 0)) {
          // failed
          ESP_LOGE(TAG, "Failed to send codec header");
        } else {
          this->controls_callbacks_.call(ResonateControls::START);
        }
        // Always release our reference (queue has its own if successful)
        if (header_chunk != nullptr) {
          header_chunk->release();
        }
      }
#endif
      break;
    }
    case ResonateServerToPlayerMessageType::SESSION_END: {
      ESP_LOGD(TAG, "Session ended");
      xEventGroupSetBits(this->event_group_, COMMAND_STOP);  // Handles stopping in the hub component
      this->controls_callbacks_.call(ResonateControls::STOP);
      break;
    }
    case ResonateServerToPlayerMessageType::SERVER_HELLO: {
      std::string server_id;
      std::string server_name;
      if (process_server_hello_message(message, &server_id, &server_name)) {
        // TODO: why did I use move instead of passing a pointer in directly...?
        this->server_id_ = std::move(server_id);
        this->server_name_ = std::move(server_name);
        xEventGroupSetBits(this->event_group_, CONTROL_START);
        ESP_LOGD(TAG, "Connected to server %s with id %s", this->server_name_.c_str(), this->server_id_.c_str());
      }
      break;
    }
    case ResonateServerToPlayerMessageType::SERVER_TIME: {
      TimeTransmittedReplacement time_replacement = this->resonate_websocket_->get_last_time_message();
      int64_t offset;
      int64_t max_error;
      if (process_server_time_message(message, timestamp, time_replacement, &offset, &max_error)) {
        this->time_filter_->update(offset, max_error, timestamp);
      }
      this->pending_time_message_ = false;
      break;
    }
    case ResonateServerToPlayerMessageType::METADATA_UPDATE: {
#ifdef USE_RESONATE_METADATA
      if (process_metadata_update_message(message, &this->metadata_)) {
        this->metadata_callbacks_.call(this->metadata_);
      }
#endif
      break;
    }
    case ResonateServerToPlayerMessageType::VOLUME_SET: {
#ifdef USE_RESONATE_AUDIO
      uint8_t volume;
      if (process_volume_set_message(message, &volume)) {
        this->update_volume(volume);
        this->controls_callbacks_.call(ResonateControls::VOLUME_UPDATE);
      }
#endif
      break;
    }
    case ResonateServerToPlayerMessageType::MUTE_SET: {
#ifdef USE_RESONATE_AUDIO
      bool is_muted;
      if (process_mute_set_message(message, &is_muted)) {
        this->update_muted(is_muted);
        this->controls_callbacks_.call(ResonateControls::MUTE_UPDATE);
      }
#endif
      break;
    }
    default:
      ESP_LOGW(TAG, "Unhandled server message: %s", message.c_str());
  }

  return true;  // Successfully processed message
}

void ResonateHub::deallocate_websocket_payload_() {
  if (this->websocket_payload_ != nullptr) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
    allocator.deallocate(this->websocket_payload_, this->websocket_len_);
    this->websocket_payload_ = nullptr;
  }
}

#ifdef USE_RESONATE_AUDIO
void ResonateHub::update_muted(bool is_muted) {
  this->muted_ = is_muted;
  this->publish_client_state();
}
void ResonateHub::update_state(std::string state) {
  // TODO: Use an enum, not a string for the state
  this->state_ = std::move(state);
  this->publish_client_state();
}
void ResonateHub::update_volume(uint8_t volume) {
  this->volume_ = volume;
  this->publish_client_state();
}

void ResonateHub::publish_client_state() {
  const PlayerStateMessage state = {.state = this->state_, .volume = this->volume_, .muted = this->muted_};
  this->resonate_websocket_->send_player_state_message(&state);
}

bool ResonateHub::send_audio_chunk_(AudioChunk *audio_chunk, TickType_t ticks_to_wait,
                                    const audio::AudioStreamInfo &stream_info) {
  if (audio_chunk == nullptr) {
    ESP_LOGE(TAG, "Null audio chunk passed to send_audio_chunk_");
    return false;
  }

  if (this->audio_chunk_callbacks_.empty()) {
    // No callbacks registered, return true so caller releases the chunk
    return true;
  }

  // Simple distribution to all consumers
  // Each consumer gets the pointer and must call add_ref() if they keep it
  for (auto &callback : this->audio_chunk_callbacks_) {
    if (!callback(audio_chunk, ticks_to_wait, stream_info)) {
      // TODO : properly handle if one consumer fails to receive and another succeeds
      return false;
    }
  }

  return true;
}

void ResonateHub::decode_task(void *params) {
  ResonateHub *this_resonate = (ResonateHub *) params;

  AudioChunk *encoded_chunk = nullptr;
  AudioChunk *decoded_chunk = nullptr;

  std::unique_ptr<ResonateDecoder> decoder = std::make_unique<ResonateDecoder>();
  audio::AudioStreamInfo current_stream_info;

  while (true) {
    EventBits_t event_bits = xEventGroupGetBits(this_resonate->event_group_);

    if (event_bits & COMMAND_STOP) {
      // Processes the stop command by stopping the speaker and resetting all states

      decoder->reset_decoders();
      this_resonate->encoded_chunk_queue_->reset();
      if (decoded_chunk != nullptr) {
        // Haven't sent a decoded chunk, so manually deallocate it
        decoded_chunk->release();
        decoded_chunk = nullptr;
      }

      vTaskDelay(pdMS_TO_TICKS(50));

      xEventGroupClearBits(this_resonate->event_group_, COMMAND_STOP);
    }

    if (decoded_chunk != nullptr) {
      // Add decoded chunk to the queue
      uint32_t new_frames = decoded_chunk->frame_count;

      if (this_resonate->send_audio_chunk_(
              decoded_chunk, pdMS_TO_TICKS(current_stream_info.frames_to_milliseconds_with_remainder(&new_frames)),
              current_stream_info)) {
        decoded_chunk->release();
        decoded_chunk = nullptr;  // Chunk released, clear pointer
      } else {
        // Try adding again
        continue;
      }
    }

    if (this_resonate->encoded_chunk_queue_->peek_chunk(&encoded_chunk, pdMS_TO_TICKS(50))) {
      if ((encoded_chunk->chunk_type != CHUNK_TYPE_ENCODED_AUDIO) &&
          (encoded_chunk->chunk_type != CHUNK_TYPE_DECODED_AUDIO)) {
        decoder->reset_decoders();
        if (!decoder->process_header(encoded_chunk, &current_stream_info)) {
          ESP_LOGE(TAG, "Failed to process audio codec header");
          continue;
        }
        xEventGroupClearBits(this_resonate->event_group_, COMMAND_STOP);  // Where the hell is this getting set?
      } else if ((decoder->get_current_codec() != ResonateCodecFormat::UNSUPPORTED) &&
                 (encoded_chunk->chunk_type == CHUNK_TYPE_ENCODED_AUDIO)) {
        if (!decoder->decode_audio_chunk(encoded_chunk, &decoded_chunk)) {
          ESP_LOGE(TAG, "Failed to decode audio chunk");
          continue;
        }
        decoded_chunk->server_timestamp =
            this_resonate->convert_server_to_client_timestamp(encoded_chunk->server_timestamp);
      }

      // Remove chunk from queue and release the queue's reference
      // Note: For PCM, decoded_chunk may be the same as encoded_chunk (with added ref)
      // For other codecs, they are separate chunks
      // In both cases, we release the encoded_chunk received from the queue
      if (this_resonate->encoded_chunk_queue_->receive_chunk(&encoded_chunk, false, 0)) {
        encoded_chunk->release();
      }
    }

    static uint32_t high_water_mark = 8192;
    uint32_t new_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
    if (new_high_water_mark < high_water_mark) {
      ESP_LOGD(TAG, "Decode task - High water mark changed from %d to %d.", high_water_mark, new_high_water_mark);
      high_water_mark = new_high_water_mark;
    }
  }
}
#endif

}  // namespace resonate
}  // namespace esphome

#endif
