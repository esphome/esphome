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
static const uint32_t ENCODED_CHUNK_QUEUE_SIZE = 100;

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
    this->resonate_websocket_->start_server(websocket_server_handler, (void *) this, this->task_stack_in_psram_,
                                            WEBSOCKET_TASK_PRIORITY);
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
  PlayerHelloMessage msg = {.player_id = get_mac_address_pretty(),
                            .name = App.get_friendly_name(),
                            .support_codecs = {"pcm"},
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

  if (ws_pkt.len) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);

    size_t ws_packet_length = ws_pkt.len;
    if (is_text) {
      ++ws_packet_length;  // will append null chracter
    }

    if (this_resonate->websocket_payload_ != nullptr) {
      ESP_LOGE(TAG, "websocket payload wasn't deallocated, closing connection");
      return ESP_FAIL;
    }
    this_resonate->websocket_payload_ = allocator.allocate(ws_packet_length);

    if (this_resonate->websocket_payload_ == nullptr) {
      ESP_LOGE(TAG, "Failed to calloc memory for buffer");
      return ESP_ERR_NO_MEM;
    }

    ws_pkt.payload = this_resonate->websocket_payload_;

    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      allocator.deallocate(this_resonate->websocket_payload_, ws_packet_length);
      this_resonate->websocket_payload_ = nullptr;
      return ret;
    }

    if (is_text) {
      this_resonate->websocket_payload_[ws_pkt.len] = '\0';

      this_resonate->handle_json_message_((char *) this_resonate->websocket_payload_, timestamp);
      // Deallocate payload
      allocator.deallocate(this_resonate->websocket_payload_, ws_packet_length);
      this_resonate->websocket_payload_ = nullptr;
    } else if (is_binary) {
      // TODO: Check the binary type and handle cover art
#ifdef USE_RESONATE_AUDIO
      if (this_resonate->time_filter_->get_covariance() > MINIMUM_TIME_SYNC_ERROR_US * MINIMUM_TIME_SYNC_ERROR_US) {
        // Time sync isn't accurate, don't forward chunk to decoder
        allocator.deallocate(this_resonate->websocket_payload_, ws_packet_length);
        this_resonate->websocket_payload_ = nullptr;
      } else {
        // Use the big endian datatype helpers for converting to host format
        int64_be_t server_timestamp;
        uint32_be_t frame_count;

        std::memcpy((void *) &server_timestamp, (void *) (this_resonate->websocket_payload_ + 1),
                    sizeof(server_timestamp));
        std::memcpy((void *) &frame_count, (void *) (this_resonate->websocket_payload_ + 9), sizeof(frame_count));

        AudioChunk audio_chunk;
        audio_chunk.server_timestamp = server_timestamp;
        audio_chunk.frame_count = frame_count;

        // TODO: Remove this extra debug logging from final version
        static int64_t previous_timestamp = 0;
        if ((audio_chunk.server_timestamp - previous_timestamp) < 0) {
          printf("server corrected timestamps are not monotonic!\n");
        }
        previous_timestamp = audio_chunk.server_timestamp;

        audio_chunk.data = this_resonate->websocket_payload_;
        audio_chunk.offset = 13;
        audio_chunk.size = ws_packet_length - 13;

        audio_chunk.chunk_type = CHUNK_TYPE_ENCODED_AUDIO;
        if (!this_resonate->encoded_chunk_queue_->add_chunk(&audio_chunk, 0)) {
          // failed
          ESP_LOGE(TAG, "Failed to send audio chunk");
          allocator.deallocate(this_resonate->websocket_payload_, ws_packet_length);
        }
        this_resonate->websocket_payload_ = nullptr;  // The audio_chunk in the queue will manage the pointer now
      }
#else
      // Not built with audio, just deallocate it and ignore
      allocator.deallocate(this_resonate->websocket_payload_, ws_packet_length);
      this_resonate->websocket_payload_ = nullptr;
#endif
    } else {
      // Unknown type?
      // Deallocate payload
      allocator.deallocate(this_resonate->websocket_payload_, ws_packet_length);
      this_resonate->websocket_payload_ = nullptr;
    }
  }

  return err;
}

void ResonateHub::handle_json_message_(const std::string &message, int64_t timestamp) {
  ResonateServerToPlayerMessageType message_type = determine_message_type(message);

  switch (message_type) {
    case ResonateServerToPlayerMessageType::SESSION_START: {
      ESP_LOGD(TAG, "Session Started");
#ifdef USE_RESONATE_AUDIO
      audio::AudioStreamInfo session_audio_stream_info;
      ResonateCodecFormat codec_format;
      std::string codec_header;
      if (process_session_start_message(message, &session_audio_stream_info, &codec_format, &codec_header)) {
        AudioChunk header_chunk;
        header_chunk.offset = 0;
        header_chunk.server_timestamp = 0;

        if ((codec_format == ResonateCodecFormat::RESONATE_CODEC_PCM) ||
            (codec_format == ResonateCodecFormat::RESONATE_CODEC_OPUS)) {
          if (!allocate_audio_chunk(sizeof(DummyHeader), &header_chunk)) {
            ESP_LOGE(TAG, "Memory allocation failed");
            return;
          }
          DummyHeader *header = reinterpret_cast<DummyHeader *>(header_chunk.data);
          header->sample_rate = session_audio_stream_info.get_sample_rate();
          header->bits_per_sample = session_audio_stream_info.get_bits_per_sample();
          header->channels = session_audio_stream_info.get_channels();

          header_chunk.size = sizeof(DummyHeader);

          if (codec_format == ResonateCodecFormat::RESONATE_CODEC_PCM) {
            header_chunk.chunk_type = CHUNK_TYPE_PCM_DUMMY_HEADER;
          } else if (codec_format == ResonateCodecFormat::RESONATE_CODEC_OPUS) {
            header_chunk.chunk_type = CHUNK_TYPE_OPUS_DUMMY_HEADER;
          }
        } else if (codec_format == ResonateCodecFormat::RESONATE_CODEC_FLAC) {
          if (!allocate_audio_chunk(codec_header.size(), &header_chunk)) {
            ESP_LOGE(TAG, "Memory allocation failed");
            return;
          }

          header_chunk.size = codec_header.size();
          header_chunk.chunk_type = CHUNK_TYPE_FLAC_HEADER;
        }

        if (!this->encoded_chunk_queue_->add_chunk(&header_chunk, 0)) {
          // failed
          ESP_LOGE(TAG, "Failed to send codec header");
          deallocate_audio_chunk(&header_chunk);
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
        this->controls_callbacks_.call(ResonateControls::START);
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
    default:
      ESP_LOGW(TAG, "Unhandled server message: %s", message.c_str());
  }
}

#ifdef USE_RESONATE_AUDIO
bool ResonateHub::send_audio_chunk_(AudioChunk &audio_chunk, TickType_t ticks_to_wait,
                                    const audio::AudioStreamInfo &stream_info) {
  if (this->audio_chunk_callback_functions_.empty()) {
    // No callbacks registered, free the memory used for the data
    deallocate_audio_chunk(&audio_chunk);
    return true;
  }

  if (this->successful_receivers_ == -1) {
    this->successful_receivers_ = this->audio_chunk_callback_functions_.size() - 1;
  }

  for (int8_t i = this->successful_receivers_; i > 0; --i) {
    // Copy the data into a new audio chunk so one caller can't deallocate the data before another caller uses it
    AudioChunk copied_chunk = audio_chunk;
    allocate_audio_chunk(audio_chunk.offset + audio_chunk.size, &copied_chunk);
    std::memcpy((void *) copied_chunk.data, (void *) audio_chunk.data, audio_chunk.offset + audio_chunk.size);
    if (this->audio_chunk_callback_functions_[i](copied_chunk, ticks_to_wait, stream_info)) {
      --this->successful_receivers_;
    } else {
      // Deallocate the copy
      deallocate_audio_chunk(&copied_chunk);
      break;
    }
  }

  if (this->successful_receivers_ == 0) {
    if (this->audio_chunk_callback_functions_[0](audio_chunk, ticks_to_wait, stream_info)) {
      this->successful_receivers_ = -1;
      return true;
    }
  }

  return false;
}

void ResonateHub::decode_task(void *params) {
  ResonateHub *this_resonate = (ResonateHub *) params;

  AudioChunk encoded_chunk;
  AudioChunk decoded_chunk;
  decoded_chunk.data = nullptr;

  std::unique_ptr<ResonateDecoder> decoder = std::make_unique<ResonateDecoder>();
  audio::AudioStreamInfo current_stream_info;

  while (true) {
    EventBits_t event_bits = xEventGroupGetBits(this_resonate->event_group_);

    if (event_bits & COMMAND_STOP) {
      // Processes the stop command by stopping the speaker and resetting all states

      decoder->reset_decoders();
      this_resonate->encoded_chunk_queue_->reset();
      if (decoded_chunk.data != nullptr) {
        // Haven't sent a decoded chunk, so manually deallocate it
        deallocate_audio_chunk(&decoded_chunk);
        decoded_chunk.data = nullptr;
      }

      // Reset the send_audio_chunk counter to make sure all receivers get the next chunk after starting again
      this_resonate->successful_receivers_ = -1;

      vTaskDelay(pdMS_TO_TICKS(50));

      xEventGroupClearBits(this_resonate->event_group_, COMMAND_STOP);
    }

    if (decoded_chunk.data != nullptr) {
      // Add decoded chunk to the queue
      uint32_t new_frames = decoded_chunk.frame_count;

      if (this_resonate->send_audio_chunk_(
              decoded_chunk, pdMS_TO_TICKS(current_stream_info.frames_to_milliseconds_with_remainder(&new_frames)),
              current_stream_info)) {
        decoded_chunk.data = nullptr;
      } else {
        // Try adding again
        continue;
      }
    }

    if (this_resonate->encoded_chunk_queue_->peek_chunk(&encoded_chunk, pdMS_TO_TICKS(50))) {
      if ((encoded_chunk.chunk_type != CHUNK_TYPE_ENCODED_AUDIO) &&
          (encoded_chunk.chunk_type != CHUNK_TYPE_DECODED_AUDIO)) {
        if (!decoder->process_header(&encoded_chunk, &current_stream_info)) {
          ESP_LOGE(TAG, "Failed to process audio codec header");
          continue;
        }
        xEventGroupClearBits(this_resonate->event_group_, COMMAND_STOP);  // Where the hell is this getting set?
      } else if ((decoder->get_current_codec() != ResonateCodecFormat::RESONATE_CODEC_UNSUPPORTED) &&
                 (encoded_chunk.chunk_type == CHUNK_TYPE_ENCODED_AUDIO)) {
        if (!decoder->decode_audio_chunk(&encoded_chunk, &decoded_chunk)) {
          ESP_LOGE(TAG, "Failed to decode audio chunk");
          continue;
        }
        decoded_chunk.server_timestamp =
            this_resonate->convert_server_to_client_timestamp(encoded_chunk.server_timestamp);
      }

      this_resonate->encoded_chunk_queue_->receive_chunk(false);
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
