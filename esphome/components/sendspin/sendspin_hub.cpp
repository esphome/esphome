#include "sendspin_hub.h"

#if defined(USE_ESP_IDF)
#ifdef USE_SENDSPIN_PLAYER
#include "sendspin_decoder.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_chunk.h"
#endif

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

#include "esphome/core/application.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.hub";

static const size_t SENDSPIN_BINARY_CHUNK_HEADER_SIZE = 9;

#ifdef USE_SENDSPIN_PLAYER
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
};

void SendspinHub::setup() {
  this->sendspin_websocket_ = make_unique<SendspinWebsocket>();
  if (this->sendspin_websocket_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create sendspin object.");
    this->mark_failed();
  }

  this->time_filter_ = make_unique<SendspinTimeFilter>(this->kalman_process_error_, this->kalman_forget_factor_);
  if (this->time_filter_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create sendspin time filter.");
    this->mark_failed();
  }
}

void SendspinHub::send_time_message_() {
  if (!this->sendspin_websocket_->is_connected() || this->pending_time_message_ || !this->hello_message_sent_) {
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

  this->sendspin_websocket_->send_time_message();
  this->last_sent_time_message_ = esp_timer_get_time();
  this->pending_time_message_ = true;
#ifdef USE_WIFI
  if (!this->high_performance_networking_requested_for_time_ &&
      wifi::global_wifi_component->request_high_performance()) {
    this->high_performance_networking_requested_for_time_ = true;
  }
#endif

#ifdef USE_SENDSPIN_SENSOR
  this->update_sendspin_sensor(
      {.type = SendspinSensorTypes::KALMAN_ERROR, .value = static_cast<float>(this->time_filter_->get_error())});
#endif
}

void SendspinHub::loop() {
  this->send_time_message_();

  if (network::is_connected() && !this->sendspin_websocket_->is_started()) {
    this->sendspin_websocket_->start_server(websocket_server_handler, websocket_close_callback, (void *) this,
                                            this->task_stack_in_psram_, WEBSOCKET_TASK_PRIORITY);
  }
}

void SendspinHub::start() {
  ClientHelloMessage msg;
  msg.client_id = get_mac_address_pretty();
  msg.name = App.get_friendly_name();

  DeviceInfoObject device_info;
  device_info.product_name = App.get_name();
  device_info.manufacturer = "ESPHome";
  device_info.software_version = ESPHOME_VERSION;
  msg.device_info = device_info;

  msg.version = 1;

  std::vector<SendspinRole> supported_roles;
  // TODO: Don't hardcode controller role
  supported_roles.push_back(SendspinRole::CONTROLLER);

#ifdef USE_SENDSPIN_PLAYER
  supported_roles.push_back(SendspinRole::PLAYER);
  std::vector<AudioSupportedFormatObject> supported_formats;
  supported_formats.push_back({SendspinCodecFormat::FLAC, 2, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::FLAC, 1, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::OPUS, 2, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::OPUS, 1, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::PCM, 2, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::PCM, 1, 48000, 16});

  PlayerSupportObject player_support = {
      .supported_formats = supported_formats,
      .buffer_capacity = this->buffer_size_,
      .supported_commands = {SendspinPlayerCommand::VOLUME, SendspinPlayerCommand::MUTE},
  };
  msg.player_v1_support = player_support;
#endif

#ifdef USE_SENDSPIN_METADATA
  supported_roles.push_back(SendspinRole::METADATA);
#endif

#ifdef USE_SENDSPIN_ARTWORK
  if (!this->preferred_image_formats_.empty()) {
    supported_roles.push_back(SendspinRole::ARTWORK);

    std::vector<ArtworkChannelFormatObject> artwork_channels;
    for (const auto &pref : this->preferred_image_formats_) {
      artwork_channels.push_back({pref.source, pref.format, pref.width, pref.height});
    }

    ArtworkSupportObject artwork_support = {
        .channels = artwork_channels,
    };
    msg.artwork_v1_support = artwork_support;
  }
#endif

  msg.supported_roles = supported_roles;

  this->sendspin_websocket_->send_hello_message(&msg);
  this->last_sent_time_message_ = esp_timer_get_time();
  this->hello_message_sent_ = true;
}

#ifdef USE_SENDSPIN_CONTROLLER
void SendspinHub::send_client_command(SendspinCommandType command, std::optional<uint8_t> volume,
                                      std::optional<bool> mute) {
  if (this->sendspin_websocket_->is_connected()) {
    this->sendspin_websocket_->send_client_command_message(command, volume, mute);
  }
}
#endif

void SendspinHub::disconnect_from_server(SendspinGoodbyeReason reason) {
  if (this->sendspin_websocket_->is_connected()) {
    this->sendspin_websocket_->send_goodbye_reason(reason);
    // TODO: Fix this hack. We should create a method to know if the goodbye reason has successfully sent
    this->set_timeout(1000, [this]() { this->sendspin_websocket_->disconnect(); });
  }
}

void SendspinHub::websocket_close_callback(void *context) {
  SendspinHub *this_sendspin = (SendspinHub *) context;
  this_sendspin->controls_callbacks_.call(SendspinControls::STOP);

  this_sendspin->time_filter_->reset();
  this_sendspin->hello_message_sent_ = false;
  this_sendspin->pending_time_message_ = false;
  this_sendspin->deallocate_websocket_payload_();

#ifdef USE_WIFI
  if (this_sendspin->high_performance_networking_requested_for_time_ &&
      wifi::global_wifi_component->release_high_performance()) {
    this_sendspin->high_performance_networking_requested_for_time_ = false;
  }
  if (this_sendspin->high_performance_networking_requested_for_playback_ &&
      wifi::global_wifi_component->release_high_performance()) {
    this_sendspin->high_performance_networking_requested_for_playback_ = false;
  }
#endif

  ESP_LOGD(TAG, "Connection closed");
}

esp_err_t SendspinHub::websocket_server_handler(httpd_req_t *req) {
  int64_t timestamp = esp_timer_get_time();
  SendspinHub *this_sendspin = (SendspinHub *) req->user_ctx;

  esp_err_t err = ESP_OK;

  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, a new connection was opened");
    delay(250);
    this_sendspin->start();

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

    if (this_sendspin->websocket_write_offset_ == 0) {
      if (this_sendspin->websocket_payload_ != nullptr) {
        ESP_LOGE(TAG, "websocket payload wasn't deallocated, closing connection");
        this_sendspin->deallocate_websocket_payload_();
        return ESP_FAIL;
      }
      this_sendspin->websocket_payload_ = allocator.allocate(this_sendspin->websocket_len_ + new_length);
    } else {
      uint8_t *new_payload =
          allocator.reallocate(this_sendspin->websocket_payload_, this_sendspin->websocket_len_ + new_length);
      if (new_payload == nullptr) {
        this_sendspin->deallocate_websocket_payload_();
      }
      this_sendspin->websocket_payload_ = new_payload;
    }

    if (this_sendspin->websocket_payload_ == nullptr) {
      ESP_LOGE(TAG, "Failed to calloc memory for buffer");
      return ESP_ERR_NO_MEM;
    }

    this_sendspin->websocket_len_ += new_length;  // Successfully allocated, update length

    ws_pkt.payload = this_sendspin->websocket_payload_ + this_sendspin->websocket_write_offset_;

    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      this_sendspin->deallocate_websocket_payload_();
      return ret;
    }
    this_sendspin->websocket_write_offset_ += ws_pkt.len;

    if (is_fin) {
      if (is_text) {
        // Create string from payload for JSON processing
        const std::string message(this_sendspin->websocket_payload_,
                                  this_sendspin->websocket_payload_ + this_sendspin->websocket_len_);
        this_sendspin->process_json_message_(message, timestamp);
        // Always deallocate after JSON processing
        this_sendspin->deallocate_websocket_payload_();
      } else if (is_binary) {
        if (this_sendspin->process_binary_message_(this_sendspin->websocket_payload_, this_sendspin->websocket_len_)) {
          // Ownership was transferred, just clear the pointer and reset lengths
          this_sendspin->websocket_payload_ = nullptr;
          this_sendspin->websocket_write_offset_ = 0;
          this_sendspin->websocket_len_ = 0;
        } else {
          // Ownership not transferred, we must deallocate
          this_sendspin->deallocate_websocket_payload_();
        }
      } else {
        // Unknown type - deallocate payload
        this_sendspin->deallocate_websocket_payload_();
      }
    }
  }

  return err;
}

bool SendspinHub::process_binary_message_(uint8_t *payload, size_t len) {
  if (len < SENDSPIN_BINARY_CHUNK_HEADER_SIZE) {
    // Packet too short for sendspin binary message header
    return false;  // deallocate payload
  }

  uint8_t binary_type = payload[0];
  uint8_t role = get_binary_role(binary_type);
  uint8_t slot = get_binary_slot(binary_type);

  // Use the big endian datatype helpers for converting to host format
  int64_be_t server_timestamp;
  std::memcpy((void *) &server_timestamp, (void *) (payload + 1), sizeof(server_timestamp));

  switch (role) {
    case SENDSPIN_ROLE_PLAYER: {
#ifdef USE_SENDSPIN_PLAYER
      if (slot == 0) {
        // Audio data (slot 0)
        // Create a shared_ptr chunk that takes ownership of the payload
        auto audio_chunk = create_sendspin_chunk_from_buffer(payload, len);
        if (audio_chunk == nullptr) {
          ESP_LOGE(TAG, "Failed to allocate SendspinAudioChunk");
          return false;  // deallocate payload
        }
        audio_chunk->offset = SENDSPIN_BINARY_CHUNK_HEADER_SIZE;
        audio_chunk->size = len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE;
        audio_chunk->timestamp = server_timestamp;
        audio_chunk->chunk_type = CHUNK_TYPE_ENCODED_AUDIO;
        audio::AudioStreamInfo stream_info(this->current_stream_params_.bit_depth.value(),
                                           this->current_stream_params_.channels.value(),
                                           this->current_stream_params_.sample_rate.value());

        if (!this->send_audio_chunk_(audio_chunk, 0, stream_info)) {
          ESP_LOGE(TAG, "Failed to send audio chunk");
        }
        // No need to manually release - shared_ptr handles cleanup automatically
        return true;  // don't deallocate payload, just clear pointer
      } else {
        ESP_LOGW(TAG, "Unknown player binary slot %d", slot);
      }
#else
      // Not built with audio, so ownership not transferred
      ESP_LOGD(TAG, "Ignoring player binary message (audio not enabled)");
#endif
      return false;  // deallocate payload
    }
    case SENDSPIN_ROLE_ARTWORK: {
#ifdef USE_SENDSPIN_ARTWORK
      // Find the format preference for this slot
      SendspinImageFormat image_format = SendspinImageFormat::JPEG;  // default fallback
      for (const auto &pref : this->preferred_image_formats_) {
        if (pref.slot == slot) {
          image_format = pref.format;
          break;
        }
      }

      // Route to slot-specific callbacks (linear search)
      bool found_callback = false;
      for (auto &entry : this->image_slot_callbacks_) {
        if (entry.slot == slot) {
          entry.callbacks.call(payload + SENDSPIN_BINARY_CHUNK_HEADER_SIZE, len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE,
                               image_format);
          found_callback = true;
          break;
        }
      }
      if (!found_callback) {
        ESP_LOGW(TAG, "No callback registered for artwork slot %d", slot);
      }
#else
      ESP_LOGD(TAG, "Ignoring artwork message with %zu bytes", len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE);
#endif
      return false;  // deallocate payload
    }
    case SENDSPIN_ROLE_VISUALIZER: {
      // TODO: implement visualizer binary message handling
      ESP_LOGD(TAG, "Ignoring visualizer message with %zu bytes", len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE);
      return false;  // deallocate payload
    }
    default: {
      ESP_LOGW(TAG, "Unknown binary role %d (type %d)", role, binary_type);
      break;
    }
  }

  return false;  // default to deallocate payload
}

bool SendspinHub::process_json_message_(const std::string &message, int64_t timestamp) {
  SendspinServerToClientMessageType message_type = determine_message_type(message);

  switch (message_type) {
    case SendspinServerToClientMessageType::STREAM_START: {
      ESP_LOGD(TAG, "Stream Started");
#ifdef USE_SENDSPIN_PLAYER
#ifdef USE_WIFI
      ESP_LOGI(TAG, "Requesting high performance networking for playback");
      if (!this->high_performance_networking_requested_for_playback_ &&
          wifi::global_wifi_component->request_high_performance()) {
        this->high_performance_networking_requested_for_playback_ = true;
      }
#endif

      StreamStartMessage stream_msg;
      if (process_stream_start_message(message, &stream_msg)) {
        if (!stream_msg.player.has_value()) {
          ESP_LOGE(TAG, "Stream start message has no player object");
          break;
        }

        const ServerPlayerStreamObject &player_obj = stream_msg.player.value();
        // Store the initial stream parameters
        this->current_stream_params_ = player_obj;

        audio::AudioStreamInfo stream_audio_stream_info(player_obj.bit_depth.value(), player_obj.channels.value(),
                                                        player_obj.sample_rate.value());
        std::shared_ptr<SendspinAudioChunk> header_chunk = nullptr;

        if ((player_obj.codec.value() == SendspinCodecFormat::PCM) ||
            (player_obj.codec.value() == SendspinCodecFormat::OPUS)) {
          header_chunk = create_sendspin_chunk(sizeof(DummyHeader));
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

          if (player_obj.codec.value() == SendspinCodecFormat::PCM) {
            header_chunk->chunk_type = CHUNK_TYPE_PCM_DUMMY_HEADER;
          } else if (player_obj.codec.value() == SendspinCodecFormat::OPUS) {
            header_chunk->chunk_type = CHUNK_TYPE_OPUS_DUMMY_HEADER;
          }
        } else if (player_obj.codec.value() == SendspinCodecFormat::FLAC) {
          if (!player_obj.codec_header.has_value()) {
            ESP_LOGE(TAG, "FLAC codec header missing");
            break;
          }
          std::vector<uint8_t> flac_header = base64_decode(player_obj.codec_header.value());
          header_chunk = create_sendspin_chunk(flac_header.size());
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

        if (!this->send_audio_chunk_(header_chunk, 0, stream_audio_stream_info)) {
          ESP_LOGE(TAG, "Failed to send codec header");
        } else {
          this->controls_callbacks_.call(SendspinControls::START);
        }
      }
#else
      this->controls_callbacks_.call(SendspinControls::START);
#endif
      break;
    }
    case SendspinServerToClientMessageType::STREAM_END: {
      StreamEndMessage end_msg;
      if (process_stream_end_message(message, &end_msg)) {
        // If roles is not specified, end all streams (default behavior)
        bool end_player = !end_msg.roles.has_value();
        bool end_artwork = !end_msg.roles.has_value();
        bool end_visualizer = !end_msg.roles.has_value();

        // Check which specific roles to end if roles array is provided
        if (end_msg.roles.has_value()) {
          for (const auto &role : end_msg.roles.value()) {
            if (role == "player") {
              end_player = true;
            } else if (role == "artwork") {
              end_artwork = true;
            } else if (role == "visualizer") {
              end_visualizer = true;
            }
          }
        }

        ESP_LOGD(TAG, "Stream ended - player:%d artwork:%d visualizer:%d", end_player, end_artwork, end_visualizer);

        if (end_player) {
          this->controls_callbacks_.call(SendspinControls::STOP);
#if defined(USE_SENDSPIN_PLAYER) && defined(USE_WIFI)
          if (this->high_performance_networking_requested_for_playback_ &&
              wifi::global_wifi_component->release_high_performance()) {
            this->high_performance_networking_requested_for_playback_ = false;
          }
#endif
        }

        // TODO: Handle artwork and visualizer stream endings when implemented
      }
      break;
    }
    case SendspinServerToClientMessageType::STREAM_CLEAR: {
      StreamClearMessage clear_msg;
      if (process_stream_clear_message(message, &clear_msg)) {
        // If roles is not specified, clear player and visualizer (default behavior per spec)
        bool clear_player = !clear_msg.roles.has_value();
        bool clear_visualizer = !clear_msg.roles.has_value();

        // Check which specific roles to clear if roles array is provided
        if (clear_msg.roles.has_value()) {
          for (const auto &role : clear_msg.roles.value()) {
            if (role == "player") {
              clear_player = true;
            } else if (role == "visualizer") {
              clear_visualizer = true;
            }
          }
        }

        ESP_LOGD(TAG, "Stream clear - player:%d visualizer:%d", clear_player, clear_visualizer);

        if (clear_player) {
#ifdef USE_SENDSPIN_PLAYER
          this->controls_callbacks_.call(SendspinControls::CLEAR);
#endif
        }

        // TODO: Handle visualizer stream clearing when implemented
      }
      break;
    }
    case SendspinServerToClientMessageType::SERVER_HELLO: {
      ServerHelloMessage hello_msg;
      if (process_server_hello_message(message, &hello_msg)) {
        this->server_information_ = std::move(hello_msg.server);
        ESP_LOGD(TAG, "Connected to server %s with id %s", this->server_information_.name.c_str(),
                 this->server_information_.server_id.c_str());

        // Send client state so server knows our current volume
#ifdef USE_SENDSPIN_PLAYER
        this->publish_client_state();
#endif
      }
      break;
    }
    case SendspinServerToClientMessageType::SERVER_TIME: {
      TimeTransmittedReplacement time_replacement = this->sendspin_websocket_->get_last_time_message();
      int64_t offset;
      int64_t max_error;
      if (process_server_time_message(message, timestamp, time_replacement, &offset, &max_error)) {
        this->time_filter_->update(offset, max_error, timestamp);
      }
      this->pending_time_message_ = false;
#ifdef USE_WIFI
      if (this->high_performance_networking_requested_for_time_ &&
          wifi::global_wifi_component->release_high_performance()) {
        this->high_performance_networking_requested_for_time_ = false;
      }
#endif
      break;
    }
    case SendspinServerToClientMessageType::SERVER_STATE: {
      ServerStateMessage state_msg;
      if (process_server_state_message(message, &state_msg)) {
        // Copy the message data and defer processing to main loop
        auto msg_copy = state_msg;
#ifdef USE_SENDSPIN_CONTROLLER
        if (msg_copy.controller.has_value()) {
          this->controller_state_ = msg_copy.controller.value();
        }
#endif
#ifdef USE_SENDSPIN_METADATA
        defer([this, msg_copy]() {
          if (msg_copy.metadata.has_value()) {
            // Apply delta updates to stored metadata
            apply_metadata_state_deltas(&this->metadata_, msg_copy.metadata.value());
            this->metadata_callbacks_.call(this->metadata_);
          }
        });
        // Wake the main loop immediately to process the deferred callback (~12μs latency vs 0-16ms)
#endif
      }

      break;
    }
    case SendspinServerToClientMessageType::SERVER_COMMAND: {
#ifdef USE_SENDSPIN_PLAYER
      ServerCommandMessage cmd_msg;
      if (process_server_command_message(message, &cmd_msg)) {
        if (!cmd_msg.player.has_value()) {
          ESP_LOGD(TAG, "Server command has no player commands");
          break;
        }

        const ServerPlayerCommandObject &player_cmd = cmd_msg.player.value();

        // Handle volume command
        if (player_cmd.command == SendspinCommandType::VOLUME && player_cmd.volume.has_value()) {
          this->update_volume(player_cmd.volume.value());
          this->controls_callbacks_.call(SendspinControls::VOLUME_UPDATE);
        }

        // Handle mute command
        if (player_cmd.command == SendspinCommandType::MUTE && player_cmd.mute.has_value()) {
          this->update_muted(player_cmd.mute.value());
          this->controls_callbacks_.call(SendspinControls::MUTE_UPDATE);
        }
      }
#endif
      break;
    }
    case SendspinServerToClientMessageType::GROUP_UPDATE: {
      GroupUpdateMessage group_msg;
      if (process_group_update_message(message, &group_msg)) {
        // Copy the group object and defer processing to main loop
        auto group_obj_copy = group_msg.group;
        defer([this, group_obj_copy]() {
          // Apply delta updates to group state
          apply_group_update_deltas(&this->group_state_, group_obj_copy);

          // Notify callbacks (media player will use this for playback state)
          this->group_update_callbacks_.call(group_obj_copy);

          ESP_LOGD(TAG, "Group update - state: %s, id: %s, name: %s",
                   this->group_state_.playback_state.has_value() ? to_string(this->group_state_.playback_state.value())
                                                                 : "unchanged",
                   this->group_state_.group_id.value_or("").c_str(),
                   this->group_state_.group_name.value_or("").c_str());
        });
      }
      break;
    }
    default:
      ESP_LOGW(TAG, "Unhandled server message: %s", message.c_str());
  }

  return true;  // Successfully processed message
}

void SendspinHub::deallocate_websocket_payload_() {
  if (this->websocket_payload_ != nullptr) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
    allocator.deallocate(this->websocket_payload_, this->websocket_len_);
    this->websocket_payload_ = nullptr;
  }
  this->websocket_write_offset_ = 0;
  this->websocket_len_ = 0;
}

#ifdef USE_SENDSPIN_PLAYER
void SendspinHub::update_muted(bool is_muted) {
  this->muted_ = is_muted;
  this->publish_client_state();
}
void SendspinHub::update_state(SendspinClientState state) {
  this->state_ = state;
  this->publish_client_state();
}
void SendspinHub::update_volume(uint8_t volume) {
  this->volume_ = volume;
  this->publish_client_state();
}

void SendspinHub::publish_client_state() {
  if (!this->sendspin_websocket_ || !this->sendspin_websocket_->is_connected() || !this->hello_message_sent_) {
    return;
  }
  ClientStateMessage state_msg;
  state_msg.state = this->state_;

  ClientPlayerStateObject player_state;
  player_state.volume = this->volume_;
  player_state.muted = this->muted_;

  state_msg.player = player_state;

  this->sendspin_websocket_->send_client_state_message(&state_msg);
}

bool SendspinHub::send_audio_chunk_(std::shared_ptr<SendspinAudioChunk> audio_chunk, TickType_t ticks_to_wait,
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
#endif

}  // namespace sendspin
}  // namespace esphome

#endif
