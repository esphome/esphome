#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include <sendspin/client.h>
#include <sendspin/config.h>
#include <sendspin/types.h>

#ifdef USE_SENDSPIN_CONTROLLER
#include <sendspin/controller_role.h>
#endif
#ifdef USE_SENDSPIN_METADATA
#include <sendspin/metadata_role.h>
#endif
#ifdef USE_SENDSPIN_PLAYER
#include <sendspin/player_role.h>
#endif

#include <functional>
#include <memory>
#include <optional>

namespace esphome::sendspin_ {

/// @brief Setup priorities for the sendspin hub and its child components.
///
/// Centralized here so every sendspin component orders itself relative to the hub
/// without each subcomponent having to pick a priority independently. Children run
/// one step later than hub so they can assume hub's setup() has already completed.
namespace sendspin_priority {
// AFTER_WIFI so the hub runs after the wifi/ethernet drivers are up and we can read the active
// interface's MAC for client_id.
inline constexpr float HUB = esphome::setup_priority::AFTER_WIFI;
inline constexpr float CHILD = HUB - 1.0f;
}  // namespace sendspin_priority

/// @brief Persistent storage structure for last played server hash.
struct LastPlayedServerPref {
  uint32_t server_id_hash;
};

#ifdef USE_SENDSPIN_PLAYER
/// @brief Persistent storage structure for player static delay.
struct StaticDelayPref {
  uint16_t delay_ms;
};
#endif

/// @brief Thin adapter over sendspin::SendspinClient.
///
/// The hub owns a SendspinClient instance and bridges its listener/provider interfaces to ESPHome's CallbackManager for
/// fan-out to child components.
///  - Provides persistence via ESPPreferenceObject and WiFi power management integration.
///  - Handles Sendspin roles that apply to multiple child components (artwork, controller, metadata) so their events
///    can be fanned out. Roles specific to a single component (player) are configured by the hub but owned by the
///    child thereafter, since no fan-out is needed.
///
/// The sendspin-cpp library follows this design:
///  - Core and role configuration are passed at client/role construction time as structs. Built in our `setup()`.
///  - Library -> user code communication happens via two interface types the user implements and registers in our
///    `setup()`: listener interfaces (for events the library pushes; e.g., group updates) and provider interfaces
///    (for services the library pulls; e.g., persistence, network readiness).
///  - User -> library communication uses exposed functions on the client and role objects that the user calls.
class SendspinHub final : public Component,
#ifdef USE_SENDSPIN_CONTROLLER
                          public sendspin::ControllerRoleListener,
#endif
#ifdef USE_SENDSPIN_METADATA
                          public sendspin::MetadataRoleListener,
#endif
                          public sendspin::SendspinClientListener,
                          public sendspin::SendspinNetworkProvider,
                          public sendspin::SendspinPersistenceProvider {
 public:
  float get_setup_priority() const override { return sendspin_priority::HUB; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  /// @brief Connects the underlying client to the given Sendspin server.
  ///
  /// No-op if the hub's client is not ready (e.g. setup() has not completed).
  /// Must be called from the main loop thread.
  /// @param url WebSocket URL of the Sendspin server, starting with `ws://` (e.g. `ws://host:port/path`).
  void connect_to_server(const std::string &url);

  /// @brief Disconnects the underlying client from the current server.
  ///
  /// Sends a `client/goodbye` message with the given reason before closing the connection.
  /// No-op if the hub's client is not ready. Must be called from the main loop thread.
  /// @param reason Reason reported to the server:
  ///   - `ANOTHER_SERVER`: client is switching to another server.
  ///   - `SHUTDOWN`: client is shutting down.
  ///   - `RESTART`: client is restarting.
  ///   - `USER_REQUEST`: user explicitly requested disconnect.
  void disconnect_from_server(sendspin::SendspinGoodbyeReason reason);

  /// @brief Updates the client's reported playback state on the server.
  ///
  /// No-op if the hub's client is not ready. Must be called from the main loop thread.
  /// @param state New client state:
  ///   - `SYNCHRONIZED`: client is synchronized and playing from the server.
  ///   - `ERROR`: client encountered a playback error.
  ///   - `EXTERNAL_SOURCE`: client is playing from a non-Sendspin source.
  void update_state(sendspin::SendspinClientState state);

  // --- Configuration setters (called from codegen) ---

  template<typename F> void add_group_update_callback(F &&callback) {
    this->group_update_callbacks_.add(std::forward<F>(callback));
  }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  // --- Sendspin role specific methods ---

#ifdef USE_SENDSPIN_CONTROLLER
  void send_client_command(sendspin::SendspinControllerCommand command, std::optional<uint8_t> volume = std::nullopt,
                           std::optional<bool> mute = std::nullopt);

  template<typename F> void add_controller_state_callback(F &&callback) {
    this->controller_state_callbacks_.add(std::forward<F>(callback));
  }
#endif

#ifdef USE_SENDSPIN_METADATA
  template<typename F> void add_metadata_update_callback(F &&callback) {
    this->metadata_update_callbacks_.add(std::forward<F>(callback));
  }

  /// @brief Returns the interpolated track progress in milliseconds, or 0 if the hub is not yet ready.
  uint32_t get_track_progress_ms() const;
#endif

#ifdef USE_SENDSPIN_PLAYER
  void set_listener(sendspin::PlayerRoleListener *listener) { this->player_listener_ = listener; }
  void set_player_config(const sendspin::PlayerRoleConfig &config) { this->player_config_ = config; }

  /// @brief Child components call this to get the PlayerRole instance after setup, so they can push updates to it.
  sendspin::PlayerRole *get_player_role();
#endif

 protected:
  /// @brief Builds the SendspinClientConfig from ESPHome configuration and platform info.
  sendspin::SendspinClientConfig build_client_config_();

  /// @brief Writes the active network interface's MAC into @p buf and returns its data pointer.
  /// Uses the ethernet MAC if ethernet is configured, otherwise the base MAC (used by wifi).
  static const char *get_client_id_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf);

  // --- SendspinClientListener overrides ---
  void on_group_update(const sendspin::GroupUpdateObject &group) override;

  void on_request_high_performance() override;

  void on_release_high_performance() override;

  // --- SendspinNetworkProvider override ---
  bool is_network_ready() override;

  // --- SendspinPersistenceProvider overrides ---
  bool save_last_server_hash(uint32_t hash) override;
  std::optional<uint32_t> load_last_server_hash() override;

  // --- Sendspin role specific methods/overrides/member variables ---

#ifdef USE_SENDSPIN_CONTROLLER
  sendspin::ControllerRole *controller_role_{nullptr};

  void on_controller_state(const sendspin::ServerStateControllerObject &state) override;

  // Callback fan-out to child components; they filter as needed
  CallbackManager<void(const sendspin::ServerStateControllerObject &)> controller_state_callbacks_{};
#endif

#ifdef USE_SENDSPIN_METADATA
  sendspin::MetadataRole *metadata_role_{nullptr};

  void on_metadata(const sendspin::ServerMetadataStateObject &metadata) override;

  // Callback fan-out to child components; they filter as needed
  CallbackManager<void(const sendspin::ServerMetadataStateObject &)> metadata_update_callbacks_{};
#endif

#ifdef USE_SENDSPIN_PLAYER
  sendspin::PlayerRoleListener *player_listener_{nullptr};
  sendspin::PlayerRoleConfig player_config_{};

  // Part of SendspinPersistenceProvider overrides
  ESPPreferenceObject static_delay_pref_;
  std::optional<uint16_t> load_static_delay() override;
  bool save_static_delay(uint16_t delay_ms) override;
#endif

  // --- Core member variables ---

  ESPPreferenceObject last_played_server_pref_;

  std::unique_ptr<sendspin::SendspinClient> client_;

  // Callback fan-out to child components
  CallbackManager<void(const sendspin::GroupUpdateObject &)> group_update_callbacks_{};

  bool task_stack_in_psram_{false};
};

/// @brief Base class for all sendspin subcomponents.
///
/// Consolidates the Component + Parented<SendspinHub> inheritance and pins the setup
/// priority so the hub's setup() always runs before any child. Subcomponents should
/// inherit from this instead of listing Component/Parented individually and must not
/// override get_setup_priority().
class SendspinChild : public Component, public Parented<SendspinHub> {
 public:
  float get_setup_priority() const override { return sendspin_priority::CHILD; }
};

/// @brief Base class for sendspin subcomponents that need polling behavior.
///
/// Same purpose as SendspinChild but inherits from PollingComponent for subcomponents
/// that poll on a fixed interval. Subcomponents should inherit from this instead of
/// listing PollingComponent/Parented individually and must not override get_setup_priority().
class SendspinPollingChild : public PollingComponent, public Parented<SendspinHub> {
 public:
  float get_setup_priority() const override { return sendspin_priority::CHILD; }
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP32
