#include "resonate_hub.h"

#if defined(USE_ESP_IDF)
#ifdef USE_RESONATE_AUDIO
#include "resonate_decoder.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_chunk.h"
#endif

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

#include "esphome/core/application.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/log.h"

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.hub";

static const size_t RESONATE_BINARY_CHUNK_HEADER_SIZE = 9;

#ifdef USE_RESONATE_AUDIO
static const uint32_t ENCODED_CHUNK_QUEUE_SIZE = 200;

static const size_t DECODE_TASK_STACK_SIZE = 6 * 1024;
static const UBaseType_t DECODE_TASK_PRIORITY = 2;

// Time synchronization accuracy thresholds:
// When Kalman filter variance exceeds this threshold (squared), time sync is considered unreliable
static const int64_t TIME_SYNC_ERROR_THRESHOLD_US = 20000;
// Minimum delay before retrying chunk decode when time sync is unreliable
static const uint32_t MIN_RETRY_DELAY_UNRELIABLE_SYNC_MS = 15;
#endif

// Send time messages more frequently when the Kalman error is high
static const int64_t KALMAN_ERROR_THRESHOLD_LOW_US = 1000;
static const int64_t KALMAN_ERROR_THRESHOLD_MEDIUM_US = 2000;
static const int64_t KALMAN_ERROR_THRESHOLD_HIGH_US = 5000;

static const int64_t TIME_MESSAGE_DELAY_THRESHOLD_LOW_MS = 3000;
static const int64_t TIME_MESSAGE_DELAY_THRESHOLD_MEDIUM_MS = 1000;
static const int64_t TIME_MESSAGE_DELAY_THRESHOLD_HIGH_MS = 500;
static const int64_t TIME_MESSAGE_DELAY_DEFAULT_MS = 200;

static const UBaseType_t WEBSOCKET_TASK_PRIORITY = 17;

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  COMMAND_START = (1 << 1),
  TASK_STARTING = (1 << 8),
  TASK_RUNNING = (1 << 9),
  TASK_STOPPING = (1 << 10),
  TASK_STOPPED = (1 << 11),
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
  this->encoded_chunk_queue_ = audio::AudioChunkQueue::create(ENCODED_CHUNK_QUEUE_SIZE, 2000000, true);
  if (this->encoded_chunk_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create encoded chunk data queue.");
    this->mark_failed();
  }
#endif
}

void ResonateHub::send_time_message_() {
  if (!this->resonate_websocket_->is_connected() || this->pending_time_message_ || !this->hello_message_sent_) {
    return;
  }

  const int64_t current_covariance = this->time_filter_->get_covariance();  // use covariance to avoid unnecessary sqrt
  int64_t delay_ms = TIME_MESSAGE_DELAY_DEFAULT_MS;

  if (current_covariance < KALMAN_ERROR_THRESHOLD_LOW_US * KALMAN_ERROR_THRESHOLD_LOW_US) {
    delay_ms = TIME_MESSAGE_DELAY_THRESHOLD_LOW_MS;
  } else if (current_covariance < KALMAN_ERROR_THRESHOLD_MEDIUM_US * KALMAN_ERROR_THRESHOLD_MEDIUM_US) {
    delay_ms = TIME_MESSAGE_DELAY_THRESHOLD_MEDIUM_MS;
  } else if (current_covariance < KALMAN_ERROR_THRESHOLD_HIGH_US * KALMAN_ERROR_THRESHOLD_HIGH_US) {
    delay_ms = TIME_MESSAGE_DELAY_THRESHOLD_HIGH_MS;
  }

  const int64_t time_since_last_ms = (esp_timer_get_time() - this->last_sent_time_message_) / 1000LL;
  if (time_since_last_ms <= delay_ms) {
    return;
  }

  this->resonate_websocket_->send_time_message();
  this->last_sent_time_message_ = esp_timer_get_time();
  this->pending_time_message_ = true;

#ifdef USE_RESONATE_SENSOR
  this->update_resonate_sensor(
      {.type = ResonateSensorTypes::KALMAN_ERROR, .value = static_cast<float>(this->time_filter_->get_error())});
#endif
}

void ResonateHub::loop() {
  this->send_time_message_();

  if (network::is_connected() && !this->resonate_websocket_->is_started()) {
    this->resonate_websocket_->start_server(websocket_server_handler, websocket_close_callback, (void *) this,
                                            this->task_stack_in_psram_, WEBSOCKET_TASK_PRIORITY);
  }

#ifdef USE_RESONATE_AUDIO
  EventBits_t event_bits = xEventGroupGetBits(this->event_group_);

  if (event_bits & EventGroupBits::COMMAND_START) {
    if (this->decode_task_handle_ == nullptr) {
      ESP_LOGD(TAG, "Trying to start decode task");
      if (this->decode_task_stack_buffer_ == nullptr) {
        // Allocate stack for decode task
        if (this->task_stack_in_psram_) {
          RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
          this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
        } else {
          RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
          this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
        }
      }
      if (this->decode_task_stack_buffer_ != nullptr) {
        this->decode_task_handle_ =
            xTaskCreateStatic(decode_task, "resonate_decode", DECODE_TASK_STACK_SIZE, (void *) this,
                              DECODE_TASK_PRIORITY, this->decode_task_stack_buffer_, &this->decode_task_stack_);
        if (this->decode_task_handle_ == nullptr) {
          ESP_LOGE(TAG, "Failed to create decode task.");
        } else {
          xEventGroupClearBits(this->event_group_, EventGroupBits::COMMAND_START);
        }
      } else {
        ESP_LOGW(TAG, "Couldn't allocate memory for decode task stack");
      }
    } else {
      // Already running
      xEventGroupClearBits(this->event_group_, EventGroupBits::COMMAND_START);
    }
  }

  if (event_bits & EventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Decode task starting");
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STARTING);
  }
  if (event_bits & EventGroupBits::TASK_RUNNING) {
    // Task is running
    ESP_LOGD(TAG, "Decode task running");
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_RUNNING);
  }
  if (event_bits & EventGroupBits::TASK_STOPPING) {
    ESP_LOGD(TAG, "Decode task stopping");
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STOPPING);
  }
  if (event_bits & EventGroupBits::TASK_STOPPED) {
    ESP_LOGD(TAG, "Decode task stopped");
    if (this->decode_task_handle_ != nullptr) {
      vTaskDelete(this->decode_task_handle_);
      this->decode_task_handle_ = nullptr;
    }
    if (this->decode_task_stack_buffer_ != nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        stack_allocator.deallocate(this->decode_task_stack_buffer_, DECODE_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        stack_allocator.deallocate(this->decode_task_stack_buffer_, DECODE_TASK_STACK_SIZE);
      }
      this->decode_task_stack_buffer_ = nullptr;
    }

    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STOPPED | EventGroupBits::COMMAND_STOP);
  }
#endif
}

void ResonateHub::start() {
  ClientHelloMessage msg;
  msg.client_id = get_mac_address_pretty();
  msg.name = App.get_friendly_name();
  msg.version = 1;

  std::vector<std::string> supported_roles;
  // TODO: Don't hardcode controller role
  supported_roles.push_back("controller");

#ifdef USE_RESONATE_AUDIO
  supported_roles.push_back("player");
  PlayerSupportObject player_support = {
      .support_codecs = {"flac", "opus", "pcm"},
      .support_bit_depth = {16},
      .support_channels = {2, 1},
      .support_sample_rates = {48000},
      .buffer_capacity = this->buffer_size_,
  };
  msg.player_support = player_support;
#endif

#ifdef USE_RESONATE_METADATA
  supported_roles.push_back("metadata");

  std::vector<std::string> support_picture_formats;
#ifdef USE_RESONATE_IMAGE
  for (auto it = this->preferred_image_formats_.begin(); it != this->preferred_image_formats_.end(); ++it) {
    support_picture_formats.push_back(it->first);
  }
#endif

  // TODO: Don't hardcode media dimensions
  MetadataSupportObject metadata_support = {
      .support_picture_formats = support_picture_formats,
      .media_width = 320,
      .media_height = 240,
  };
  msg.metadata_support = metadata_support;
#endif

  msg.supported_roles = supported_roles;

  this->resonate_websocket_->send_hello_message(&msg);
  this->last_sent_time_message_ = esp_timer_get_time();
#ifdef USE_RESONATE_AUDIO
  this->publish_client_state();
#endif
  this->hello_message_sent_ = true;
}

#ifdef USE_MEDIA_PLAYER
void ResonateHub::send_group_command(const media_player::MediaPlayerCommand &command) {
  if (this->resonate_websocket_->is_connected()) {
    this->resonate_websocket_->send_group_command_message(command);
  }
}
#endif

void ResonateHub::websocket_close_callback(void *context) {
  ResonateHub *this_resonate = (ResonateHub *) context;
  xEventGroupSetBits(this_resonate->event_group_, COMMAND_STOP);  // Handles stopping in the hub component
  this_resonate->controls_callbacks_.call(ResonateControls::STOP);

  this_resonate->time_filter_->reset();
  this_resonate->hello_message_sent_ = false;
  this_resonate->pending_time_message_ = false;
  this_resonate->deallocate_websocket_payload_();

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

    if (this_resonate->websocket_write_offset_ == 0) {
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

    ws_pkt.payload = this_resonate->websocket_payload_ + this_resonate->websocket_write_offset_;

    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      this_resonate->deallocate_websocket_payload_();
      return ret;
    }
    this_resonate->websocket_write_offset_ += ws_pkt.len;

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
          // Ownership was transferred, just clear the pointer and reset lengths
          this_resonate->websocket_payload_ = nullptr;
          this_resonate->websocket_write_offset_ = 0;
          this_resonate->websocket_len_ = 0;
        } else {
          // Ownership not transferred, we must deallocate
          this_resonate->deallocate_websocket_payload_();
        }
      } else {
        // Unknown type - deallocate payload
        this_resonate->deallocate_websocket_payload_();
      }
    }
  }

  return err;
}

bool ResonateHub::process_binary_message_(uint8_t *payload, size_t len) {
  ResonateBinaryType binary_type;

  if (len < RESONATE_BINARY_CHUNK_HEADER_SIZE) {
    // Packet too short for resonate binary message header
    return false;  // deallocate payload
  }

  std::memcpy((void *) &binary_type, (void *) payload, 1);

  // Use the big endian datatype helpers for converting to host format
  int64_be_t server_timestamp;
  std::memcpy((void *) &server_timestamp, (void *) (payload + 1), sizeof(server_timestamp));

  switch (binary_type) {
    case RESONATE_AUDIO_BINARY: {
#ifdef USE_RESONATE_AUDIO
      if (len < RESONATE_BINARY_CHUNK_HEADER_SIZE) {
        // Packet too short for resonate binary audio chunk header
        return false;  // deallocate payload
      } else {
        // Use the big endian datatype helpers for converting to host format
        int64_be_t server_timestamp;

        std::memcpy((void *) &server_timestamp, (void *) (payload + 1), sizeof(server_timestamp));

        // Create a shared_ptr chunk that takes ownership of the payload
        auto audio_chunk = create_resonate_chunk_from_buffer(payload, len);
        if (audio_chunk == nullptr) {
          ESP_LOGE(TAG, "Failed to allocate ResonateAudioChunk");
          return false;  // deallocate payload
        }
        audio_chunk->offset = RESONATE_BINARY_CHUNK_HEADER_SIZE;
        audio_chunk->size = len - RESONATE_BINARY_CHUNK_HEADER_SIZE;
        audio_chunk->timestamp = server_timestamp;
        audio_chunk->chunk_type = CHUNK_TYPE_ENCODED_AUDIO;

        if (!this->encoded_chunk_queue_->add_chunk(audio_chunk, 0)) {
          // Failed to add
          ESP_LOGE(TAG, "Failed to send audio chunk, clearing encoded chunk queue");
          this->encoded_chunk_queue_->reset();
        }
        // No need to manually release - shared_ptr handles cleanup automatically
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
      // TODO don't hardcode this
      ResonateImageFormat image_format = RESONATE_IMAGE_JPG;

      this->image_callbacks_.call(payload + RESONATE_BINARY_CHUNK_HEADER_SIZE, len - RESONATE_BINARY_CHUNK_HEADER_SIZE,
                                  image_format);
#else
      ESP_LOGD(TAG, "Ignoring an album art message with %d bytes", len - 2);
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
  ResonateServerToClientMessageType message_type = determine_message_type(message);

  switch (message_type) {
    case ResonateServerToClientMessageType::STREAM_START: {
      xEventGroupSetBits(this->event_group_, COMMAND_START);
    }  // intentional fallthrough
    case ResonateServerToClientMessageType::STREAM_UPDATE: {
      ESP_LOGD(TAG, "Stream Started");
#ifdef USE_RESONATE_AUDIO
      audio::AudioStreamInfo stream_audio_stream_info;
      ResonateCodecFormat codec_format;
      std::string codec_header;
      if (process_stream_start_message(message, &stream_audio_stream_info, &codec_format, &codec_header)) {
        std::shared_ptr<ResonateAudioChunk> header_chunk = nullptr;

        if ((codec_format == ResonateCodecFormat::PCM) || (codec_format == ResonateCodecFormat::OPUS)) {
          header_chunk = create_resonate_chunk(sizeof(DummyHeader));
          if (header_chunk == nullptr) {
            ESP_LOGE(TAG, "Memory allocation failed");
            return false;
          }
          DummyHeader *header = reinterpret_cast<DummyHeader *>(header_chunk->get_data());
          header->sample_rate = stream_audio_stream_info.get_sample_rate();
          header->bits_per_sample = stream_audio_stream_info.get_bits_per_sample();
          header->channels = stream_audio_stream_info.get_channels();

          header_chunk->offset = 0;
          header_chunk->size = sizeof(DummyHeader);
          header_chunk->timestamp = 0;

          if (codec_format == ResonateCodecFormat::PCM) {
            header_chunk->chunk_type = CHUNK_TYPE_PCM_DUMMY_HEADER;
          } else if (codec_format == ResonateCodecFormat::OPUS) {
            header_chunk->chunk_type = CHUNK_TYPE_OPUS_DUMMY_HEADER;
          }
        } else if (codec_format == ResonateCodecFormat::FLAC) {
          std::vector<uint8_t> flac_header = base64_decode(codec_header);
          header_chunk = create_resonate_chunk(flac_header.size());
          if (header_chunk == nullptr) {
            ESP_LOGE(TAG, "Memory allocation failed");
            return false;
          }
          std::memcpy((void *) header_chunk->get_data(), (void *) flac_header.data(), flac_header.size());
          header_chunk->offset = 0;
          header_chunk->size = flac_header.size();
          header_chunk->chunk_type = CHUNK_TYPE_FLAC_HEADER;
          header_chunk->timestamp = 0;
        }

        if (!this->encoded_chunk_queue_->add_chunk(header_chunk, 0)) {
          // failed
          ESP_LOGE(TAG, "Failed to send codec header");
        } else if (message_type == ResonateServerToClientMessageType::STREAM_START) {
          this->controls_callbacks_.call(ResonateControls::START);
        }
      }
#else
      if (message_type == ResonateServerToClientMessageType::STREAM_START) {
        // Indicate stream started
        this->controls_callbacks_.call(ResonateControls::START);
      }
#endif
      break;
    }
    case ResonateServerToClientMessageType::STREAM_END: {
      ESP_LOGD(TAG, "Stream ended");
      xEventGroupSetBits(this->event_group_, COMMAND_STOP);  // Handles stopping in the hub component
      this->controls_callbacks_.call(ResonateControls::STOP);
#ifdef USE_RESONATE_AUDIO
      this->encoded_chunk_queue_->reset();
#endif
      break;
    }
    case ResonateServerToClientMessageType::SERVER_HELLO: {
      std::string server_id;
      std::string server_name;
      if (process_server_hello_message(message, &server_id, &server_name)) {
        // TODO: why did I use move instead of passing a pointer in directly...?
        this->server_id_ = std::move(server_id);
        this->server_name_ = std::move(server_name);
        ESP_LOGD(TAG, "Connected to server %s with id %s", this->server_name_.c_str(), this->server_id_.c_str());
      }
      break;
    }
    case ResonateServerToClientMessageType::SERVER_TIME: {
      TimeTransmittedReplacement time_replacement = this->resonate_websocket_->get_last_time_message();
      int64_t offset;
      int64_t max_error;
      if (process_server_time_message(message, timestamp, time_replacement, &offset, &max_error)) {
        this->time_filter_->update(offset, max_error, timestamp);
      }
      this->pending_time_message_ = false;
      break;
    }
    case ResonateServerToClientMessageType::SESSION_UPDATE: {
#ifdef USE_RESONATE_METADATA
      if (process_session_update_message(message, &this->metadata_)) {
        this->metadata_callbacks_.call(this->metadata_);
      }
#endif
      break;
    }
    case ResonateServerToClientMessageType::VOLUME_SET: {
#ifdef USE_RESONATE_AUDIO
      uint8_t volume;
      if (process_volume_set_message(message, &volume)) {
        this->update_volume(volume);
        this->controls_callbacks_.call(ResonateControls::VOLUME_UPDATE);
      }
#endif
      break;
    }
    case ResonateServerToClientMessageType::MUTE_SET: {
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
  this->websocket_write_offset_ = 0;
  this->websocket_len_ = 0;
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
  const PlayerUpdateMessage state = {.state = this->state_, .volume = this->volume_, .muted = this->muted_};
  this->resonate_websocket_->send_player_state_message(&state);
}

bool ResonateHub::send_audio_chunk_(std::shared_ptr<ResonateAudioChunk> audio_chunk, TickType_t ticks_to_wait,
                                    const audio::AudioStreamInfo &stream_info) {
  if (audio_chunk == nullptr) {
    ESP_LOGE(TAG, "Null audio chunk passed to send_audio_chunk_");
    return false;
  }

  if (this->audio_chunk_callbacks_.empty()) {
    // No callbacks registered, return true
    return true;
  }

  // Simple distribution to all consumers
  // Each consumer gets a shared_ptr copy automatically
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

  xEventGroupSetBits(this_resonate->event_group_, TASK_STARTING);

  std::shared_ptr<ResonateAudioChunk> encoded_chunk = nullptr;  // timestamp is in server time domain
  std::shared_ptr<ResonateAudioChunk> decoded_chunk = nullptr;  // timestamp is in client time domain

  std::unique_ptr<ResonateDecoder> decoder = std::make_unique<ResonateDecoder>();
  audio::AudioStreamInfo current_stream_info;

  xEventGroupSetBits(this_resonate->event_group_, TASK_RUNNING);
  while (!(xEventGroupGetBits(this_resonate->event_group_) & COMMAND_STOP)) {
    if (decoded_chunk != nullptr) {
      // Add decoded chunk to the queue
      if ((esp_timer_get_time() > decoded_chunk->timestamp) ||
          this_resonate->send_audio_chunk_(
              decoded_chunk, pdMS_TO_TICKS(current_stream_info.bytes_to_frames(decoded_chunk->get_usable_size())),
              current_stream_info)) {
        // Clear chunk if it was already supposed to start playing (skipping it) or if successfully sent to consumers
        decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
      } else {
        // Try adding again
        continue;
      }
    }

    if (encoded_chunk == nullptr) {
      auto audio_chunk = this_resonate->encoded_chunk_queue_->receive_chunk(pdMS_TO_TICKS(50));
      if (audio_chunk) {
        encoded_chunk = std::static_pointer_cast<ResonateAudioChunk>(audio_chunk);
      }
    }

    if (encoded_chunk != nullptr) {
      // Already have an encoded chunk or successfully received one from the queue

      if ((encoded_chunk->chunk_type != CHUNK_TYPE_ENCODED_AUDIO) &&
          (encoded_chunk->chunk_type != CHUNK_TYPE_DECODED_AUDIO)) {
        // New codec header
        decoder->reset_decoders();
        if (!decoder->process_header(encoded_chunk, &current_stream_info)) {
          ESP_LOGE(TAG, "Failed to process audio codec header");
          xEventGroupSetBits(this_resonate->event_group_, COMMAND_STOP);  // force stop
        } else {
          xEventGroupClearBits(this_resonate->event_group_, COMMAND_STOP);  // where the hell is this getting set?
        }
      } else if ((decoder->get_current_codec() != ResonateCodecFormat::UNSUPPORTED) &&
                 (encoded_chunk->chunk_type == CHUNK_TYPE_ENCODED_AUDIO)) {
        int64_t client_timestamp = this_resonate->time_filter_->compute_client_time(encoded_chunk->timestamp);
        int64_t time_until_playback_us = client_timestamp - esp_timer_get_time();
        if (time_until_playback_us < 0) {
          // Chunk was already supposed to play, skip it!
          encoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
          continue;
        }

        if (this_resonate->time_filter_->get_covariance() >
            TIME_SYNC_ERROR_THRESHOLD_US * TIME_SYNC_ERROR_THRESHOLD_US) {
          // Time sync is unreliable, so delay decoding to avoid timing issues
          const uint32_t time_until_playback_ms =
              static_cast<uint32_t>(time_until_playback_us / 1000LL);  // Convert to milliseconds

          // Wait for half the remaining time or minimum retry delay (whichever is larger)
          // If chunk is too close to playback time, it will be discarded on next iteration
          uint32_t wait_time_ms = std::max(time_until_playback_ms / 2, MIN_RETRY_DELAY_UNRELIABLE_SYNC_MS);
          vTaskDelay(pdMS_TO_TICKS(wait_time_ms));
          continue;
        }

        if (!decoder->decode_audio_chunk(encoded_chunk, decoded_chunk)) {
          ESP_LOGE(TAG, "Failed to decode audio chunk");
        } else {
          decoded_chunk->timestamp = client_timestamp;
        }
      }

      // Clear the encoded chunk. Note, for PCM, decoded_chunk is the same data as encoded_chunk but has its own
      // shared_ptr reference
      encoded_chunk = nullptr;
    }

    static uint32_t high_water_mark = 8192;
    uint32_t new_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
    if (new_high_water_mark < high_water_mark) {
      ESP_LOGD(TAG, "Decode task - High water mark changed from %d to %d.", high_water_mark, new_high_water_mark);
      high_water_mark = new_high_water_mark;
    }
  }

  xEventGroupSetBits(this_resonate->event_group_, TASK_STOPPING);

  decoder->reset_decoders();

  // shared_ptr automatically handles cleanup
  encoded_chunk = nullptr;
  decoded_chunk = nullptr;

  xEventGroupSetBits(this_resonate->event_group_, TASK_STOPPED);
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
#endif

}  // namespace resonate
}  // namespace esphome

#endif
