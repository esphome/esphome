#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "sendspin_client_connection.h"
#include "sendspin_connection.h"
#include "sendspin_protocol.h"
#include "sendspin_server_connection.h"
#include "sendspin_time_burst.h"
#include "sendspin_ws_server.h"

#ifdef USE_SENDSPIN_PLAYER
#include "esphome/components/audio/audio.h"
#endif

#ifdef USE_SENDSPIN_ARTWORK
#include <vector>
#endif

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <freertos/FreeRTOS.h>

#include <limits>

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

#ifdef USE_SENDSPIN_SENSOR
enum class SendspinSensorTypes {
  KALMAN_ERROR,
  TRACK_PROGRESS,
  TRACK_DURATION,
};

struct SendspinSensorUpdate {
  SendspinSensorTypes type;
  float value;
};
#endif

#ifdef USE_SENDSPIN_ARTWORK
struct ImageSlotPreference {
  uint8_t slot;
  SendspinImageSource source;
  SendspinImageFormat format;
  uint16_t width;
  uint16_t height;
};

struct ImageSlotCallback {
  uint8_t slot;
  CallbackManager<void(const uint8_t *, size_t, SendspinImageFormat, int64_t)> callbacks;
};
#endif

// These are meant for internal controls to keep child components in sync
enum class SendspinControls {
  START,
  STOP,
  CLEAR,
  MUTE_UPDATE,
  VOLUME_UPDATE,
};

/// @brief Persistent storage structure for last played server hash.
struct LastPlayedServerPref {
  uint32_t server_id_hash;  ///< FNV1 hash of the server ID that most recently had playback_state: 'playing'
};

/// @brief Persistent storage structure for static delay.
struct StaticDelayPref {
  uint16_t static_delay_ms;
};

class SendspinHub : public Component {
  /* Basic component design: This hub component handles creating the websocket server and interacting with a sendspin
   * server once it connects via websockets. It interacts with the Sendspin server directly over websockets.
   *  - sendspin_protocol.h handles the specific Sendspin protocol messages
   *  - sendspin_time_filter.h uses a Kalman filter to accurately convert the client timestamp into a server timestamp
   *    or vice versa
   *
   * The hub sends appropriate data to child Sendspin components using callbacks.
   *  - Track metadata is sent to text sensors
   *  - Decoded audio is sent to a media player. The media player handles playing that audio in sync.
   *  - Any Sendspin component can use the hub's ``update_sendspin_sensor`` function to update diagnostic sensors.
   *    - the acutal sensor updates are sent via callbacks to the sensor components.
   *  - Internal controls for keeping the child components synchronized use ``controls_callback_``
   *  - TODO: Reevaluate this callback structure, as each particular callback follows a slightly different pattern. It
   *    would be nice if it were more uniform.
   */
 public:
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }
  void setup() override;
  void loop() override;

  void start(SendspinConnection *conn);

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }
  void set_buffer_size(size_t buffer_size) { this->buffer_size_ = buffer_size; }
  size_t get_buffer_size() const { return this->buffer_size_; }

#ifdef USE_SENDSPIN_PLAYER
  void set_initial_static_delay_ms(uint16_t delay_ms) { this->initial_static_delay_ms_ = delay_ms; }
  void set_fixed_delay_us(int32_t delay_us) { this->fixed_delay_us_ = delay_us; }
  int32_t get_fixed_delay_us() const { return this->fixed_delay_us_; }
  void set_static_delay_adjustable(bool adjustable);

  /// @brief Sets the single audio chunk callback.
  /// @param callback Function called with raw audio data for each chunk.
  void set_audio_chunk_callback(std::function<bool(const uint8_t *data, size_t data_size, int64_t timestamp,
                                                   ChunkType chunk_type, TickType_t ticks_to_wait)> &&callback) {
    this->audio_chunk_callback_ = std::move(callback);
  }

  uint8_t get_volume() { return this->volume_; }
  bool get_muted() { return this->muted_; }
  uint16_t get_static_delay_ms() { return this->static_delay_ms_; }
  void update_muted(bool is_muted);
  void update_volume(uint8_t volume);
  void update_static_delay(uint16_t delay_ms);
  void update_state(SendspinClientState state);
  void publish_client_state(SendspinConnection *conn);

  ServerPlayerStreamObject &get_current_stream_params() { return this->current_stream_params_; }
#endif

  void add_controls_callback(std::function<void(const SendspinControls &)> &&callback) {
    this->controls_callbacks_.add(std::move(callback));
  }

  void add_group_update_callback(std::function<void(const GroupUpdateObject &)> &&callback) {
    this->group_update_callbacks_.add(std::move(callback));
  }

  std::string get_group_id() const { return this->group_state_.group_id.value_or(""); }
  std::string get_group_name() const { return this->group_state_.group_name.value_or(""); }

#ifdef USE_SENDSPIN_METADATA
  void add_metadata_callback(std::function<void(const ServerMetadataStateObject &)> &&callback) {
    this->metadata_callbacks_.add(std::move(callback));
  }

  /// @brief Returns the interpolated track progress in milliseconds using the spec formula.
  /// Accounts for playback speed and time elapsed since last server update.
  /// Returns 0 if no progress data available.
  uint32_t get_track_progress_ms();

  /// @brief Returns the track duration in milliseconds. 0 means unknown/live.
  uint32_t get_track_duration_ms();
#endif

#ifdef USE_SENDSPIN_SENSOR
  void add_sensor_callback(std::function<void(const SendspinSensorUpdate &)> &&callback) {
    this->sensor_callbacks_.add(std::move(callback));
  }

  void update_sendspin_sensor(SendspinSensorUpdate sensor_update) { this->sensor_callbacks_.call(sensor_update); }
#endif

#ifdef USE_SENDSPIN_CONTROLLER
  void send_client_command(SendspinControllerCommand command, std::optional<uint8_t> volume = std::nullopt,
                           std::optional<bool> mute = std::nullopt);

  ServerStateControllerObject get_controller_state() { return this->controller_state_; }
#endif

  void disconnect_from_server(SendspinGoodbyeReason reason);

  /// @brief Connects to a Sendspin server at the given URL (client-initiated connection).
  ///
  /// This creates a client connection to the specified server URL. If there's already
  /// an active connection (server-initiated), the handoff protocol is followed:
  /// - The new connection completes its hello handshake
  /// - Based on connection_reason, the hub decides which connection to keep
  /// - The losing connection receives a goodbye with reason 'another_server'
  ///
  /// @param url The WebSocket server URL (e.g., "ws://server.local:8927/sendspin").
  void connect_to_server(const std::string &url);

  /// @brief Converts a server timestamp to the equivalent client timestamp.
  /// @param server_time Server timestamp in microseconds.
  /// @return Equivalent client timestamp in microseconds (0 if no active connection).
  int64_t get_client_time(int64_t server_time) {
    if (this->current_connection_ == nullptr) {
      return 0;
    }
    return this->current_connection_->get_client_time(server_time);
  }

  /// @brief Returns true if the time filter has received at least one measurement.
  /// @return True if time synchronization has started, false if no connection or no measurements yet.
  bool is_time_synced() const {
    if (this->current_connection_ == nullptr) {
      return false;
    }
    return this->current_connection_->is_time_synced();
  }

  /// @brief Gets the current active connection (if any).
  /// @return Pointer to the current connection, or nullptr if none.
  SendspinConnection *get_current_connection() const { return this->current_connection_.get(); }

#ifdef USE_SENDSPIN_ARTWORK
  void add_image_slot_callback(uint8_t slot,
                               std::function<void(const uint8_t *, size_t, SendspinImageFormat, int64_t)> &&callback) {
    // Linear search for existing slot
    for (auto &entry : this->image_slot_callbacks_) {
      if (entry.slot == slot) {
        entry.callbacks.add(std::move(callback));
        return;
      }
    }
    // Create new entry for this slot
    this->image_slot_callbacks_.push_back({slot, {}});
    this->image_slot_callbacks_.back().callbacks.add(std::move(callback));
  }
  void add_image_preferred_format(const ImageSlotPreference &preference) {
    this->preferred_image_formats_.push_back(preference);
  }
  const std::vector<ImageSlotPreference> &get_image_preferred_formats() const { return this->preferred_image_formats_; }
#endif

 protected:
  /// @brief Schedules hello message sending with exponential backoff.
  /// @param conn The connection to send the hello message to.
  /// @param delay_ms Delay before the next attempt in milliseconds.
  /// @param attempts_remaining Number of attempts remaining.
  void try_send_hello_(SendspinConnection *conn, uint32_t delay_ms, uint8_t attempts_remaining);

  /// @brief Attempt to send the hello message.
  /// @param remaining_attempts Number of retry attempts remaining.
  /// @param conn The connection to send the hello message to.
  /// @return true if done (success or non-recoverable), false if should retry.
  bool send_hello_message_(uint8_t remaining_attempts, SendspinConnection *conn);

#ifdef USE_SENDSPIN_PLAYER
  bool send_audio_chunk_(const uint8_t *data, size_t data_size, int64_t timestamp, ChunkType chunk_type,
                         TickType_t ticks_to_wait);

  std::function<bool(const uint8_t *data, size_t data_size, int64_t timestamp, ChunkType chunk_type,
                     TickType_t ticks_to_wait)>
      audio_chunk_callback_;

  uint8_t volume_;
  bool muted_;
  uint16_t static_delay_ms_{0};
  uint16_t initial_static_delay_ms_{0};
  int32_t fixed_delay_us_{0};
  bool static_delay_adjustable_{false};
  SendspinClientState state_{SendspinClientState::SYNCHRONIZED};
  ServerPlayerStreamObject current_stream_params_{};

  /// @brief Preference storage for static delay.
  ESPPreferenceObject static_delay_pref_;

  /// @brief Loads the static delay from persistent storage.
  void load_static_delay_();

  /// @brief Persists the static delay to storage.
  void persist_static_delay_();
#endif

  SendspinTimeBurst time_burst_;

  /// @brief Processes a sendspin binary message (payload owned by connection, read-only).
  void process_binary_message_(uint8_t *payload, size_t len);

  // Process JSON message
  // Returns true if message was successfully processed, false otherwise
  bool process_json_message_(SendspinConnection *conn, const std::string &message, int64_t timestamp);

  // --- Connection management ---

  /// @brief Called when a new connection is accepted by the WebSocket server.
  /// @param conn The new connection (ownership transferred to hub).
  void on_new_connection_(std::unique_ptr<SendspinServerConnection> conn);

  /// @brief Called when a connection completes its hello handshake.
  /// @param conn Pointer to the connection that completed its handshake.
  void on_connection_handshake_complete_(SendspinConnection *conn);

  /// @brief Called when a server connection's socket is closed by httpd.
  /// Resolves the sockfd to a connection pointer and delegates to on_connection_lost_().
  /// @param sockfd The socket file descriptor of the closed connection.
  void on_connection_closed_(int sockfd);

  /// @brief Handles a connection being lost (closed or disconnected).
  /// If current: cleans up playback state, resets, and promotes pending if available.
  /// If pending: just resets (no cleanup needed since it never streamed).
  /// Used by both server connections (via on_connection_closed_) and client connections (via on_disconnected).
  /// @param conn Pointer to the connection that was lost.
  void on_connection_lost_(SendspinConnection *conn);

  /// @brief Determines whether to switch from current connection to a new one during handoff.
  /// @param current Pointer to the current active connection.
  /// @param new_conn Pointer to the new pending connection.
  /// @return true if should switch to new connection, false to keep current.
  bool should_switch_to_new_server_(SendspinConnection *current, SendspinConnection *new_conn);

  /// @brief Completes the handoff process by disconnecting the loser.
  /// @param switch_to_new true to promote pending to current, false to keep current.
  void complete_handoff_(bool switch_to_new);

  /// @brief Disconnects a connection with a goodbye reason, keeping it alive through the async send.
  /// Converts the unique_ptr to a shared_ptr captured in the goodbye completion lambda,
  /// so the connection object survives until the async goodbye send finishes.
  /// @param conn The connection to disconnect (ownership transferred).
  /// @param reason The goodbye reason to send.
  void disconnect_and_release_(std::unique_ptr<SendspinConnection> conn, SendspinGoodbyeReason reason);

  /// @brief Cleans up hub-level playback state (stops audio, releases WiFi).
  /// Only call when the active streaming connection is being removed.
  void cleanup_connection_state_();

  // --- Persistence ---

  /// @brief Loads the last played server hash from persistent storage.
  void load_last_played_server_();

  /// @brief Hashes and persists the server ID as the last played server.
  /// @param server_id The server ID to hash and persist.
  void persist_last_played_server_(const std::string &server_id);

  // --- Connection state ---

  /// @brief The current active connection (only one at a time per sendspin spec).
  std::unique_ptr<SendspinConnection> current_connection_;

  /// @brief Pending connection during handoff (temporary, until decision is made).
  std::unique_ptr<SendspinConnection> pending_connection_;

  /// @brief Connection being gracefully disconnected (kept alive until goodbye completes).
  /// Remains findable by sockfd so httpd can route messages during the async goodbye send.
  std::shared_ptr<SendspinConnection> dying_connection_;

  /// @brief WebSocket server listener (server mode only).
  std::unique_ptr<SendspinWsServer> ws_server_;

  /// @brief Connection mode (server or client).
  // --- Handoff state ---

  /// @brief FNV1 hash of the last server ID that had playback_state: 'playing'.
  uint32_t last_played_server_hash_{0};

  /// @brief Whether we have a persisted last played server.
  bool has_last_played_server_{false};

  /// @brief Preference storage for last played server.
  ESPPreferenceObject last_played_server_pref_;

  // --- Configuration state ---

  bool task_stack_in_psram_{false};
  size_t buffer_size_{1000000};

  bool force_publish_state_{false};

#ifdef USE_WIFI
  bool high_performance_networking_requested_for_time_{false};
  bool high_performance_networking_requested_for_playback_{false};
#endif

  ServerInformationObject server_information_{};
  GroupUpdateObject group_state_{};

  CallbackManager<void(const SendspinControls &)> controls_callbacks_{};
  CallbackManager<void(const GroupUpdateObject &)> group_update_callbacks_{};

#ifdef USE_SENDSPIN_ARTWORK
  std::vector<ImageSlotCallback> image_slot_callbacks_;
  std::vector<ImageSlotPreference> preferred_image_formats_;
#endif

#ifdef USE_SENDSPIN_CONTROLLER
  ServerStateControllerObject controller_state_{};
#endif

#ifdef USE_SENDSPIN_METADATA
  CallbackManager<void(const ServerMetadataStateObject &)> metadata_callbacks_{};
  ServerMetadataStateObject metadata_;
#endif

#ifdef USE_SENDSPIN_SENSOR
  CallbackManager<void(const SendspinSensorUpdate &)> sensor_callbacks_{};
#endif
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
