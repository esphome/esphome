#pragma once

// Pull in ESPHome's USE_* macro table BEFORE the ifdef guards below evaluate
// them — every sibling endpoint header does the same. Without this a TU that
// includes matter_component.h before defines.h strips the whole class at
// preprocess time, and worse: the #ifdef USE_* member vectors would silently
// differ across TUs (an ODR violation).
#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_err.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration for defer_attribute_update()'s value parameter. The
// full definition lives in esp_matter/data_model/esp_matter_attribute_utils.h
// (dragged in transitively by matter_component.cpp). Keeping it out of this
// header preserves the "matter_component.h stays independent of the esp_matter
// managed component" invariant that lets clang-tidy strip the whole matter
// TU cleanly on lint jobs where the SDK isn't fetched.
struct esp_matter_attr_val;

namespace esphome::matter {

// RAII guard for the per-endpoint applying_report_ flag. Sets the flag on
// construction, clears it on destruction — so an early return anywhere in
// report_state_to_fabric_ cannot leave the endpoint permanently deaf to
// fabric writes. The underlying flag is std::atomic<bool> because it is
// written on the ESPHome main loop and read by the CHIP PlatformManager
// task via the applying_report() accessor.
class ApplyingReportGuard {
 public:
  explicit ApplyingReportGuard(std::atomic<bool> &flag) : flag_(flag) {
    this->flag_.store(true, std::memory_order_release);
  }
  ~ApplyingReportGuard() { this->flag_.store(false, std::memory_order_release); }
  ApplyingReportGuard(const ApplyingReportGuard &) = delete;
  ApplyingReportGuard &operator=(const ApplyingReportGuard &) = delete;

 private:
  std::atomic<bool> &flag_;
};

#ifdef USE_SWITCH
class MatterSwitchEndpoint;
#endif
#ifdef USE_BINARY_SENSOR
class MatterBinarySensorEndpoint;
#endif
#ifdef USE_SENSOR
class MatterSensorEndpoint;
#endif
#ifdef USE_COVER
class MatterCoverEndpoint;
#endif
#ifdef USE_FAN
class MatterFanEndpoint;
#endif
#ifdef USE_LOCK
class MatterLockEndpoint;
#endif
#ifdef USE_VALVE
class MatterValveEndpoint;
#endif
#ifdef USE_SELECT
class MatterSelectEndpoint;
#endif
#ifdef USE_BUTTON
class MatterButtonEndpoint;
#endif
#ifdef USE_LIGHT
class MatterLightEndpoint;
#endif
#ifdef USE_CLIMATE
class MatterClimateEndpoint;
#endif

class MatterComponent : public Component {
 public:
  MatterComponent(uint16_t vendor_id, uint16_t product_id, std::string setup_code, uint16_t discriminator,
                  bool ble_commissioning, std::string vendor_name, std::string product_name, std::string node_label,
                  std::string hardware_version_string, std::string software_version_string, bool composed_topology);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void factory_reset();

  // Global attribute-write dispatcher — the esp-matter callback set in setup()
  // routes here. Returns true when the write was routed to an entity wrapper,
  // false when no endpoint owns it (endpoint map out of sync with the CHIP
  // data model, or a foreign cluster registered under the same id). The
  // callback maps `false` to a non-OK esp_err_t so CHIP does not silently
  // accept an unroutable write.
  bool handle_attribute_write(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, bool bool_value);

  // Separate dispatcher for uint16 nullable attributes (e.g. WindowCovering's
  // TargetPositionLiftPercent100ths). Returns true on dispatch, false on miss.
  bool handle_cover_target_write(uint16_t endpoint_id, uint16_t percent100ths);

  // Dispatcher for uint8 attribute writes: FanControl.PercentSetting (nullable
  // uint8 0..100) and FanControl.FanMode (enum8). Returns true on dispatch.
  bool handle_fan_percent_write(uint16_t endpoint_id, uint8_t percent);
  bool handle_fan_mode_write(uint16_t endpoint_id, uint8_t fan_mode);

  // Dispatcher for DoorLock LockDoor/UnlockDoor commands. Called from the
  // weak-symbol overrides in matter_lock_endpoint.cpp — not from the
  // attribute_update_cb (DoorLock uses commands, not attribute writes, for
  // remote lock operations). is_lock=true → LockDoor, false → UnlockDoor.
  // Returns true if a wrapper for endpoint_id was found and the command was
  // dispatched.
  bool handle_lock_command(uint16_t endpoint_id, bool is_lock);

  // Dispatcher for ModeSelect.CurrentMode writes. The fabric's ChangeToMode
  // command is decoded by CHIP's mode-select-server which writes CurrentMode
  // via Attributes::CurrentMode::Set — that fires PRE_UPDATE on our
  // attribute_update_cb, which routes here.
  bool handle_select_current_mode_write(uint16_t endpoint_id, uint8_t mode);

  // Light dispatchers — LevelControl.CurrentLevel (dimmable+) and
  // ColorControl.ColorTemperatureMireds (CT+) writes come here after CHIP's
  // command servers finish decoding MoveToLevel / MoveToColorTemperature.
  // OnOff.OnOff writes for light endpoints reuse handle_attribute_write —
  // that method falls through from the switch-endpoint lookup to the
  // light-endpoint lookup so the same OnOff attribute id routes correctly.
  bool handle_light_level_write(uint16_t endpoint_id, uint8_t level);
  bool handle_light_color_temp_write(uint16_t endpoint_id, uint16_t mireds);

  // Climate dispatchers — Thermostat.SystemMode (enum8) writes come here after
  // the fabric changes mode; Occupied{Heating,Cooling}Setpoint (int16
  // hundredths of °C) come here after the fabric moves the slider.
  bool handle_climate_system_mode_write(uint16_t endpoint_id, uint8_t system_mode);
  bool handle_climate_heating_setpoint_write(uint16_t endpoint_id, int16_t hundredths);
  bool handle_climate_cooling_setpoint_write(uint16_t endpoint_id, int16_t hundredths);

  // Singleton accessor for the C-style esp-matter callback to find us.
  // Set in constructor. Only one MatterComponent per binary makes sense.
  static MatterComponent *instance() { return global_matter_component; }

  // Open an Enhanced Commissioning Window so another Matter admin can pair
  // with this already-commissioned device (multi-admin). Generates a fresh
  // passcode + discriminator + spake2p verifier every call — Matter spec
  // §5.5.7.1 deprecates Basic Commissioning after first fabric, so the
  // original setup code from the yaml won't work post-commissioning. The
  // new manual pairing code + QR are logged at INFO level; scan/type them
  // in the second controller's add-device flow.
  //
  // `timeout_seconds` clamped to [180, 900] per spec (§11.19.9.1). Call is
  // marshaled to the CHIP PlatformManager task before touching the
  // CommissioningWindowManager — safe to invoke from any thread (button
  // on_press lambda, WebSocket API call, etc.).
  //
  // Intended usage from yaml:
  //     button:
  //       - platform: template
  //         name: "Open Matter Pairing"
  //         on_press:
  //           - lambda: |-
  //               esphome::matter::MatterComponent::instance()
  //                 ->open_commissioning_window(300);
  void open_commissioning_window(uint32_t timeout_seconds);

  // Called by every endpoint wrapper after it creates its Matter endpoint.
  // Adds a FixedLabel server cluster AND a UserLabel server cluster to
  // `endpoint`, and records ("name", label) for endpoint_id in
  // endpoint_labels_. MatterDeviceInfoProvider reads FixedLabel lazily
  // whenever a controller queries — no NVS involved. UserLabel entries are
  // populated at boot from NVS by load_user_labels_() and persisted back on
  // every fabric-side write. `endpoint` is opaque here
  // (::esp_matter::endpoint_t *), passed as void to keep this header free
  // of esp-matter includes.
  void register_endpoint_label(void *endpoint, uint16_t endpoint_id, const std::string &label);

  // Single entry in a per-endpoint UserLabel list. Owned strings — CharSpans
  // handed to CHIP iterators reference the .data() while iteration is alive.
  struct UserLabelEntry {
    std::string label;
    std::string value;
  };

  // MatterDeviceInfoProvider reaches into these methods to satisfy CHIP's
  // UserLabel cluster reads and writes. Callable only from the CHIP
  // PlatformManager task (single-threaded — the DeviceInfoProvider interface
  // is only consulted there) so no locking.
  const std::vector<UserLabelEntry> *user_labels_for(uint16_t endpoint_id) const;
  size_t user_label_count_for(uint16_t endpoint_id) const;
  // Modifiers persist to NVS synchronously — the CHIP write path expects
  // durable storage before returning success to the fabric.
  int set_user_label_length(uint16_t endpoint_id, size_t new_length);
  int set_user_label_at(uint16_t endpoint_id, size_t index, const std::string &label, const std::string &value);
  int delete_user_label_at(uint16_t endpoint_id, size_t index);

  // Thread-safe way for endpoint wrappers to run an ESPHome-side action
  // (light.make_call().perform(), cover position updates, etc.) from a
  // CHIP callback. The `attribute_update_cb` invoked by esp-matter runs on
  // the CHIP PlatformManager event-loop task, NOT the ESPHome main loop,
  // and touching an ESPHome component's state machine off-loop races with
  // whatever the main loop is doing to that same component. On LightState
  // that manifested as `LightTransformer::setup` null-deref crashes after
  // a handful of rapid on/off/level toggles, because LevelControl's own
  // tick callback would fire concurrent Set()s from the CHIP thread while
  // the main loop was still animating a previous transition.
  //
  // Wraps `Component::defer()` (which is protected on the base class) so
  // wrappers can queue a lambda to run on the very next ESPHome loop tick,
  // on the main thread, without needing friendship or subclass access.
  void defer_on_main_loop(std::function<void()> cb) { this->defer(std::move(cb)); }

  // Marshal an esp_matter::attribute::update() onto the CHIP task via
  // PlatformMgr().ScheduleWork(). Every entity wrapper that reports device
  // state to the fabric ultimately calls this — routing the write through
  // the CHIP task keeps the ESPHome main loop from blocking on the CHIP
  // stack lock (esp_matter::attribute::update grabs it with portMAX_DELAY),
  // which can otherwise stall the loop past the task watchdog on a
  // high-endpoint bridge when the CHIP task is holding the lock to encode
  // subscription reports. Uses a fixed-size static pool of payloads to
  // avoid heap allocation after setup(); if the pool is exhausted the
  // update is dropped with a WARN log rather than blocking or growing.
  void defer_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                              const ::esp_matter_attr_val &val);

 protected:
  void log_onboarding_payload_();
  // Create the Aggregator endpoint (Matter device type 0x000E) that will
  // act as the parent of every entity endpoint. With this in place plus a
  // BridgedDeviceBasicInformation cluster on each child, controllers see
  // the device as a Matter Bridge and each endpoint as its own separate
  // accessory named by BridgedDeviceBasicInformation.NodeLabel (Matter spec
  // §9.13). Runs from setup() right after create_root_node_() so the
  // Aggregator handle is available before any entity wrapper calls
  // register_endpoint_label.
  void create_aggregator_endpoint_();
  // Load all UserLabel blobs from NVS ("mtr-ul" namespace, one blob per
  // endpoint keyed "ep<endpoint_id>") into user_labels_. Called from setup()
  // before endpoints are created — the DeviceInfoProvider then serves them
  // as soon as CHIP asks.
  void load_user_labels_();
  // Serialize user_labels_[endpoint_id] and write to NVS. Called from
  // set_user_label_length / set_user_label_at / delete_user_label_at on
  // every write. If the vector is empty, deletes the key instead of
  // writing an empty blob. Returns ESP_OK on success, or the first non-OK
  // NVS error the callers propagate as CHIP_ERROR_PERSISTED_STORAGE_FAILED
  // so the fabric sees the write failed instead of a silent lie.
  esp_err_t persist_user_labels_for_endpoint_(uint16_t endpoint_id);
  // Runs on the CHIP PlatformManager task — the public open_commissioning_
  // window() marshals here via ScheduleWork so the CommissioningWindowManager
  // touches (fresh spake2p verifier, discriminator, DNSSD advertisement)
  // happen on the same task that owns them.
  void open_commissioning_window_impl_(uint32_t timeout_seconds);
  // Writes the configured identity strings (vendor_name / product_name /
  // node_label / hw-ver-str / sw-ver-str) to the CHIP factory NVS namespace
  // before esp_matter::start reads them. Only writes non-empty values —
  // empty defaults leave CHIP's own default strings in place.
  void write_factory_strings_();
  // Points our MatterDeviceInfoProvider at endpoint_labels_ and registers
  // it with CHIP via esp_matter::set_custom_device_info_provider(). Must
  // run before esp_matter::start(): the setup_providers step inside
  // esp_matter::start reads the custom-provider pointer.
  void install_device_info_provider_();
  // Returns false on failure (node creation refused); setup() must bail
  // immediately so subsequent scan_and_register_* passes don't cascade
  // per-entity errors against a non-existent node. mark_failed() is called
  // internally so the caller only needs to short-circuit.
  bool create_root_node_();
  // Start the periodic subscription keep-alive tick. Some hub UIs — notably
  // the eWeLink Cube / NSPanel mobile app path (their v2.10.2 change log
  // states: "Router devices report within 1 minute … Devices exceeding the
  // reporting interval will appear offline until triggered again") — apply a
  // Zigbee-inherited "last-seen" heuristic on top of Matter subscriptions and
  // mark endpoints offline if no report arrives within their internal
  // threshold (~60s), regardless of the MaxIntervalCeiling negotiated for the
  // subscription. Matter-native controllers (Apple Home, Google Home) do not
  // do this — they honor the subscription MaxInterval — so the tick is a
  // no-op there beyond a small periodic report. Called from setup() after
  // esp_matter::start(); no-op if there are no bridged endpoints.
  void start_keepalive_ticker_();
  // Marks all bridged endpoints as dirty on the CHIP reporting engine, which
  // then batches subscription reports out to every active subscriber. Runs on
  // the CHIP PlatformManager task (marshaled via ScheduleWork by the ESPHome
  // interval callback) because MatterReportingAttributeChangeCallback touches
  // the ReadHandler pool that only the PlatformManager task owns.
  void keepalive_tick_impl_();
  void scan_and_register_switches_();
  void scan_and_register_binary_sensors_();
  void scan_and_register_sensors_();
  void scan_and_register_covers_();
  void scan_and_register_fans_();
  void scan_and_register_locks_();
  void scan_and_register_valves_();
  void scan_and_register_selects_();
  void scan_and_register_buttons_();
  void scan_and_register_lights_();
  void scan_and_register_climates_();
  void push_initial_states_();

  uint16_t vendor_id_;
  uint16_t product_id_;
  std::string setup_code_;
  uint16_t discriminator_;
  bool ble_commissioning_;
  std::string vendor_name_;
  std::string product_name_;
  std::string node_label_;
  std::string hardware_version_string_;
  std::string software_version_string_;
  // Selected endpoint topology. true = composed device (entity endpoints
  // attach directly to root_node, no Aggregator, no BridgedDeviceBasicInfo).
  // false = bridge (default): Aggregator + BDBI wraps every entity endpoint.
  // Chosen at yaml level via matter.topology; see the CONF_TOPOLOGY comment
  // in components/matter/__init__.py.
  bool composed_topology_;

  // (endpoint_id, name) pairs populated by register_endpoint_label() during
  // scan_and_register_*(). Read by MatterDeviceInfoProvider whenever a
  // controller queries the FixedLabel cluster on any endpoint — the vector
  // is stable for the lifetime of MatterComponent.
  FixedVector<std::pair<uint16_t, std::string>> endpoint_labels_;

  // Opaque handle to the Aggregator endpoint created by
  // create_aggregator_endpoint_(). Cast to ::esp_matter::endpoint_t * at
  // point of use — kept as void* so this header does not have to pull in
  // the esp-matter data-model headers. All entity endpoints reparent to
  // this via set_parent_endpoint() during register_endpoint_label().
  void *aggregator_endpoint_ = nullptr;

  // Owned storage for the string backing each endpoint's
  // BridgedDeviceBasicInformation.NodeLabel attribute. esp-matter's
  // create_node_label API stores the char* by reference (no internal copy),
  // so the buffer must outlive esp_matter::start AND the pointer must not
  // move as the vector grows. Using unique_ptr keeps each string at a
  // stable heap address regardless of vector realloc — safer than raw
  // std::string values, whose short-string-optimized data() would dangle
  // on realloc. Push-once; no keyed lookup needed.
  FixedVector<std::unique_ptr<std::string>> bridged_labels_;

  // Writable UserLabel entries, keyed by endpoint id. Populated at boot from
  // NVS by load_user_labels_() and mutated by fabric writes via the
  // set_user_label_*/delete_user_label_at methods. Each mutation persists
  // synchronously to NVS in persist_user_labels_for_endpoint_().
  std::unordered_map<uint16_t, std::vector<UserLabelEntry>> user_labels_;

  // Dispatcher lookup pattern: a std::vector<std::pair<uint16_t, T*>> and
  // a linear scan. Typical devices carry a handful of entities per platform,
  // where the ~2KB fixed overhead of std::unordered_map dwarfs the cost of a
  // linear scan over 5–30 pairs. This also drops the STL hash-table machinery
  // (rehash, bucket table, iterator invalidation on rehash) from the binary.

#ifdef USE_SWITCH
  // Owned wrappers, one per registered switch. Vector holds the storage;
  // endpoints_by_id_ is the dispatcher's linear-scan lookup.
  FixedVector<std::unique_ptr<MatterSwitchEndpoint>> switch_endpoints_;
  FixedVector<std::pair<uint16_t, MatterSwitchEndpoint *>> switch_endpoints_by_id_;
#endif
#ifdef USE_BINARY_SENSOR
  // Read-only wrappers — no dispatcher lookup needed.
  FixedVector<std::unique_ptr<MatterBinarySensorEndpoint>> binary_sensor_endpoints_;
#endif
#ifdef USE_SENSOR
  FixedVector<std::unique_ptr<MatterSensorEndpoint>> sensor_endpoints_;
#endif
#ifdef USE_COVER
  // Bidirectional wrappers — dispatcher needs endpoint→wrapper lookup for
  // fabric-driven target writes.
  FixedVector<std::unique_ptr<MatterCoverEndpoint>> cover_endpoints_;
  FixedVector<std::pair<uint16_t, MatterCoverEndpoint *>> cover_endpoints_by_id_;
#endif
#ifdef USE_FAN
  FixedVector<std::unique_ptr<MatterFanEndpoint>> fan_endpoints_;
  FixedVector<std::pair<uint16_t, MatterFanEndpoint *>> fan_endpoints_by_id_;
#endif
#ifdef USE_LOCK
  // Bidirectional wrappers — the DoorLock command hooks in the .cpp look up
  // the wrapper by endpoint id via handle_lock_command.
  FixedVector<std::unique_ptr<MatterLockEndpoint>> lock_endpoints_;
  FixedVector<std::pair<uint16_t, MatterLockEndpoint *>> lock_endpoints_by_id_;
#endif
#ifdef USE_VALVE
  // Bidirectional wrappers. No endpoint→wrapper map: fabric→device flows
  // through the CHIP Delegate pattern (MatterValveEndpoint implements the
  // ValveConfigurationAndControl::Delegate interface), so CHIP calls our
  // methods directly on the object registered during setup — no dispatcher
  // lookup needed.
  FixedVector<std::unique_ptr<MatterValveEndpoint>> valve_endpoints_;
#endif
#ifdef USE_SELECT
  // Bidirectional wrappers. Fabric ChangeToMode → CHIP writes CurrentMode →
  // our attribute_update_cb dispatches by endpoint, so we do need the map.
  FixedVector<std::unique_ptr<MatterSelectEndpoint>> select_endpoints_;
  FixedVector<std::pair<uint16_t, MatterSelectEndpoint *>> select_endpoints_by_id_;
#endif
#ifdef USE_BUTTON
  // Emit-only wrappers. Buttons don't receive commands from the fabric — no
  // dispatcher and no endpoint→wrapper map. Each wrapper hooks its ESPHome
  // button's press callback and emits Switch cluster events directly.
  FixedVector<std::unique_ptr<MatterButtonEndpoint>> button_endpoints_;
#endif
#ifdef USE_LIGHT
  // Bidirectional wrappers. Multiple Matter clusters per endpoint (OnOff +
  // LevelControl + ColorControl depending on device kind), so the map is
  // consulted from several dispatcher branches.
  FixedVector<std::unique_ptr<MatterLightEndpoint>> light_endpoints_;
  FixedVector<std::pair<uint16_t, MatterLightEndpoint *>> light_endpoints_by_id_;
#endif
#ifdef USE_CLIMATE
  // Bidirectional wrappers. Thermostat cluster has three writable attrs the
  // fabric touches (SystemMode + Occupied{Heating,Cooling}Setpoint); each
  // dispatcher branch consults the same map.
  FixedVector<std::unique_ptr<MatterClimateEndpoint>> climate_endpoints_;
  FixedVector<std::pair<uint16_t, MatterClimateEndpoint *>> climate_endpoints_by_id_;
#endif

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static MatterComponent *global_matter_component;
};

}  // namespace esphome::matter

#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
