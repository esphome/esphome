#include "sendspin_hub.h"

#ifdef USE_ESP32

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

#include <algorithm>

#include "esphome/core/application.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/version.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.hub";

static const size_t SENDSPIN_BINARY_CHUNK_HEADER_SIZE = 9;

static const UBaseType_t WEBSOCKET_TASK_PRIORITY = 17;

void SendspinHub::setup() {
  // Load persisted state
  this->load_last_played_server_();
#ifdef USE_SENDSPIN_PLAYER
  this->load_static_delay_();
#endif

  // Create the WebSocket server listener (always active - servers connect to us)
  this->ws_server_ = make_unique<SendspinWsServer>();

  // Configure callbacks for the server
  this->ws_server_->set_new_connection_callback(
      [this](std::unique_ptr<SendspinServerConnection> conn) { this->on_new_connection_(std::move(conn)); });

  this->ws_server_->set_connection_closed_callback([this](int sockfd) { this->on_connection_closed_(sockfd); });

  // Set up connection lookup callback for routing messages
  this->ws_server_->set_find_connection_callback([this](int sockfd) -> SendspinServerConnection * {
    // Only server connections have valid sockfd (client returns -1)
    if (this->current_connection_ != nullptr && this->current_connection_->get_sockfd() == sockfd) {
      return static_cast<SendspinServerConnection *>(this->current_connection_.get());
    }
    if (this->pending_connection_ != nullptr && this->pending_connection_->get_sockfd() == sockfd) {
      return static_cast<SendspinServerConnection *>(this->pending_connection_.get());
    }
    if (this->dying_connection_ != nullptr && this->dying_connection_->get_sockfd() == sockfd) {
      return static_cast<SendspinServerConnection *>(this->dying_connection_.get());
    }
    return nullptr;
  });
}

void SendspinHub::loop() {
  // Handle time synchronization for the active connection via burst strategy
  if (this->current_connection_ != nullptr) {
    auto result = this->time_burst_.loop(this->current_connection_.get());
#ifdef USE_WIFI
    if (result.sent && !this->high_performance_networking_requested_for_time_ &&
        wifi::global_wifi_component->request_high_performance()) {
      this->high_performance_networking_requested_for_time_ = true;
    }
    if (result.burst_completed && this->high_performance_networking_requested_for_time_ &&
        wifi::global_wifi_component->release_high_performance()) {
      this->high_performance_networking_requested_for_time_ = false;
    }
#endif
#ifdef USE_SENDSPIN_SENSOR
    if (result.burst_completed) {
      this->update_sendspin_sensor(
          {.type = SendspinSensorTypes::KALMAN_ERROR,
           .value = static_cast<float>(this->current_connection_->get_time_filter()->get_error())});
    }
#endif
  }

  // Start the WebSocket server when network is connected
  if (this->ws_server_ != nullptr && network::is_connected() && !this->ws_server_->is_started()) {
    this->ws_server_->start(this, this->task_stack_in_psram_, WEBSOCKET_TASK_PRIORITY);
  }

  // Call loop on the current connection if it exists
  if (this->current_connection_ != nullptr) {
    this->current_connection_->loop();
  }

  // Call loop on pending connection if it exists (during handoff)
  if (this->pending_connection_ != nullptr) {
    this->pending_connection_->loop();
  }
}

void SendspinHub::connect_to_server(const std::string &url) {
  ESP_LOGI(TAG, "Initiating client connection to: %s", url.c_str());

  // Create the client connection
  auto client_conn = make_unique<SendspinClientConnection>(url);

  // Disable auto-reconnect for on-demand client connections
  client_conn->set_auto_reconnect(false);

  // Set up callbacks for the client connection
  client_conn->on_connected = [this](SendspinConnection *conn) {
    // Connection established - initiate hello handshake
    this->start(conn);
  };
  client_conn->on_json_message = [this](SendspinConnection *conn, const std::string &message, int64_t timestamp) {
    this->process_json_message_(conn, message, timestamp);
  };
  client_conn->on_binary_message = [this](SendspinConnection *conn, uint8_t *payload, size_t len) {
    this->process_binary_message_(payload, len);
  };
  client_conn->on_handshake_complete = [this](SendspinConnection *conn) {
    this->on_connection_handshake_complete_(conn);
  };
  client_conn->on_disconnected = [this](SendspinConnection *conn) { this->on_connection_lost_(conn); };

  // Initialize time filter and start
  client_conn->init_time_filter();

  // If we have an existing connection, this becomes a pending connection for handoff
  if (this->current_connection_ != nullptr && this->current_connection_->is_connected()) {
    ESP_LOGD(TAG, "Existing connection active, new connection will go through handoff");
    this->pending_connection_ = std::move(client_conn);
    this->pending_connection_->start();
  } else {
    // No existing connection, this becomes the current connection
    this->current_connection_ = std::move(client_conn);
    this->current_connection_->start();
  }
}

void SendspinHub::start(SendspinConnection *conn) {
  // Send hello with exponential backoff: 100ms initial, 3 attempts, 2x backoff
  this->try_send_hello_(conn, 100, 3);
}

void SendspinHub::try_send_hello_(SendspinConnection *conn, uint32_t delay_ms, uint8_t attempts_remaining) {
  const char *key = (conn == this->pending_connection_.get()) ? "hello_pending" : "hello_current";
  this->set_timeout(key, delay_ms, [this, conn, delay_ms, attempts_remaining]() {
    if (this->send_hello_message_(attempts_remaining - 1, conn)) {
      return;  // Done or non-recoverable, stop retrying
    }
    // Transient failure - retry with exponential backoff if attempts remain
    if (attempts_remaining > 1) {
      this->try_send_hello_(conn, delay_ms * 2, attempts_remaining - 1);
    }
  });
}

bool SendspinHub::send_hello_message_(uint8_t remaining_attempts, SendspinConnection *conn) {
  // Verify the connection is still one of our managed connections (it may have been destroyed between retries)
  if (conn != this->current_connection_.get() && conn != this->pending_connection_.get()) {
    ESP_LOGW(TAG, "Connection no longer valid for hello message");
    return true;
  }

  if (conn == nullptr || !conn->is_connected()) {
    ESP_LOGW(TAG, "Cannot send hello - not connected");
    return true;  // Stop retrying, no point if disconnected
  }

  ClientHelloMessage msg;
  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  msg.client_id = get_mac_address_pretty_into_buffer(mac_buf);
  msg.name = App.get_friendly_name();

  DeviceInfoObject device_info;
  device_info.product_name = App.get_name();
  device_info.manufacturer = "ESPHome";
  device_info.software_version = ESPHOME_VERSION;
  msg.device_info = device_info;

  msg.version = 1;

  std::vector<SendspinRole> supported_roles;
#ifdef USE_SENDSPIN_CONTROLLER
  supported_roles.push_back(SendspinRole::CONTROLLER);
#endif

#ifdef USE_SENDSPIN_PLAYER
  supported_roles.push_back(SendspinRole::PLAYER);
  std::vector<AudioSupportedFormatObject> supported_formats;
  supported_formats.push_back({SendspinCodecFormat::FLAC, 2, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::FLAC, 1, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::OPUS, 2, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::OPUS, 1, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::PCM, 2, 48000, 16});
  supported_formats.push_back({SendspinCodecFormat::PCM, 1, 48000, 16});

  // Advertise 80% of the overall buffer capacity. This is to account for the ring buffer now stores chunk metadata. It
  // also better handles cases when rapidly stopping then starting. The media source may not clear enough buffers when
  // restarting, and so the hub can overwhelm it when the next stream starts.
  PlayerSupportObject player_support = {
      .supported_formats = supported_formats,
      .buffer_capacity = this->buffer_size_ * 4 / 5,
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
    artwork_channels.reserve(this->preferred_image_formats_.size());
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

  // Format the hello message using the protocol layer
  std::string hello_message = format_client_hello_message(&msg);

  // Try to queue the message, with callback for when it's actually sent
  esp_err_t err = conn->send_text_message(hello_message, [conn](bool success, int64_t actual_send_time) {
    if (success) {
      // Mark connection's client hello as sent
      conn->set_client_hello_sent(true);
      // Initialize time message tracking - pretend we just sent one to start the delay
      conn->set_last_sent_time_message(actual_send_time);
    } else {
      ESP_LOGW(TAG, "Hello message send failed");
      // Don't retry here - if client disconnected, reconnection will trigger new hello
    }
  });

  if (err == ESP_OK) {
    return true;  // Successfully queued
  }

  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "No client connected for hello message");
    return true;  // Don't retry - wait for reconnection
  }

  // ESP_ERR_NO_MEM or ESP_FAIL - transient failure, retry
  ESP_LOGW(TAG, "Failed to queue hello message (err=%d), %d attempts remaining", err, remaining_attempts);
  return false;
}

#ifdef USE_SENDSPIN_CONTROLLER
void SendspinHub::send_client_command(SendspinControllerCommand command, std::optional<uint8_t> volume,
                                      std::optional<bool> mute) {
  if (this->current_connection_ != nullptr && this->current_connection_->is_connected()) {
    std::string command_message = format_client_command_message(command, volume, mute);
    this->current_connection_->send_text_message(command_message, nullptr);
  }
}
#endif

void SendspinHub::disconnect_from_server(SendspinGoodbyeReason reason) {
  if (this->current_connection_ != nullptr && this->current_connection_->is_connected()) {
    this->current_connection_->disconnect(reason, nullptr);
  }
}

// --- Connection management ---

// NOLINTNEXTLINE(readability-identifier-naming)
void SendspinHub::on_new_connection_(std::unique_ptr<SendspinServerConnection> conn) {
  conn->init_time_filter();

  // Set up message callbacks for the server connection
  conn->on_json_message = [this](SendspinConnection *c, const std::string &message, int64_t timestamp) {
    this->process_json_message_(c, message, timestamp);
  };
  conn->on_binary_message = [this](SendspinConnection *c, uint8_t *payload, size_t len) {
    this->process_binary_message_(payload, len);
  };
  conn->on_handshake_complete = [this](SendspinConnection *c) { this->on_connection_handshake_complete_(c); };

  // Set up connection callbacks
  conn->on_connected = [this](SendspinConnection *c) {
    // WebSocket handshake completed - initiate hello handshake
    this->start(c);
  };
  conn->on_disconnected = [](SendspinConnection *c) {
    // This callback is invoked when the connection detects it's closed
    // The actual cleanup happens in on_connection_closed_ triggered by the server
  };

  if (this->current_connection_ == nullptr) {
    // No existing connection - this becomes the current connection
    ESP_LOGD(TAG, "No existing connection, accepting as current");
    this->current_connection_ = std::move(conn);
    // Note: Don't call start() here - the WebSocket handshake hasn't completed yet.
    // The on_connected callback will trigger start() after the HTTP GET upgrade
    // completes and we can actually send WebSocket frames.
  } else {
    // Already have a connection - this becomes pending for handoff
    ESP_LOGD(TAG, "Existing connection present, setting as pending for handoff");
    if (this->pending_connection_ != nullptr) {
      // Already have a pending connection - reject this one
      ESP_LOGW(TAG, "Already have pending connection, rejecting new connection");
      this->disconnect_and_release_(std::move(conn), SendspinGoodbyeReason::ANOTHER_SERVER);
      return;
    }
    this->pending_connection_ = std::move(conn);
    // The pending connection's on_connected callback will trigger start() with the
    // correct connection pointer once its WebSocket handshake completes.
  }
}

void SendspinHub::on_connection_handshake_complete_(SendspinConnection *conn) {
  ESP_LOGI(TAG, "Connection handshake complete: server_id=%s, connection_reason=%s", conn->get_server_id().c_str(),
           to_cstr(conn->get_connection_reason()));

  // Send client state so server knows our current volume
#ifdef USE_SENDSPIN_PLAYER
  this->publish_client_state(conn);
#endif

  // Check if this is the pending connection completing authentication
  if (this->pending_connection_ != nullptr && this->pending_connection_.get() == conn) {
    // Pending connection authenticated - decide handoff
    bool should_switch = this->should_switch_to_new_server_(this->current_connection_.get(), conn);
    ESP_LOGI(TAG, "Handoff decision: %s", should_switch ? "switch to new" : "keep current");
    this->complete_handoff_(should_switch);
  }
}

void SendspinHub::on_connection_closed_(int sockfd) {
  ESP_LOGD(TAG, "Connection closed callback for socket %d", sockfd);

  // Resolve sockfd to connection pointer, then delegate to shared handler.
  // Note: This callback is only invoked for server connections (from ws_server_).
  // Client connections use on_disconnected → on_connection_lost_() directly.
  if (this->current_connection_ != nullptr && this->current_connection_->get_sockfd() == sockfd) {
    this->on_connection_lost_(this->current_connection_.get());
  } else if (this->pending_connection_ != nullptr && this->pending_connection_->get_sockfd() == sockfd) {
    this->on_connection_lost_(this->pending_connection_.get());
  }
}

void SendspinHub::on_connection_lost_(SendspinConnection *conn) {
  if (conn == nullptr) {
    return;
  }

  if (this->current_connection_ != nullptr && this->current_connection_.get() == conn) {
    ESP_LOGI(TAG, "Current connection lost");
    this->time_burst_.reset();
    this->cleanup_connection_state_();
    this->current_connection_.reset();

    // If we have a pending connection, promote it.
    // No need to call start() - the promoted connection's on_connected callback
    // already triggered the hello handshake when its WebSocket upgrade completed.
    if (this->pending_connection_ != nullptr) {
      ESP_LOGD(TAG, "Promoting pending connection to current");
      this->current_connection_ = std::move(this->pending_connection_);
    }
  } else if (this->pending_connection_ != nullptr && this->pending_connection_.get() == conn) {
    // No cleanup_connection_state_() - the pending connection never streamed,
    // so there's no playback to stop or WiFi to release.
    ESP_LOGD(TAG, "Pending connection lost");
    this->pending_connection_.reset();
  }
}

bool SendspinHub::should_switch_to_new_server_(SendspinConnection *current, SendspinConnection *new_conn) {
  if (current == nullptr || new_conn == nullptr) {
    return new_conn != nullptr;
  }

  auto new_reason = new_conn->get_connection_reason();
  auto current_reason = current->get_connection_reason();

  // New server wants playback -> switch to new
  if (new_reason == SendspinConnectionReason::PLAYBACK) {
    ESP_LOGD(TAG, "New server has playback reason, switching");
    return true;
  }

  // New is discovery, current had playback -> keep current
  if (new_reason == SendspinConnectionReason::DISCOVERY && current_reason == SendspinConnectionReason::PLAYBACK) {
    ESP_LOGD(TAG, "New is discovery, current had playback, keeping current");
    return false;
  }

  // Both discovery -> prefer last played server
  if (this->has_last_played_server_) {
    if (fnv1_hash(new_conn->get_server_id()) == this->last_played_server_hash_) {
      ESP_LOGD(TAG, "New server matches last played server, switching");
      return true;
    }
    if (fnv1_hash(current->get_server_id()) == this->last_played_server_hash_) {
      ESP_LOGD(TAG, "Current server matches last played server, keeping");
      return false;
    }
  }

  // Default: keep existing connection
  ESP_LOGD(TAG, "Default handoff decision: keep existing");
  return false;
}

void SendspinHub::complete_handoff_(bool switch_to_new) {
  if (switch_to_new) {
    ESP_LOGD(TAG, "Completing handoff: switching to new server");
    // Disconnect current, promote pending to current
    if (this->current_connection_ != nullptr) {
      this->time_burst_.reset();
      this->cleanup_connection_state_();
      auto old_current = std::move(this->current_connection_);
      this->current_connection_ = std::move(this->pending_connection_);
      this->disconnect_and_release_(std::move(old_current), SendspinGoodbyeReason::ANOTHER_SERVER);
    } else {
      this->current_connection_ = std::move(this->pending_connection_);
    }
  } else {
    ESP_LOGD(TAG, "Completing handoff: keeping current server");
    // Keep current, disconnect pending. No cleanup_connection_state_() needed -
    // the pending connection never streamed, so there's no playback to stop or WiFi to release.
    if (this->pending_connection_ != nullptr) {
      this->disconnect_and_release_(std::move(this->pending_connection_), SendspinGoodbyeReason::ANOTHER_SERVER);
    }
  }
}

// NOLINTNEXTLINE(readability-identifier-naming)
void SendspinHub::disconnect_and_release_(std::unique_ptr<SendspinConnection> conn, SendspinGoodbyeReason reason) {
  // Store as dying_connection_ so it remains findable by sockfd during the async goodbye send.
  // This prevents httpd from returning ESP_FAIL (and force-closing the socket) when the server
  // sends messages between the handoff decision and the goodbye completion.
  this->dying_connection_ = std::shared_ptr<SendspinConnection>(std::move(conn));
  this->dying_connection_->disconnect(reason, [this]() { this->dying_connection_.reset(); });
}

void SendspinHub::cleanup_connection_state_() {
  ESP_LOGV(TAG, "Cleaning up connection state");

  // Stop controls (for player, etc.)
  this->controls_callbacks_.call(SendspinControls::STOP);

  // Note: Time message state is per-connection and cleaned up when connection is destroyed

#ifdef USE_WIFI
  if (this->high_performance_networking_requested_for_time_ &&
      wifi::global_wifi_component->release_high_performance()) {
    this->high_performance_networking_requested_for_time_ = false;
  }
  if (this->high_performance_networking_requested_for_playback_ &&
      wifi::global_wifi_component->release_high_performance()) {
    this->high_performance_networking_requested_for_playback_ = false;
  }
#endif
}

// --- Persistence ---

void SendspinHub::load_last_played_server_() {
  this->last_played_server_pref_ =
      global_preferences->make_preference<LastPlayedServerPref>(fnv1_hash("sendspin_last_played"));
  LastPlayedServerPref pref{};
  if (this->last_played_server_pref_.load(&pref)) {
    if (pref.server_id_hash != 0) {
      this->last_played_server_hash_ = pref.server_id_hash;
      this->has_last_played_server_ = true;
      ESP_LOGI(TAG, "Loaded last played server hash: 0x%08X", this->last_played_server_hash_);
    }
  }
}

void SendspinHub::persist_last_played_server_(const std::string &server_id) {
  if (server_id.empty()) {
    return;
  }

  uint32_t hash = fnv1_hash(server_id);
  this->last_played_server_hash_ = hash;
  this->has_last_played_server_ = true;

  LastPlayedServerPref pref{};
  pref.server_id_hash = hash;

  if (this->last_played_server_pref_.save(&pref)) {
    ESP_LOGD(TAG, "Persisted last played server: %s (hash: 0x%08X)", server_id.c_str(), hash);
  } else {
    ESP_LOGW(TAG, "Failed to persist last played server");
  }
}

#ifdef USE_SENDSPIN_PLAYER
void SendspinHub::load_static_delay_() {
  this->static_delay_pref_ = global_preferences->make_preference<StaticDelayPref>(fnv1_hash("sendspin_static_delay"));
  StaticDelayPref pref{};
  if (this->static_delay_pref_.load(&pref)) {
    if (pref.static_delay_ms <= 5000) {
      this->static_delay_ms_ = pref.static_delay_ms;
      ESP_LOGI(TAG, "Loaded static delay: %u ms", this->static_delay_ms_);
    } else {
      ESP_LOGW(TAG, "Persisted static delay out of range (%u), ignoring", pref.static_delay_ms);
    }
  } else if (this->initial_static_delay_ms_ > 0) {
    this->static_delay_ms_ = this->initial_static_delay_ms_;
    ESP_LOGI(TAG, "Using initial static delay from config: %u ms", this->static_delay_ms_);
  }
}

void SendspinHub::persist_static_delay_() {
  StaticDelayPref pref{};
  pref.static_delay_ms = this->static_delay_ms_;

  if (this->static_delay_pref_.save(&pref)) {
    ESP_LOGD(TAG, "Persisted static delay: %u ms", this->static_delay_ms_);
  } else {
    ESP_LOGW(TAG, "Failed to persist static delay");
  }
}
#endif  // USE_SENDSPIN_PLAYER

void SendspinHub::process_binary_message_(uint8_t *payload, size_t len) {
  if (len < SENDSPIN_BINARY_CHUNK_HEADER_SIZE) {
    return;
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
        // Audio data (slot 0) - pass raw data to callback (no heap allocation)
        if (!this->send_audio_chunk_(payload + SENDSPIN_BINARY_CHUNK_HEADER_SIZE,
                                     len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE, server_timestamp,
                                     CHUNK_TYPE_ENCODED_AUDIO, 0)) {
          ESP_LOGW(TAG, "Failed to send audio chunk");
        }
      } else {
        ESP_LOGW(TAG, "Unknown player binary slot %d", slot);
      }
#else
      ESP_LOGV(TAG, "Ignoring player binary message (audio not enabled)");
#endif
      break;
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
                               image_format, (int64_t) server_timestamp);
          found_callback = true;
          break;
        }
      }
      if (!found_callback) {
        ESP_LOGW(TAG, "No callback registered for artwork slot %d", slot);
      }
#else
      ESP_LOGV(TAG, "Ignoring artwork message with %zu bytes", len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE);
#endif
      break;
    }
    case SENDSPIN_ROLE_VISUALIZER: {
      // TODO: implement visualizer binary message handling
      ESP_LOGV(TAG, "Ignoring visualizer message with %zu bytes", len - SENDSPIN_BINARY_CHUNK_HEADER_SIZE);
      break;
    }
    default: {
      ESP_LOGW(TAG, "Unknown binary role %d (type %d)", role, binary_type);
      break;
    }
  }
}

bool SendspinHub::process_json_message_(SendspinConnection *conn, const std::string &message, int64_t timestamp) {
  // Parse JSON once and pass the parsed document to all handlers
  JsonDocument doc = json::parse_json(reinterpret_cast<const uint8_t *>(message.c_str()), message.size());
  if (doc.overflowed() || doc.isNull()) {
    ESP_LOGW(TAG, "Failed to parse JSON message");
    return false;
  }
  JsonObject root = doc.as<JsonObject>();

  SendspinServerToClientMessageType message_type = determine_message_type(root);

  switch (message_type) {
    case SendspinServerToClientMessageType::STREAM_START: {
      ESP_LOGD(TAG, "Stream Started");
#ifdef USE_SENDSPIN_PLAYER
#ifdef USE_WIFI
      ESP_LOGD(TAG, "Requesting high performance networking for playback");
      if (!this->high_performance_networking_requested_for_playback_ &&
          wifi::global_wifi_component->request_high_performance()) {
        this->high_performance_networking_requested_for_playback_ = true;
      }
#endif

      StreamStartMessage stream_msg;
      if (process_stream_start_message(root, &stream_msg)) {
        if (!stream_msg.player.has_value()) {
          ESP_LOGE(TAG, "Stream start message has no player object");
          break;
        }

        // Start player immediately so the task can drain the ring buffer, if necessary. Also reduces overall latency to
        // first sound.
        this->controls_callbacks_.call(SendspinControls::START);

        const ServerPlayerStreamObject &player_obj = stream_msg.player.value();
        // Store the initial stream parameters
        this->current_stream_params_ = player_obj;

        if (!player_obj.bit_depth.has_value() || !player_obj.channels.has_value() ||
            !player_obj.sample_rate.has_value() || !player_obj.codec.has_value()) {
          ESP_LOGE(TAG, "Stream start message missing required audio parameters");
          break;
        }

        auto codec = player_obj.codec.value();
        audio::AudioStreamInfo stream_audio_stream_info(player_obj.bit_depth.value(), player_obj.channels.value(),
                                                        player_obj.sample_rate.value());
        bool header_sent = false;

        if ((codec == SendspinCodecFormat::PCM) || (codec == SendspinCodecFormat::OPUS)) {
          DummyHeader header;
          header.sample_rate = stream_audio_stream_info.get_sample_rate();
          header.bits_per_sample = stream_audio_stream_info.get_bits_per_sample();
          header.channels = stream_audio_stream_info.get_channels();

          ChunkType chunk_type =
              (codec == SendspinCodecFormat::PCM) ? CHUNK_TYPE_PCM_DUMMY_HEADER : CHUNK_TYPE_OPUS_DUMMY_HEADER;

          header_sent = this->send_audio_chunk_(reinterpret_cast<const uint8_t *>(&header), sizeof(DummyHeader), 0,
                                                chunk_type, pdMS_TO_TICKS(100));
        } else if (codec == SendspinCodecFormat::FLAC) {
          if (!player_obj.codec_header.has_value()) {
            ESP_LOGE(TAG, "FLAC codec header missing");
            break;
          }
          std::vector<uint8_t> flac_header = base64_decode(player_obj.codec_header.value());
          header_sent = this->send_audio_chunk_(flac_header.data(), flac_header.size(), 0, CHUNK_TYPE_FLAC_HEADER,
                                                pdMS_TO_TICKS(100));
        }

        if (!header_sent) {
          ESP_LOGE(TAG, "Failed to send codec header");
          this->controls_callbacks_.call(SendspinControls::STOP);
        }
      }
#else
      this->controls_callbacks_.call(SendspinControls::START);
#endif
      break;
    }
    case SendspinServerToClientMessageType::STREAM_END: {
      StreamEndMessage end_msg;
      if (process_stream_end_message(root, &end_msg)) {
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
      if (process_stream_clear_message(root, &clear_msg)) {
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
      if (process_server_hello_message(root, &hello_msg)) {
        this->server_information_ = std::move(hello_msg.server);
        ESP_LOGD(TAG, "Connected to server %s with id %s (reason: %s)", this->server_information_.name.c_str(),
                 this->server_information_.server_id.c_str(), to_cstr(hello_msg.connection_reason));

        // Store server info in the connection that sent this message
        if (conn != nullptr) {
          // Update the connection with server info from the hello message
          conn->set_server_id(this->server_information_.server_id);
          conn->set_server_name(this->server_information_.name);
          conn->set_connection_reason(hello_msg.connection_reason);
          conn->set_server_hello_received(true);

          // Notify that handshake is complete via the connection's callback
          if (conn->on_handshake_complete) {
            conn->on_handshake_complete(conn);
          }
        }
      }
      break;
    }
    case SendspinServerToClientMessageType::SERVER_TIME: {
      if (conn == nullptr) {
        ESP_LOGW(TAG, "Received time message but no connection context");
        break;
      }

      // peek_time_replacement() is thread-safe (backed by a FreeRTOS queue), so this
      // can safely run on the callback thread without racing with send_time_message().
      int64_t offset;
      int64_t max_error;
      if (process_server_time_message(root, timestamp, conn->peek_time_replacement(), &offset, &max_error)) {
        defer([this, offset, max_error, timestamp]() {
          if (this->current_connection_ != nullptr) {
            this->time_burst_.on_time_response(this->current_connection_.get(), offset, max_error, timestamp);
          }
        });
      }
      break;
    }
    case SendspinServerToClientMessageType::SERVER_STATE: {
      ServerStateMessage state_msg;
      if (process_server_state_message(root, &state_msg)) {
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
#endif
      }

      break;
    }
    case SendspinServerToClientMessageType::SERVER_COMMAND: {
#ifdef USE_SENDSPIN_PLAYER
      ServerCommandMessage cmd_msg;
      if (process_server_command_message(root, &cmd_msg)) {
        if (!cmd_msg.player.has_value()) {
          ESP_LOGV(TAG, "Server command has no player commands");
          break;
        }

        const ServerPlayerCommandObject &player_cmd = cmd_msg.player.value();

        // Handle volume command
        if (player_cmd.command == SendspinPlayerCommand::VOLUME && player_cmd.volume.has_value()) {
          this->update_volume(player_cmd.volume.value());
          this->controls_callbacks_.call(SendspinControls::VOLUME_UPDATE);
        }

        // Handle mute command
        if (player_cmd.command == SendspinPlayerCommand::MUTE && player_cmd.mute.has_value()) {
          this->update_muted(player_cmd.mute.value());
          this->controls_callbacks_.call(SendspinControls::MUTE_UPDATE);
        }

        // Handle set_static_delay command
        if (player_cmd.command == SendspinPlayerCommand::SET_STATIC_DELAY && player_cmd.static_delay_ms.has_value()) {
          this->update_static_delay(player_cmd.static_delay_ms.value());
        }
      }
#endif
      break;
    }
    case SendspinServerToClientMessageType::GROUP_UPDATE: {
      GroupUpdateMessage group_msg;
      if (process_group_update_message(root, &group_msg)) {
        // Copy the group object and defer processing to main loop
        auto group_obj_copy = group_msg.group;
        defer([this, group_obj_copy]() {
          // Apply delta updates to group state
          apply_group_update_deltas(&this->group_state_, group_obj_copy);

          // Notify callbacks (media player will use this for playback state)
          this->group_update_callbacks_.call(group_obj_copy);

          // Persist last played server when playback starts
          if (group_obj_copy.playback_state.has_value() &&
              group_obj_copy.playback_state.value() == SendspinPlaybackState::PLAYING) {
            if (this->current_connection_ != nullptr) {
              const std::string &server_id = this->current_connection_->get_server_id();
              if (!server_id.empty()) {
                this->persist_last_played_server_(server_id);
              }
            }
          }

          ESP_LOGD(TAG, "Group update - state: %s, id: %s, name: %s",
                   this->group_state_.playback_state.has_value() ? to_cstr(this->group_state_.playback_state.value())
                                                                 : "unchanged",
                   this->group_state_.group_id.value_or("").c_str(),
                   this->group_state_.group_name.value_or("").c_str());
        });
      }
      break;
    }
    default:
      ESP_LOGW(TAG, "Unhandled server message type: %s",
               root["type"].is<const char *>() ? root["type"].as<const char *>() : "unknown");
  }

  return true;  // Successfully processed message
}

#ifdef USE_SENDSPIN_PLAYER
void SendspinHub::update_muted(bool is_muted) {
  this->muted_ = is_muted;
  this->publish_client_state(this->current_connection_.get());
}
void SendspinHub::update_state(SendspinClientState state) {
  this->state_ = state;
  this->publish_client_state(this->current_connection_.get());
}
void SendspinHub::update_volume(uint8_t volume) {
  this->volume_ = volume;
  this->publish_client_state(this->current_connection_.get());
}
void SendspinHub::update_static_delay(uint16_t delay_ms) {
  if (delay_ms > 5000) {
    delay_ms = 5000;
  }
  this->static_delay_ms_ = delay_ms;
  this->persist_static_delay_();
  this->publish_client_state(this->current_connection_.get());
}
void SendspinHub::set_static_delay_adjustable(bool adjustable) {
  this->static_delay_adjustable_ = adjustable;
  this->publish_client_state(this->current_connection_.get());
}

void SendspinHub::publish_client_state(SendspinConnection *conn) {
  if (conn == nullptr || !conn->is_connected() || !conn->is_handshake_complete()) {
    return;
  }

  ClientStateMessage state_msg;
  state_msg.state = this->state_;

  ClientPlayerStateObject player_state;
  player_state.volume = this->volume_;
  player_state.muted = this->muted_;
  player_state.static_delay_ms = this->static_delay_ms_;
  if (this->static_delay_adjustable_) {
    player_state.supported_commands = {SendspinPlayerCommand::SET_STATIC_DELAY};
  }

  state_msg.player = player_state;

  // Use protocol formatting with base class send_text_message (works for both connection types)
  std::string state_message = format_client_state_message(&state_msg);
  conn->send_text_message(state_message, nullptr);
}

bool SendspinHub::send_audio_chunk_(const uint8_t *data, size_t data_size, int64_t timestamp, ChunkType chunk_type,
                                    TickType_t ticks_to_wait) {
  if (data == nullptr || data_size == 0) {
    ESP_LOGE(TAG, "Invalid data passed to send_audio_chunk_");
    return false;
  }

  if (!this->audio_chunk_callback_) {
    return true;
  }

  return this->audio_chunk_callback_(data, data_size, timestamp, chunk_type, ticks_to_wait);
}
#endif

#ifdef USE_SENDSPIN_METADATA
uint32_t SendspinHub::get_track_progress_ms() {
  if (!this->metadata_.progress.has_value()) {
    return 0;
  }

  const auto &progress = this->metadata_.progress.value();

  // If paused (playback_speed == 0), return the snapshot value directly
  if (progress.playback_speed == 0) {
    return progress.track_progress;
  }

  int64_t client_target = this->get_client_time(this->metadata_.timestamp);
  if (client_target == 0) {
    return progress.track_progress;
  }

  // calculated_progress = track_progress + (now - metadata_client_time) * playback_speed / 1_000_000
  int64_t elapsed_us = esp_timer_get_time() - client_target;
  int64_t calculated = static_cast<int64_t>(progress.track_progress) +
                       elapsed_us * static_cast<int64_t>(progress.playback_speed) / 1000000;

  if (progress.track_duration != 0) {
    calculated = std::max(std::min(calculated, static_cast<int64_t>(progress.track_duration)), (int64_t) 0);
  } else {
    calculated = std::max(calculated, (int64_t) 0);
  }

  return static_cast<uint32_t>(calculated);
}

uint32_t SendspinHub::get_track_duration_ms() {
  if (!this->metadata_.progress.has_value()) {
    return 0;
  }
  return this->metadata_.progress.value().track_duration;
}
#endif

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
