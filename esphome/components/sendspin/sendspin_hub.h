#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/version.h"

#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
#include "esphome/components/mdns/mdns_component.h"
#endif

#include <sendspin/client.h>
#include <sendspin/config.h>
#include <sendspin/types.h>

#ifdef USE_SENDSPIN_ARTWORK
#include <sendspin/artwork_role.h>
#endif
#ifdef USE_SENDSPIN_CONTROLLER
#include <sendspin/controller_role.h>
#endif
#ifdef USE_SENDSPIN_METADATA
#include <sendspin/metadata_role.h>
#endif
#ifdef USE_SENDSPIN_PLAYER
#include <sendspin/player_role.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace esphome::sendspin_ {

/// @brief Setup priorities for the sendspin hub and its child components.
///
/// Centralized here so every sendspin component orders itself relative to the hub
/// without each subcomponent having to pick a priority independently. Children run
/// one step later than hub so they can assume hub's setup() has already completed.
namespace sendspin_priority {
// AFTER_WIFI so the hub runs after the wifi/ethernet drivers are up and we can read the active
// interface's MAC address for the device info.
inline constexpr float HUB = esphome::setup_priority::AFTER_WIFI;
inline constexpr float CHILD = HUB - 1.0f;
}  // namespace sendspin_priority

// PREFERENCE KEYS: each library persistence key has its own preference, hashed from "sendspin_" + name. Renaming
// one erases it.

/// Pairing-record slots, handed to the client as max_pairing_records.
inline constexpr size_t SENDSPIN_RECORD_SLOTS = sendspin::SendspinClientConfig::DEFAULT_MAX_PAIRING_RECORDS;

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
#ifdef USE_SENDSPIN_ARTWORK
                          public sendspin::ArtworkRoleListener,
#endif
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
  void on_shutdown() override;
  void dump_config() override;

  /// @brief Connects the underlying client to the given Sendspin server.
  ///
  /// No-op if the hub's client is not running (see is_client_running()).
  /// Must be called from the main loop thread.
  /// @param url WebSocket URL of the Sendspin server, starting with `ws://` (e.g. `ws://host:port/path`).
  void connect_to_server(const std::string &url);

  /// @brief Disconnects the underlying client from the current server.
  ///
  /// Sends a `client/goodbye` message with the given reason before closing the connection.
  /// No-op if the hub's client is not running. Must be called from the main loop thread.
  /// @param reason Reason reported to the server:
  ///   - `ANOTHER_SERVER`: client is switching to another server.
  ///   - `SHUTDOWN`: client is shutting down.
  ///   - `RESTART`: client is restarting.
  ///   - `USER_REQUEST`: user explicitly requested disconnect.
  void disconnect_from_server(sendspin::SendspinGoodbyeReason reason);

  /// @brief Leaves the current group so another source can use the speaker.
  ///
  /// Sends `client/leave` while the group is playing. A stopped group is left alone, so the device stays grouped.
  /// No-op if the hub's client is not running. Must be called from the main loop thread.
  void leave_group();

  /// @brief Confirms a pairing attempt on the device. With no attempt waiting, it opens the pairing window for the
  /// next one.
  ///
  /// No-op if the hub's client is not running. Must be called from the main loop thread.
  void confirm_pairing_window();

  /// @brief Closes an open pairing window, so a waiting pairing attempt is not confirmed.
  ///
  /// No-op if the hub's client is not running. Must be called from the main loop thread.
  void cancel_pairing_window();

  // --- Configuration setters (called from codegen) ---

  template<typename F> void add_group_update_callback(F &&callback) {
    this->group_update_callbacks_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_open_pairing_window_callback(F &&callback) {
    this->open_pairing_window_callbacks_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_close_pairing_window_callback(F &&callback) {
    this->close_pairing_window_callbacks_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_pairing_succeeded_callback(F &&callback) {
    this->pairing_succeeded_callbacks_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_pairing_failed_callback(F &&callback) {
    this->pairing_failed_callbacks_.add(std::forward<F>(callback));
  }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  /// @brief Requests the Sendspin client, including the server, the roles and the mDNS advertisement, to start or
  /// stop.
  ///
  /// Applied from the hub's loop(). Stopping blocks until the client is fully stopped; the roles' clear callbacks
  /// fire from inside that call. With a sendspin switch configured the client stays stopped until the switch has
  /// called this once. Must be called from the main loop thread.
  void set_enabled(bool enabled);

  /// Turns unpaired (Sentinel) access on or off from setup(); see SendspinClient::set_unpaired_access_enabled().
  void set_default_unpaired_access(bool enabled) { this->default_unpaired_access_ = enabled; }

  /// @brief Returns whether the Sendspin client is running.
  bool is_client_running() const { return this->client_ != nullptr && this->client_->is_started(); }

  /// @brief Sets the device information reported to the server in the `client/hello` message.
  ///
  /// Each takes a pointer to a string literal emitted by codegen, so it must stay valid for the
  /// lifetime of the hub. Only called for values the configuration overrides; anything left alone
  /// keeps the default described on the member below.
  void set_manufacturer(const char *manufacturer) { this->manufacturer_ = manufacturer; }
  void set_model(const char *model) { this->model_ = model; }
  void set_firmware_version(const char *firmware_version) { this->firmware_version_ = firmware_version; }

#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
  void set_mdns(mdns::MDNSComponent *mdns) { this->mdns_ = mdns; }
#endif

  /// The static pairing code, used on every boot.
  void set_static_pairing_code(const char *code) { this->static_pairing_code_ = code; }

  // --- Sendspin role specific methods ---

#ifdef USE_SENDSPIN_ARTWORK
  void set_artwork_config(const sendspin::ArtworkRoleConfig &config) { this->artwork_config_ = config; }

  /// @brief Acknowledges the most recent artwork delivery (display or clear) for a slot.
  ///
  /// Every slot is configured with the library's require_frame_done gate, which withholds the
  /// next delivery for the slot until this is called. Exactly one ack is owed per delivery; a
  /// redundant call is a safe no-op in the library. Must be called from the main loop thread.
  void artwork_frame_done(uint8_t slot);

  template<typename F> void add_image_decode_callback(F &&callback) {
    this->artwork_image_decode_callbacks_.add(std::forward<F>(callback));
  }
  template<typename F> void add_image_display_callback(F &&callback) {
    this->artwork_image_display_callbacks_.add(std::forward<F>(callback));
  }
  template<typename F> void add_image_clear_callback(F &&callback) {
    this->artwork_image_clear_callbacks_.add(std::forward<F>(callback));
  }
#endif

#ifdef USE_SENDSPIN_CONTROLLER
  void send_client_command(sendspin::SendspinControllerCommand command, std::optional<uint8_t> volume = std::nullopt,
                           std::optional<bool> mute = std::nullopt);

  /// @brief Sends the SWITCH controller command; exposed as the sendspin.switch action.
  void switch_client();

  template<typename F> void add_controller_state_callback(F &&callback) {
    this->controller_state_callbacks_.add(std::forward<F>(callback));
  }

  /// @brief Registers a callback that fires when the connection is lost and the cached controller state is dropped.
  template<typename F> void add_controller_state_clear_callback(F &&callback) {
    this->controller_state_clear_callbacks_.add(std::forward<F>(callback));
  }
#endif

#ifdef USE_SENDSPIN_METADATA
  /// @brief Registers a callback that fires when the server sends metadata.
  ///
  /// Also fires when the connection is lost, with an all-empty state object (every field nullopt, timestamp 0) meaning
  /// the cached metadata was dropped. Subscribers must treat an absent field as cleared, not as no update.
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

  /// The preference a library key is stored in and its blob size, or {nullptr, 0} for a key with
  /// no storage here.
  std::pair<ESPPreferenceObject *, size_t> pref_for_key_(const std::string &key);

  /// @brief Returns the product name reported to the server: the configured model, or the device name.
  const char *get_product_name_() const;

  /// @brief Writes the active network interface's MAC, in lowercase, into @p buf and returns its data pointer.
  /// Uses the ethernet MAC if ethernet is configured, otherwise the base MAC (used by wifi).
  static const char *get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf);

#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
  /// @brief Keeps the `_sendspin` mDNS service advertised while the client is running.
  void update_mdns_service_();
#endif

  // --- SendspinClientListener overrides ---
  void on_group_update(const sendspin::GroupUpdateObject &group) override;

  void on_request_high_performance() override;

  void on_release_high_performance() override;

  void on_open_pairing_window() override;

  void on_close_pairing_window() override;

  void on_pairing_succeeded(const std::string &server_id) override;

  void on_pairing_failed(const std::string &server_id, sendspin::SendspinPairAbortReason reason) override;

  // --- SendspinNetworkProvider override ---
  bool is_network_ready() override;

  // --- SendspinPersistenceProvider overrides ---
  // The library calls commit() after writing pairing secrets, so they reach flash right away.
  std::optional<std::vector<uint8_t>> load_blob(const std::string &key) override;
  bool save_blob(const std::string &key, const uint8_t *data, size_t len) override;
  bool commit() override;

  // --- Sendspin role specific methods/overrides/member variables ---

#ifdef USE_SENDSPIN_ARTWORK
  void on_image_decode(uint8_t slot, const uint8_t *data, size_t length, sendspin::SendspinImageFormat format) override;

  void on_image_display(uint8_t slot, uint32_t lateness_ms) override;

  void on_image_clear(uint8_t slot) override;

  sendspin::ArtworkRoleConfig artwork_config_{};
  sendspin::ArtworkRole *artwork_role_{nullptr};

  // Callback fan-out to child components; they filter by slot as needed.
  CallbackManager<void(uint8_t, const uint8_t *, size_t, sendspin::SendspinImageFormat)>
      artwork_image_decode_callbacks_{};
  CallbackManager<void(uint8_t, uint32_t)> artwork_image_display_callbacks_{};
  CallbackManager<void(uint8_t)> artwork_image_clear_callbacks_{};
#endif

#ifdef USE_SENDSPIN_CONTROLLER
  sendspin::ControllerRole *controller_role_{nullptr};

  void on_controller_state(const sendspin::ServerStateControllerObject &state) override;

  void on_controller_state_clear() override;

  // Callback fan-out to child components; they filter as needed. Only a media_player subscribes, while the switch
  // action and the media source enable the controller role without one, so keep the idle cost to a single pointer.
  LazyCallbackManager<void(const sendspin::ServerStateControllerObject &)> controller_state_callbacks_{};
  LazyCallbackManager<void()> controller_state_clear_callbacks_{};
#endif

#ifdef USE_SENDSPIN_METADATA
  sendspin::MetadataRole *metadata_role_{nullptr};

  void on_metadata(const sendspin::ServerMetadataStateObject &metadata) override;

  void on_metadata_clear() override;

  // Callback fan-out to child components; they filter as needed
  CallbackManager<void(const sendspin::ServerMetadataStateObject &)> metadata_update_callbacks_{};
#endif

#ifdef USE_SENDSPIN_PLAYER
  sendspin::PlayerRoleListener *player_listener_{nullptr};
  sendspin::PlayerRoleConfig player_config_{};
#endif

  // --- Core member variables ---

  // Built once in setup(): make_preference() allocates a backend that is never freed.
  ESPPreferenceObject keypair_pref_;
  ESPPreferenceObject pairing_psk_pref_;
  ESPPreferenceObject last_played_pref_;
  ESPPreferenceObject output_delay_pref_;
  std::array<ESPPreferenceObject, SENDSPIN_RECORD_SLOTS> record_slot_prefs_;
  ESPPreferenceObject record_order_pref_;

  std::unique_ptr<sendspin::SendspinClient> client_;

  // Callback fan-out to child components
  CallbackManager<void(const sendspin::GroupUpdateObject &)> group_update_callbacks_{};

  // Lazy: each pairing callback is fed by an optional YAML surface.
  LazyCallbackManager<void()> open_pairing_window_callbacks_{};
  LazyCallbackManager<void()> close_pairing_window_callbacks_{};
  LazyCallbackManager<void(const std::string &)> pairing_succeeded_callbacks_{};
  LazyCallbackManager<void(const std::string &, StringRef)> pairing_failed_callbacks_{};

  const char *static_pairing_code_{nullptr};  // Codegen string literal, or nullptr when not configured
  bool default_unpaired_access_{true};
  bool task_stack_in_psram_{false};
#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
  bool mdns_advertised_{false};  // Last state requested from mdns
#endif

  // Requested client state, applied from loop(). Empty until the switch restores its state.
  std::optional<bool> enabled_;

  // Device information sent in the `client/hello` message. Defaults apply when neither the
  // sendspin configuration nor the project information supplies a value.
  const char *manufacturer_{"ESPHome"};
  const char *model_{nullptr};  // nullptr reports the device name instead
  const char *firmware_version_{ESPHOME_VERSION};

#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
  mdns::MDNSComponent *mdns_{nullptr};
#endif
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

#endif  // USE_ESP_IDF
