#include "sendspin_hub.h"

#ifdef USE_ESP_IDF

#include "esphome/components/network/util.h"
#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include <esp_log.h>

#include <cstring>
#include <memory>

namespace esphome::sendspin_ {

ESPHOME_LOG_TAG(TAG, "sendspin.hub");

namespace {

// The reason passed to on_pairing_failed automations, in the protocol's own spelling.
StringRef pair_abort_reason_to_string(sendspin::SendspinPairAbortReason reason) {
  using sendspin::SendspinPairAbortReason;
  switch (reason) {
    case SendspinPairAbortReason::ATTEMPT_TIMEOUT:
      return StringRef::from_lit("attempt_timeout");
    case SendspinPairAbortReason::CONCURRENT_ATTEMPT:
      return StringRef::from_lit("concurrent_attempt");
    case SendspinPairAbortReason::METHOD_NOT_SUPPORTED:
      return StringRef::from_lit("method_not_supported");
    case SendspinPairAbortReason::PAIRING_CODE_MISMATCH:
      return StringRef::from_lit("pairing_code_mismatch");
    case SendspinPairAbortReason::USER_CANCELLED:
      return StringRef::from_lit("user_cancelled");
    case SendspinPairAbortReason::UNKNOWN:
      break;
  }
  return StringRef::from_lit("unknown");
}

// Zeroes through a volatile pointer so dead-store elimination cannot drop it.
void secure_wipe(void *data, size_t len) {
  volatile auto *p = static_cast<volatile uint8_t *>(data);
  for (size_t i = 0; i < len; ++i) {
    p[i] = 0;
  }
}

// Call once per key, from setup().
ESPPreferenceObject make_blob_pref(const std::string &name, size_t size) {
  return global_preferences->make_preference(size, fnv1a_hash("sendspin_" + name));
}

// Loads straight into the returned buffer, which the library wipes after use. A failed load can
// leave part of a stored private key or PSK behind, so that path wipes it here. This only covers
// this buffer: the preference backend keeps its own copy of a pending write until it syncs.
std::optional<std::vector<uint8_t>> read_blob(ESPPreferenceObject &pref, size_t size) {
  std::vector<uint8_t> out(size);
  if (!pref.load(out.data(), out.size())) {
    secure_wipe(out.data(), out.size());
    return std::nullopt;
  }
  return out;
}

// The library always writes a key's fixed size; any other length is misuse and is not stored.
bool write_blob(ESPPreferenceObject &pref, const char *key, size_t size, const uint8_t *data, size_t len) {
  if (len != size) {
    ESP_LOGW(TAG, "\"%s\" blob of %zu bytes does not match its size of %zu; rejecting write", key, len, size);
    return false;
  }
  if (!pref.save(data, len)) {
    ESP_LOGW(TAG, "Failed to persist \"%s\" blob (%zu bytes)", key, len);
    return false;
  }
  return true;
}

// "rec_<n>" to n; nullopt for any other key or a slot with no storage here.
std::optional<size_t> parse_record_slot(const std::string &key) {
  const char *const prefix = sendspin::persistence_keys::RECORD_SLOT_PREFIX;
  const size_t prefix_len = std::strlen(prefix);
  if (key.size() <= prefix_len || key.compare(0, prefix_len, prefix) != 0) {
    return std::nullopt;
  }
  size_t slot = 0;
  for (size_t i = prefix_len; i < key.size(); i++) {
    const char c = key[i];
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    slot = slot * 10 + static_cast<size_t>(c - '0');
    if (slot >= SENDSPIN_RECORD_SLOTS) {
      return std::nullopt;
    }
  }
  return slot;
}

}  // namespace

#ifdef USE_SENDSPIN_ARTWORK
// Indexed by the library enums, which start at zero and are contiguous.
static const char *const IMAGE_SOURCE_NAMES[] = {"ALBUM", "ARTIST", "NONE"};
static const char *const IMAGE_FORMAT_NAMES[] = {"JPEG", "PNG"};
#endif

SendspinHub *global_sendspin_hub = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

SendspinHub::SendspinHub() { global_sendspin_hub = this; }

void SendspinHub::setup() {
  auto config = this->build_client_config_();
  this->client_ = std::make_unique<sendspin::SendspinClient>(std::move(config));

  // Set up persistence (preferences must be initialized before providers are added to the client). These key names
  // are frozen; see PREFERENCE KEYS in sendspin_hub.h.
  namespace keys = sendspin::persistence_keys;
  this->keypair_pref_ = make_blob_pref("keypair", keys::KEYPAIR_SIZE);
  this->pairing_psk_pref_ = make_blob_pref("pair_psk", keys::PAIRING_PSK_SIZE);
  this->last_played_pref_ = make_blob_pref("last_played", keys::LAST_PLAYED_SIZE);
  // Released firmware's name, so a calibrated delay survives.
  this->output_delay_pref_ = make_blob_pref("static_delay", keys::OUTPUT_DELAY_SIZE);
  for (size_t slot = 0; slot < SENDSPIN_RECORD_SLOTS; slot++) {
    this->record_slot_prefs_[slot] = make_blob_pref(keys::record_slot_key(slot), keys::RECORD_SLOT_SIZE);
  }
  this->record_order_pref_ = make_blob_pref(keys::RECORD_ORDER, SENDSPIN_RECORD_SLOTS);

  // Wire providers and client listener
  this->client_->set_listener(this);
  this->client_->set_network_provider(this);
  this->client_->set_persistence_provider(this);

#ifdef USE_SENDSPIN_ARTWORK
  this->artwork_role_ = &this->client_->add_artwork(this->artwork_config_);
  this->artwork_role_->set_listener(this);
#endif

#ifdef USE_SENDSPIN_CONTROLLER
  this->controller_role_ = &this->client_->add_controller();
  this->controller_role_->set_listener(this);
#endif

#ifdef USE_SENDSPIN_METADATA
  this->metadata_role_ = &this->client_->add_metadata();
  this->metadata_role_->set_listener(this);
#endif

#ifdef USE_SENDSPIN_PLAYER
  this->client_->add_player(this->player_config_).set_listener(this->player_listener_);
#endif

  // Set before setup() by codegen; an unpaired access switch sets it later, from its own setup().
  if (this->unpaired_access_.has_value()) {
    this->client_->set_unpaired_access_enabled(*this->unpaired_access_);
  }
}

void SendspinHub::loop() {
  if (this->enabled_.has_value() && this->unpaired_access_.has_value() &&
      this->enabled_.value() != this->client_->is_started() && !this->status_has_error()) {
    if (!this->enabled_.value()) {
      this->client_->stop();
    } else if (!this->client_->start()) {
      this->status_set_error(LOG_STR("Failed to start Sendspin client"));
    }
  }
  this->client_->loop();

#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
  this->update_mdns_service_();
#endif
}

// Sends each server a goodbye and writes what the client still owed its provider. Wi-Fi sets up
// before this hub, so it shuts down after it and the goodbyes can still go out.
void SendspinHub::on_shutdown() {
  // Keeps loop() from starting the client again.
  this->enabled_ = false;
  if (this->is_client_running()) {
    this->client_->stop();
  }
}

void SendspinHub::dump_config() {
  // client_id exists only once start() has run.
  const char *client_id = "(unavailable)";
  if (this->client_ != nullptr && !this->client_->client_id().empty()) {
    client_id = this->client_->client_id().c_str();
  }
  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGCONFIG(TAG,
                "Sendspin Hub:\n"
                "  Client ID: %s\n"
                "  MAC address: %s\n"
                "  Manufacturer: %s\n"
                "  Model: %s\n"
                "  Firmware version: %s\n"
                "  Task stack in PSRAM: %s\n"
                "  Unpaired access: %s\n"
                "  Pairing code method: %s",
                client_id, get_mac_address_into_buffer(mac_buf), this->manufacturer_, this->get_product_name_(),
                this->firmware_version_, YESNO(this->task_stack_in_psram_),
                YESNO(this->client_ != nullptr && this->client_->is_unpaired_access_enabled()),
                this->pairing_code_method_());

#ifdef USE_SENDSPIN_ARTWORK
  // Slot indices come from the order the image platform entries were declared, so the log is the
  // only place the mapping from a slot to the artwork it asked for can be read back.
  uint8_t slot = 0;
  for (const auto &preference : this->artwork_config_.preferred_formats) {
    ESP_LOGCONFIG(TAG, "  Artwork slot %u: %s as %s, %ux%u, display offset %" PRId32 " ms", slot++,
                  IMAGE_SOURCE_NAMES[static_cast<uint8_t>(preference.source)],
                  IMAGE_FORMAT_NAMES[static_cast<uint8_t>(preference.format)], preference.width, preference.height,
                  preference.display_offset_ms);
  }
#endif
}

// THREAD CONTEXT: Main loop (invoked from codegen before setup(), or from Sendspin components)
void SendspinHub::set_enabled(bool enabled) {
  if (this->status_has_error()) {
    ESP_LOGE(TAG, "Cannot %s: Sendspin failed to start, reboot to retry",
             enabled ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"));
    return;
  }
  this->enabled_ = enabled;
}

// THREAD CONTEXT: Main loop
std::optional<std::string> SendspinHub::get_pairing_token() const {
  if (this->client_ == nullptr) {
    return std::nullopt;
  }
  return this->client_->pairing_token();
}

// THREAD CONTEXT: Main loop (invoked from codegen before setup(), or from Sendspin components)
void SendspinHub::set_unpaired_access_enabled(bool enabled) {
  this->unpaired_access_ = enabled;
  // Before setup() there is no client yet; setup() applies the stored value.
  if (this->client_ != nullptr) {
    this->client_->set_unpaired_access_enabled(enabled);
  }
}

#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
// THREAD CONTEXT: Main loop
void SendspinHub::update_mdns_service_() {
  // Synced from loop() because mdns sets up after this hub and only builds its service list then.
  if (!this->mdns_->is_ready()) {
    return;
  }
  bool advertise = this->client_->is_started();
  if (advertise == this->mdns_advertised_) {
    return;
  }
  // One attempt per change
  this->mdns_advertised_ = advertise;
  if (!this->mdns_->set_service_enabled("_sendspin", "_tcp", advertise)) {
    ESP_LOGE(TAG, "Failed to %s mDNS service", advertise ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"));
  }
}
#endif

// --- Delegating methods ---

// THREAD CONTEXT: Main loop (invoked from Sendspin components)
void SendspinHub::connect_to_server(const std::string &url) {
  if (this->is_client_running()) {
    this->client_->connect_to(url);
  }
}

// THREAD CONTEXT: Main loop (invoked from Sendspin components)
void SendspinHub::disconnect_from_server(sendspin::SendspinGoodbyeReason reason) {
  if (this->is_client_running()) {
    this->client_->disconnect(reason);
  }
}

// THREAD CONTEXT: Main loop (invoked from Sendspin components)
void SendspinHub::leave_group() {
  if (this->is_client_running() &&
      this->client_->get_group_state().playback_state == sendspin::SendspinPlaybackState::PLAYING) {
    this->client_->leave();
  }
}

// THREAD CONTEXT: Main loop (invoked from the sendspin.confirm_pairing_window action)
void SendspinHub::confirm_pairing_window() {
  if (this->is_client_running()) {
    this->client_->confirm_pairing_window();
  }
}

// THREAD CONTEXT: Main loop (invoked from the sendspin.cancel_pairing_window action)
void SendspinHub::cancel_pairing_window() {
  if (this->is_client_running()) {
    this->client_->cancel_pairing_window();
  }
}

const char *SendspinHub::get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf) {
  // The server matches this MAC against the L2 source MAC of the device's multicast traffic.
  // ESP-IDF derives the ethernet MAC as base+3 by default on ESP32-S3, so we cannot use the
  // eFuse base MAC when ethernet is the active interface.
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr) {
    ethernet::global_eth_component->get_eth_mac_address_pretty_into_buffer(buf);
  } else {
    get_mac_address_pretty_into_buffer(buf);
  }
#else
  get_mac_address_pretty_into_buffer(buf);
#endif
  // The pretty format is uppercase, but SendspinClientConfig::mac_address must be lowercase.
  for (char &c : buf) {
    if (c >= 'A' && c <= 'F') {
      c += 'a' - 'A';
    }
  }
  return buf.data();
}

// Validation rejects a static code together with the dynamic code, so at most one is set.
const char *SendspinHub::pairing_code_method_() const {
  if (this->pairing_code_display_supported_) {
    return LOG_STR_LITERAL("dynamic");
  }
  if (this->static_pairing_code_ != nullptr) {
    return LOG_STR_LITERAL("static");
  }
  return LOG_STR_LITERAL("none");
}

const char *SendspinHub::get_product_name_() const {
  return this->model_ != nullptr ? this->model_ : App.get_name().c_str();
}

sendspin::SendspinClientConfig SendspinHub::build_client_config_() {
  sendspin::SendspinClientConfig config;

  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  config.mac_address = SendspinHub::get_mac_address_into_buffer(mac_buf);
  config.name = App.get_friendly_name();
  config.product_name = this->get_product_name_();
  config.manufacturer = this->manufacturer_;
  config.software_version = this->firmware_version_;
  config.httpd_psram_stack = this->task_stack_in_psram_;
  config.protocol_task_psram_stack = this->task_stack_in_psram_;
  config.max_pairing_records = SENDSPIN_RECORD_SLOTS;

  // The dynamic code needs a channel and a format. Only digits, since automations get the bare string and could not
  // tell a QR code token apart.
  if (this->pairing_code_display_supported_) {
    config.pairing_code_out_channels = {sendspin::SendspinPairingCodeChannel::DISPLAY};
    config.pairing_code_formats = {sendspin::SendspinPairingCodeFormat::DIGITS};
  }

  if (this->static_pairing_code_ != nullptr) {
    config.static_pairing_code_locations = {"operator"};
    config.static_pairing_code = this->static_pairing_code_;
  }

  return config;
}

// --- SendspinClientListener overrides ---
// THREAD CONTEXT: Main loop (fired from client_->loop())

void SendspinHub::on_group_update(const sendspin::GroupUpdateObject &group) {
  this->group_update_callbacks_.call(group);
}

void SendspinHub::on_request_high_performance() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->request_high_performance();
    wifi::global_wifi_component->request_roaming_suppression();
  }
#endif
}

void SendspinHub::on_release_high_performance() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->release_high_performance();
    wifi::global_wifi_component->release_roaming_suppression();
  }
#endif
}

void SendspinHub::on_open_pairing_window() { this->open_pairing_window_callbacks_.call(); }

void SendspinHub::on_close_pairing_window() { this->close_pairing_window_callbacks_.call(); }

// Only digits are offered, and the library refuses an activation in any other format.
void SendspinHub::on_display_pairing_code(const std::string &code, sendspin::SendspinPairingCodeFormat /*format*/) {
  this->display_pairing_code_callbacks_.call(code);
}

void SendspinHub::on_clear_pairing_code() { this->clear_pairing_code_callbacks_.call(); }

void SendspinHub::on_pairing_succeeded(const std::string &server_id) {
  this->pairing_succeeded_callbacks_.call(server_id);
}

void SendspinHub::on_pairing_failed(const std::string &server_id, sendspin::SendspinPairAbortReason reason) {
  this->pairing_failed_callbacks_.call(server_id, pair_abort_reason_to_string(reason));
}

// --- SendspinNetworkProvider override ---

// THREAD CONTEXT: Main loop (polled by start() and client_->loop())
bool SendspinHub::is_network_ready() { return network::is_connected(); }

// --- SendspinPersistenceProvider overrides ---
// THREAD CONTEXT: Main loop (the library makes every provider call there, including from start() and stop())

std::pair<ESPPreferenceObject *, size_t> SendspinHub::pref_for_key_(const std::string &key) {
  namespace keys = sendspin::persistence_keys;
  if (auto slot = parse_record_slot(key); slot.has_value()) {
    return {&this->record_slot_prefs_[*slot], keys::RECORD_SLOT_SIZE};
  }
  if (key == keys::RECORD_ORDER) {
    return {&this->record_order_pref_, SENDSPIN_RECORD_SLOTS};
  }
  if (key == keys::KEYPAIR) {
    return {&this->keypair_pref_, keys::KEYPAIR_SIZE};
  }
  if (key == keys::PAIRING_PSK) {
    return {&this->pairing_psk_pref_, keys::PAIRING_PSK_SIZE};
  }
  if (key == keys::LAST_PLAYED) {
    return {&this->last_played_pref_, keys::LAST_PLAYED_SIZE};
  }
  if (key == keys::OUTPUT_DELAY) {
    return {&this->output_delay_pref_, keys::OUTPUT_DELAY_SIZE};
  }
  return {nullptr, 0};
}

std::optional<std::vector<uint8_t>> SendspinHub::load_blob(const std::string &key) {
  auto [pref, size] = this->pref_for_key_(key);
  if (pref == nullptr) {
    ESP_LOGW(TAG, "load_blob: unknown key \"%s\"", key.c_str());
    return std::nullopt;
  }
  return read_blob(*pref, size);
}

bool SendspinHub::save_blob(const std::string &key, const uint8_t *data, size_t len) {
  auto [pref, size] = this->pref_for_key_(key);
  if (pref == nullptr) {
    ESP_LOGW(TAG, "save_blob: unknown key \"%s\"", key.c_str());
    return false;
  }
  return write_blob(*pref, key.c_str(), size, data, len);
}

// Writes every component's queued preferences, so a failure may belong to another component.
bool SendspinHub::commit() { return global_preferences->sync(); }

// --- Sendspin role specific methods/overrides ---

#ifdef USE_SENDSPIN_ARTWORK
// THREAD CONTEXT: Dedicated artwork decode thread; downstream callbacks run here too
void SendspinHub::on_image_decode(uint8_t slot, const uint8_t *data, size_t length,
                                  sendspin::SendspinImageFormat format) {
  this->artwork_image_decode_callbacks_.call(slot, data, length, format);
}

// THREAD CONTEXT: Main loop (fired from client_->loop() once the slot's offset-shifted display
// deadline is reached; lateness_ms reports how far past the deadline the display slipped)
void SendspinHub::on_image_display(uint8_t slot, uint32_t lateness_ms) {
  this->artwork_image_display_callbacks_.call(slot, lateness_ms);
}

// THREAD CONTEXT: Main loop (fired from client_->loop())
void SendspinHub::on_image_clear(uint8_t slot) { this->artwork_image_clear_callbacks_.call(slot); }

// THREAD CONTEXT: Main loop (invoked from SendspinImageSlot once a delivery is fully presented)
void SendspinHub::artwork_frame_done(uint8_t slot) {
  if (this->artwork_role_ != nullptr) {
    this->artwork_role_->frame_done(slot);
  }
}
#endif

#ifdef USE_SENDSPIN_CONTROLLER
// THREAD CONTEXT: Main loop (invoked from ESPHome actions / other components)
void SendspinHub::send_client_command(sendspin::SendspinControllerCommand command, std::optional<uint8_t> volume,
                                      std::optional<bool> mute) {
  if (this->is_client_running()) {
    sendspin::ClientCommandControllerObject obj = {
        .command = command,
        .volume = volume,
        .muted = mute,
    };
    this->controller_role_->send_command(obj);
  }
}

// THREAD CONTEXT: Main loop (invoked from the sendspin.switch action)
void SendspinHub::switch_client() { this->send_client_command(sendspin::SendspinControllerCommand::SWITCH); }

// THREAD CONTEXT: Main loop (ControllerRoleListener override, fired from client_->loop())
void SendspinHub::on_controller_state(const sendspin::ServerStateControllerObject &state) {
  this->controller_state_callbacks_.call(state);
}

// THREAD CONTEXT: Main loop (ControllerRoleListener override, fired from client_->loop())
// Unlike metadata, this cannot be fanned out as a default-constructed state object: volume and muted are plain values
// rather than optionals, so children would read a real-looking 0% volume where we mean no value at all. A separate
// callback lets each child clear only what it can represent.
void SendspinHub::on_controller_state_clear() { this->controller_state_clear_callbacks_.call(); }
#endif

#ifdef USE_SENDSPIN_METADATA
// THREAD CONTEXT: Main loop (MetadataRoleListener override, fired from client_->loop())
void SendspinHub::on_metadata(const sendspin::ServerMetadataStateObject &metadata) {
  this->metadata_update_callbacks_.call(metadata);
}

// THREAD CONTEXT: Main loop (MetadataRoleListener override, fired from client_->loop())
// The cached metadata was dropped because the connection to the server was lost, so what the children now mirror is
// the empty state. Fanning that out as a default-constructed state object rather than through a separate callback
// keeps one code path in the children: every field is nullopt, which they already publish as empty/unknown.
void SendspinHub::on_metadata_clear() { this->metadata_update_callbacks_.call(sendspin::ServerMetadataStateObject{}); }

// THREAD CONTEXT: Main loop (invoked from Sendspin components)
uint32_t SendspinHub::get_track_progress_ms() const {
  if (this->is_ready()) {
    return this->metadata_role_->get_track_progress_ms();
  }
  return 0;
}
#endif

#ifdef USE_SENDSPIN_PLAYER
// THREAD CONTEXT: Main loop, called from child component setup() after player role is created and configured
sendspin::PlayerRole *SendspinHub::get_player_role() {
  if (this->is_ready()) {
    return this->client_->player();
  }
  return nullptr;
}
#endif

}  // namespace esphome::sendspin_

#endif  // USE_ESP_IDF
