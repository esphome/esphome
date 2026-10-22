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

#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

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

  // Human-readable entity name (the YAML name). TopTronicBase has no Entity
  // base, so each concrete entity type forwards this to its ESPHome Entity.
  virtual const LogString *get_log_name() const = 0;

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
  // Number of value bytes a complete RESPONSE must carry for this sensor's
  // configured type (1/2/4/8) - used to reject truncated frames before they
  // are decoded and before the cold-cache write guard is unlocked.
  size_t value_width() const;
  SensorType type() override { return SENSOR; }
  const LogString *get_log_name() const override { return this->sensor::Sensor::get_log_name(); }

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
  const LogString *get_log_name() const override { return this->number::Number::get_log_name(); }
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
  const LogString *get_log_name() const override { return this->text_sensor::TextSensor::get_log_name(); }

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
  const LogString *get_log_name() const override { return this->select::Select::get_log_name(); }
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
  // Arms the CAN receive path immediately. All hubs are constructed in generated
  // main.cpp before App.setup() runs, so a hub can receive and parse frames even
  // if its setup() is deferred by another slow component.
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

  // Trigger a GET refresh for every registered sensor across all devices.
  // Can be called from a template button or automation to force an update.
  void update_all();

  void set_device_type(uint16_t device_type) { this->device_type_ = device_type; }
  void set_device_addr(uint8_t device_addr) { this->device_addr_ = device_addr; }
  uint16_t get_device_id() { return this->device_type_ | this->device_addr_; }

  // Tuning knobs for the multi-frame reassembly buffer, stale-fragment sweep,
  // refresh burst throttle, and start-frame count guard. All optional in YAML;
  // the defaults match the historical built-in constants.
  void set_max_pending_messages(size_t max_pending_messages) { this->max_pending_messages_ = max_pending_messages; }
  void set_max_pending_age_ms(uint32_t max_pending_age_ms) { this->max_pending_age_ms_ = max_pending_age_ms; }
  void set_cleanup_interval_ms(uint32_t cleanup_interval_ms) { this->cleanup_interval_ms_ = cleanup_interval_ms; }
  void set_max_frames_per_message(uint8_t max_frames_per_message) {
    this->max_frames_per_message_ = max_frames_per_message;
  }
  // Write safety: minimum spacing (ms) between two SET requests for the same
  // datapoint. 0 disables the rate limit (default 2000 ms). Protects the
  // 50 kbps bus and the boiler controller from rapid SET spamming.
  void set_write_min_interval(uint32_t ms) { this->write_min_interval_ms_ = ms; }
  // Write safety (cold-cache guard): reject SET requests for datapoints that
  // have not delivered at least one RESPONSE since boot, so we never write
  // blind. Datapoints without a registered read sensor (e.g. the HV
  // filter-maintenance button) are never blocked. Default true.
  void set_reject_writes_before_read(bool reject) { this->reject_writes_before_read_ = reject; }

  // Pause/resume CAN frame processing. Used during OTA to free the main loop
  // and logging path so the update connection is not starved.
  // Implemented in toptronic.cpp so transitions are logged (makes a stuck-pause
  // bug visible instead of silently freezing polls and refresh bursts).
  void pause();
  void resume();
  bool is_paused() { return this->paused_; }

  void setup() override;
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

  // Multi-frame reassembly buffer cap (default 32).
  size_t max_pending_messages_{32};
  // A pending message with no continuation frame for this long is considered lost (default 5000 ms).
  uint32_t max_pending_age_ms_{5000};
  // Sweep interval for the stale-fragment eviction (default 5000 ms).
  uint32_t cleanup_interval_ms_{5000};
  // Largest sane multi-frame message in total frames (default 8).
  uint8_t max_frames_per_message_{8};

  // Write safety state.
  // Last SET timestamp per datapoint (keyed by get_id()) for the rate limit.
  std::unordered_map<uint32_t, uint32_t> last_write_ms_;
  // Datapoints (get_id()) that have published at least one value since boot.
  std::unordered_set<uint32_t> read_ok_ids_;
  // Minimum ms between two SETs of the same datapoint (0 = no limit).
  uint32_t write_min_interval_ms_{2000};
  // Cold-cache guard: reject SETs until the datapoint answered a GET.
  bool reject_writes_before_read_{true};

  // When true, parse_frame() ignores incoming CAN frames (used during OTA).
  bool paused_{false};
};

}  // namespace esphome::toptronic

#endif  // USE_ESP32
