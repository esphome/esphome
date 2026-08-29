#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include <deque>
#include <map>
#include <memory>
#include <unordered_map>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esphome::toptronic {

class TopTronic;

// Byte width / signedness of a TopTronic datapoint value.
// Determines how raw CAN bytes are interpreted as a numeric type.
enum TypeName {
  U8,
  U16,
  U32,
  S8,
  S16,
  S32,
  S64,
};

// Discriminator used to downcast TopTronicBase pointers to the correct subtype
// without RTTI (dynamic_cast is disabled in ESPHome builds).
enum SensorType {
  SENSOR,      // read-only numeric (TopTronicSensor)
  TEXTSENSOR,  // read-only string  (TopTronicTextSensor) or read-write (TopTronicSelect)
  BUTTON,      // fire-and-forget action (TopTronicButton)
};

// Build a 29-bit extended CAN ID from sender and receiver addresses.
uint32_t build_can_id(uint16_t sender_id, uint16_t receiver_mask);
// Build a GET request payload asking for a single datapoint.
std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint);
// Build a SET request payload with the given value bytes appended.
std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint,
                                       const std::vector<uint8_t> &value);

// Base class shared by all TopTronic entity types (sensor, number, text sensor, select).
// Extends PollingComponent so ESPHome calls update() on a configurable interval,
// which triggers a GET request to read the latest value from the boiler.
//
// Device addressing (device_type_/device_addr_) is owned by the TopTronic hub and
// inherited by every entity registered on it.
class TopTronicBase : public PollingComponent {
 public:
  void set_function_group(uint8_t function_group) { this->function_group_ = function_group; }
  void set_function_number(uint8_t function_number) { this->function_number_ = function_number; }
  void set_datapoint(uint16_t datapoint) { this->datapoint_ = datapoint; }

  // Unique entity ID: encodes function_group | function_number | datapoint into a single uint32.
  // Used as the key in the per-device sensor/input hash maps.
  uint32_t get_id();

  // Returns the pre-built GET request payload. Must call cache_request_data() first (done at setup).
  const std::vector<uint8_t> &get_request_data();
  // Builds and stores the GET request payload once, avoiding a heap allocation on every poll.
  void cache_request_data();

  virtual SensorType type() = 0;

  // Called by ESPHome scheduler at the configured polling interval.
  void update() override;
  void add_on_update_callback(std::function<void()> &&callback);
  void add_on_set_callback(std::function<void(const std::vector<uint8_t> &)> &&callback);

 protected:
  uint8_t function_group_;
  uint8_t function_number_;
  uint16_t datapoint_;

  // GET request bytes, built once at setup. Returned by reference to avoid copying on every poll.
  std::vector<uint8_t> request_data_;

  CallbackManager<void()> update_callback_;
  CallbackManager<void(const std::vector<uint8_t> &)> set_callback_;
};

#ifdef USE_SENSOR
// Read-only numeric sensor: receives CAN responses and publishes a float state.
class TopTronicSensor : public sensor::Sensor, public TopTronicBase {
 public:
  void set_type(TypeName type) { this->type_ = type; }

  // Decode raw bytes from the CAN response into a float using the configured type.
  // data/len point into the reassembly buffer or the received CAN frame — no copy.
  float parse_value(const uint8_t *data, size_t len);
  SensorType type() override { return SENSOR; }

 protected:
  TypeName type_;
};
#endif

#ifdef USE_NUMBER
// Writable numeric entity: sends a SET request on the CAN bus when the user changes the value.
// The multiplier converts between the HA-facing value (e.g. °C) and the raw boiler integer.
class TopTronicNumber : public number::Number, public TopTronicBase {
 public:
  // Write-only entity: never polls (no update callback registered). The schema
  // no longer emits set_update_interval(), so this guards against any future
  // codegen change that would re-enable the no-op polling tick.
  TopTronicNumber() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }
  float get_multiplier() { return this->multiplier_; }
  SensorType type() override { return SENSOR; }
  // Called by ESPHome when the user sets a new value from Home Assistant.
  void control(float value) override;

 protected:
  TypeName type_;
  float multiplier_{1.0f};  // set from YAML decimal: field (e.g. decimal: 1 → multiplier 10)
};
#endif

#ifdef USE_TEXT_SENSOR
// Read-only text sensor: maps raw boiler integer codes to human-readable strings.
// Both directions are stored (int→text for reading, text→int for the paired select).
class TopTronicTextSensor : public text_sensor::TextSensor, public TopTronicBase {
 public:
  // Decode raw bytes from the CAN response into a string label using the option map.
  // data/len point into the reassembly buffer or the received CAN frame — no copy.
  std::string parse_value(const uint8_t *data, size_t len);
  SensorType type() override { return TEXTSENSOR; }

  // Register a value↔text mapping (called from generated YAML code at startup).
  void add_option(uint8_t value, const std::string &text) {
    this->to_text_[value] = text;
    this->to_value_[text] = value;
  }

 protected:
  // std::map chosen over unordered_map here: string keys make hashing less beneficial,
  // and these maps are tiny (one entry per YAML option) and only accessed at set-time.
  std::map<uint8_t, std::string> to_text_;   // raw boiler code → label string
  std::map<std::string, uint8_t> to_value_;  // label string → raw boiler code
};
#endif

#ifdef USE_SELECT
// Writable select entity: sends a SET request when the user picks a new option.
// Mirrors TopTronicTextSensor but exposes a dropdown in Home Assistant.
class TopTronicSelect : public select::Select, public TopTronicBase {
 public:
  // Write-only entity: never polls (no update callback registered). See
  // TopTronicNumber for the SCHEDULER_DONT_RUN rationale.
  TopTronicSelect() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  SensorType type() override { return TEXTSENSOR; }
  TypeName get_value_type() { return this->type_; }

  // Register a value↔text mapping (called from generated YAML code at startup).
  void add_option(uint8_t value, const std::string &text) {
    this->to_text_[value] = text;
    this->to_value_[text] = value;
  }

 protected:
  std::map<uint8_t, std::string> to_text_;   // raw boiler code → label string
  std::map<std::string, uint8_t> to_value_;  // label string → raw boiler code

  TypeName type_{U8};  // value encoding width (U8 default; U16 used by party duration)
  // Called by ESPHome when the user picks a new option from Home Assistant.
  void control(const std::string &text) override;
};
#endif

#ifdef USE_BUTTON
// Fire-and-forget button: sends a fixed SET request on the CAN bus when pressed.
// The value is configured in YAML (e.g. an acknowledgment or reset command).
class TopTronicButton : public button::Button, public TopTronicBase {
 public:
  // Fire-and-forget: never polls (no update callback registered). See
  // TopTronicNumber for the SCHEDULER_DONT_RUN rationale.
  TopTronicButton() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  void set_value(float value) { this->value_ = value; }
  SensorType type() override { return BUTTON; }

 protected:
  TypeName type_;
  float value_{0.0f};

  // Called by ESPHome when the user presses the button from Home Assistant.
  void press_action() override;
};

// Build-wide "Refresh all" button, generated once by the component codegen
// (__init__.py). press_action() calls the parent hub's refresh_all(), which
// fans out to EVERY registered hub (HV, BM, WEZ …) and staggers each hub's
// batch by REFRESH_STAGGER_MS so the 50 kbps bus is not spammed.
class TopTronicRefreshButton : public button::Button, public Component {
 public:
  void set_parent(TopTronic *parent) { this->parent_ = parent; }

 protected:
  // Called by ESPHome when the user presses the button from Home Assistant.
  void press_action() override;

  TopTronic *parent_{nullptr};
};
#endif

#ifdef USE_SWITCH
// Debug switches, one per independent debug feature. Each switch controls its
// own build-wide boolean flag (s_candump_enabled / s_find_can_id_enabled) and
// registers on its own per-feature callback manager, so the two switches are
// fully independent — turning one ON never affects the other, and both can be
// active simultaneously. The switches are assumed-state (optimistic): HA applies
// the user's toggle immediately, and the component mirrors the real flag via
// publish_state on every change, so the UI can never get stuck ON.
class TopTronicDebugSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(TopTronic *parent) { this->parent_ = parent; }
  // The debug feature this switch controls (1 = candump, 2 = find can_id).
  // This is only a discriminator — the actual enable/disable path uses the
  // dedicated set_candump_enabled() / set_find_can_id_enabled() setters.
  void set_debug_mode(uint8_t mode) { this->mode_ = mode; }

  void setup() override;
  void dump_config() override;

  // Debug switches are "soft" momentary controls: the real logging state is a
  // build-wide flag that can also be turned OFF by the runtime auto-off timer,
  // so the front-end must not assume it can predict the device state. Marking
  // the switch as assumed-state puts Home Assistant in optimistic mode — the
  // toggle responds immediately and never fights the state pushed back from
  // the device.
  bool assumed_state() override { return true; }

 protected:
  // Switch base class hook: state=true → enable this switch's debug feature,
  // state=false → disable it. Only the feature selected by mode_ is touched.
  void write_state(bool state) override;

  TopTronic *parent_{nullptr};
  uint8_t mode_{0};
};
#endif

// Groups all sensors and writable inputs that belong to a single CAN device (device_type | device_addr).
// Lifetime of sensors/inputs is managed by the ESPHome component registry — these are non-owning pointers.
class TopTronicDevice {
 public:
  // unordered_map: integer keys allow O(1) lookup on every received CAN frame.
  // Per §5.9 the maps stay private (pointer lifetime safety — the pointers are
  // owned by the ESPHome registry, never deleted here); the hub reaches them
  // only through these accessors.
  std::unordered_map<uint32_t, TopTronicBase *> &get_sensors() { return this->sensors_; }
  const std::unordered_map<uint32_t, TopTronicBase *> &get_sensors() const { return this->sensors_; }
  std::unordered_map<uint32_t, TopTronicBase *> &get_inputs() { return this->inputs_; }
  const std::unordered_map<uint32_t, TopTronicBase *> &get_inputs() const { return this->inputs_; }

 private:
  std::unordered_map<uint32_t, TopTronicBase *> sensors_;  // keyed by get_id()
  std::unordered_map<uint32_t, TopTronicBase *> inputs_;   // keyed by get_id()
};

// Root component: owns all TopTronicDevice instances and is the entry point
// for every incoming CAN frame (parse_frame is called from the internal
// canbus callback registered in setup()).
class TopTronic : public Component {
 public:
  // Registers this hub in the build-wide registry immediately, so refresh_all()
  // always covers every hub even if setup() has not run yet (e.g. during a slow
  // App.setup() stall). All hubs are constructed in generated main.cpp before
  // App.setup() runs.
  explicit TopTronic(canbus::Canbus *canbus);

  // Register sensors and inputs (called from generated YAML code).
  void add_sensor(TopTronicBase *sensor);
  void add_input(TopTronicBase *input);

  // Called on every received CAN frame. Handles multi-frame reassembly,
  // then dispatches to interpret_message_() once a complete message is available.
  void parse_frame(const std::vector<uint8_t> &data, uint32_t can_id, bool remote_transmission_request);

  // Wire up polling callbacks for read-only sensors (GET requests on update interval).
  void register_sensor_callbacks();
  // Wire up write callbacks for inputs so they send SET requests over CAN.
  void register_input_callbacks();

  // One-time functional wiring (link_inputs_, sensor/input callbacks, command
  // queue, boot-refresh gate, work-pump scheduling). Called from the CONFIG
  // phase (emitted by __init__.py right after all add_sensor/add_input calls)
  // instead of setup(): ESPHome sizes components_ from ESPHOME_COMPONENT_COUNT,
  // which is computed before this component registers its preset-entity IDs, so
  // a hub can be silently dropped from components_ and its setup() never runs.
  // Config-phase statements always run for every hub.
  void configure_hub();

  // Trigger a GET refresh for every registered sensor across all devices.
  // Can be called from a template button or automation to force an update.
  void update_all();

  // Refresh EVERY registered TopTronic hub's sensors, staggering each hub's
  // batch by REFRESH_STAGGER_MS so the 50 kbps bus is not spammed with a burst
  // of GET frames. Fans out to all hubs, so calling it on any one hub covers
  // every hub (HV, BM, WEZ …). Also used by the one-shot boot refresh.
  void refresh_all();

  // Process the throttled refresh burst (send/retry/drop, completion log, stall
  // watchdog, state heartbeat, deferred run). Called from loop() and also
  // re-scheduled through the ESPHome scheduler, so a hub whose component loop()
  // is not invoked still drains its burst.
  void drain_refresh_burst();

  // Scheduler-driven work pump (order-independent): all critical work (command
  // bridge, burst drain, boot refresh, cleanup) is driven from here via
  // set_interval() in setup(), so a hub works even if ESPHome does not invoke
  // its component loop() (Phase B). loop() calls pump() as well.
  void pump();

  // Thread-safe requests callable from any FreeRTOS task. They only enqueue a
  // command; the main loop task drains the queue and does the real work, so all
  // component state stays single-threaded. Non-blocking (not ISR-safe).
  void request_refresh();
  void request_pause();
  void request_resume();

  // Fan out Pause/Resume to EVERY registered hub via the build-wide registry,
  // so OTA pause/resume lambdas can target any single hub id and still cover
  // all hubs (HV, BM, WEZ …). Thread-safe request variants.
  void request_pause_all();
  void request_resume_all();

  void set_device_type(uint16_t device_type) { this->device_type_ = device_type; }
  void set_device_addr(uint8_t device_addr) { this->device_addr_ = device_addr; }
  uint16_t get_device_id() { return this->device_type_ | this->device_addr_; }

  // Delay before the one-shot post-boot refresh; 0 disables it.
  void set_boot_refresh_delay(uint32_t ms) { this->boot_refresh_delay_ms_ = ms; }

  // Tuning knobs for the multi-frame reassembly buffer, stale-fragment sweep,
  // refresh burst throttle, and start-frame count guard. All optional in YAML;
  // the defaults match the historical built-in constants.
  void set_max_pending_messages(size_t max_pending_messages) { this->max_pending_messages_ = max_pending_messages; }
  void set_max_pending_age_ms(uint32_t max_pending_age_ms) { this->max_pending_age_ms_ = max_pending_age_ms; }
  void set_cleanup_interval_ms(uint32_t cleanup_interval_ms) { this->cleanup_interval_ms_ = cleanup_interval_ms; }
  void set_max_refresh_per_loop(size_t max_refresh_per_loop) { this->max_refresh_per_loop_ = max_refresh_per_loop; }
  void set_max_frames_per_message(uint8_t max_frames_per_message) {
    this->max_frames_per_message_ = max_frames_per_message;
  }
  // Length (ms) of the refresh window that max_refresh_per_loop_ GETs are spread
  // across; effective per-GET spacing = ceil(refresh_gap_ms_ / max_refresh_per_loop_)
  // so a burst never emits more than max_refresh_per_loop_ GETs within the window.
  void set_refresh_gap_ms(uint32_t refresh_gap_ms) { this->refresh_gap_ms_ = refresh_gap_ms; }
  // Maximum number of retransmissions for an unanswered GET during a refresh
  // burst. Re-sent after refresh_retry_interval_ms_ of no response, so a lost
  // GET (e.g. colliding with the boiler's own broadcast traffic) self-heals
  // instead of leaving the sensor unknown until the next 30 s poll.
  void set_max_refresh_retries(uint32_t max_refresh_retries) { this->max_refresh_retries_ = max_refresh_retries; }
  // Wall-clock delay (ms) before an unanswered GET in a refresh burst is re-sent.
  void set_refresh_retry_interval_ms(uint32_t refresh_retry_interval_ms) {
    this->refresh_retry_interval_ms_ = refresh_retry_interval_ms;
  }

  // Debug frame logging (candump / find-can_id). Each feature is an independent
  // build-wide boolean flag, so the two debug switches never interfere with each
  // other and can even both be active at the same time. Both flags reset to OFF
  // on every boot. Logging callbacks are deduplicated across hubs (see setup()).
  void set_candump_enabled(bool enabled);
  void set_find_can_id_enabled(bool enabled);
  bool get_candump_enabled();
  bool get_find_can_id_enabled();

  // Register a callback fired whenever a specific debug flag changes, so the
  // matching TopTronicDebugSwitch stays in sync with the real logging state.
  // (Instantiated by the debug switches themselves — no user-facing API.)
  void add_candump_update_callback(std::function<void(bool)> &&callback);
  void add_find_can_id_update_callback(std::function<void(bool)> &&callback);
  // Build-wide fan-out for each debug flag (see s_candump_enabled semantics).
  static CallbackManager<void(bool)> candump_update_callbacks;
  static CallbackManager<void(bool)> find_can_id_update_callbacks;

  // Pause/resume CAN frame processing. Used during OTA to free the main loop
  // and logging path so the update connection is not starved.
  // Implemented in toptronic.cpp so transitions are logged (makes a stuck-pause
  // bug visible instead of silently freezing polls and refresh bursts).
  void pause();
  void resume();
  bool is_paused() { return this->paused_; }

  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;

 protected:
  // State of an in-progress multi-frame message being reassembled.
  struct PendingMessage {
    std::vector<uint8_t> data;    // accumulated payload (CRC not yet stripped)
    uint8_t remaining_frames{0};  // continuation frames still expected
    uint32_t last_update_ms{0};   // for stale-fragment expiry
  };

  // Look up device by ID; create a new TopTronicDevice if it does not exist yet.
  TopTronicDevice *get_or_create_device_(uint32_t device_id);
  // For each input, find its matching read sensor (same device + datapoint) and
  // subscribe so the input stays in sync with the current boiler value.
  void link_inputs_();
  // Return the sensor for a given (device_id, sensor_id) pair, or nullptr if not found.
  TopTronicBase *get_sensor_(uint32_t device_id, uint32_t sensor_id);
  // Parse a fully reassembled CAN message and update the matching sensor or input.
  // data/len reference the reassembly buffer directly — no per-frame heap copies.
  void interpret_message_(const uint8_t *data, size_t len, uint32_t can_id, bool remote_transmission_request);

  // True if a device with this sender node id is registered on this hub. The
  // sender node id (bits 21-11 of a CAN id) is exactly the devices_ map key, so
  // membership is a single O(1) lookup — it covers EVERY device this hub owns,
  // not a single hardcoded address.
  bool owns_device_(uint32_t device_id) const { return this->devices_.contains(device_id); }

  canbus::Canbus *canbus_;

  // Devices are owned here — unique_ptr guarantees cleanup when TopTronic is destroyed.
  // unordered_map gives O(1) lookup by device ID on every received CAN frame.
  std::unordered_map<uint32_t, std::unique_ptr<TopTronicDevice>> devices_;

  // Multi-frame reassembly buffer. TopTronic protocol splits long messages across
  // several CAN frames; each entry holds the accumulated bytes and remaining frame count.
  // Keyed by (source_device_id << 8 | msg_header) from the first frame so that identical
  // 8-bit msg_header values from different CAN devices cannot collide.
  std::unordered_map<uint32_t, PendingMessage> pending_messages_;

  // CAN address of this gateway node (used when building outgoing CAN IDs).
  uint16_t device_type_;
  uint8_t device_addr_;

  // Commands marshalled from other tasks into the main loop task.
  // Enumerator names are UPPER_SNAKE_CASE per §5.3; values stay 0/1/2.
  enum class Command : uint8_t { REFRESH, PAUSE, RESUME };
  // Producer/consumer bridge: other tasks enqueue here, loop() drains on the
  // main loop task. Created in setup(); nullptr until then.
  QueueHandle_t cmd_queue_{nullptr};

  // Delay (ms) for the one-shot post-boot refresh; 0 disables it.
  uint32_t boot_refresh_delay_ms_{30000};

  // Multi-frame reassembly buffer cap (default 32).
  size_t max_pending_messages_{32};
  // A pending message with no continuation frame for this long is considered lost (default 5000 ms).
  uint32_t max_pending_age_ms_{5000};
  // Throttle interval for the stale-fragment sweep in loop() (default 5000 ms).
  uint32_t cleanup_interval_ms_{5000};
  // GET burst budget per refresh_gap_ms_ window (default 8). Combined with
  // refresh_gap_ms_ it yields the effective per-GET spacing, so BOTH knobs
  // drive the refresh throughput: raising it sends more GETs per window,
  // lowering it paces harder.
  size_t max_refresh_per_loop_{8};
  // Largest sane multi-frame message in total frames (default 8).
  uint8_t max_frames_per_message_{8};
  // Length (ms) of the refresh window that max_refresh_per_loop_ GETs are spread
  // across. Effective per-GET spacing = ceil(refresh_gap_ms_ / max_refresh_per_loop_)
  // (default 50 ms / 8 = 7 ms), which keeps the boiler's responses interleaved so
  // the main loop stays responsive during a refresh_all() and never exceeds the
  // per-window GET budget.
  uint32_t refresh_gap_ms_{50};
  // Maximum number of re-sends for an unanswered GET during a refresh burst
  // (default 0 = single-pass, no retries; the normal 30 s poll is the backstop).
  // If you enable it, keep it small: a burst should never hammer a dead/absent device.
  uint32_t max_refresh_retries_{0};
  // Wall-clock delay (ms) before an unanswered GET is re-sent during a refresh
  // burst (default 200 ms). Longer than the worst-case multi-frame response so
  // a slow device response is not needlessly re-polled.
  uint32_t refresh_retry_interval_ms_{200};
  // Timestamp (millis()) of the last GET sent from a refresh burst. Starts at 0
  // so the first GET of a burst goes out immediately.
  uint32_t last_refresh_send_ms_{0};

  // Sensors still waiting in a throttled update_all() burst, each tracked so an
  // unanswered GET can be re-sent (see max_refresh_retries_ /
  // refresh_retry_interval_ms_). On a fresh send the entry is pushed back; a
  // retry updates the in-place entry (update_all() never queues duplicates).
  // Empty = no burst pending.
  struct RefreshEntry {
    TopTronicBase *sensor;
    uint32_t last_send_ms;  // millis() when this GET was last sent
    uint32_t attempts;      // GETs sent so far for this sensor this burst
  };
  std::deque<RefreshEntry> pending_refresh_;

  // Coalesced refresh request: set by update_all() when a refresh is asked for
  // while a burst is still draining. loop() starts one fresh burst once the
  // current one empties, so a "Refresh all" press during a burst is deferred
  // and honored rather than silently dropped.
  bool refresh_pending_{false};

  // Refresh-burst observability + stall watchdog. burst_in_progress_ is true
  // while a throttled burst is draining; the counters feed the completion log
  // (queued / answered / dropped) and last_burst_progress_ms_ drives the
  // loop() stall watchdog, which aborts a burst that stops making progress
  // (no GET sent, no response erased) for 5 s + refresh_retry_interval_ms_.
  bool burst_in_progress_{false};
  size_t burst_queued_{0};
  size_t burst_answered_{0};
  size_t burst_dropped_{0};
  uint32_t last_burst_progress_ms_{0};

  // Timestamp of the last stale-fragment sweep in loop().
  uint32_t last_cleanup_ms_{0};

  // When true, parse_frame() ignores incoming CAN frames (used during OTA).
  bool paused_{false};
};

}  // namespace esphome::toptronic

#endif  // USE_ESP32
