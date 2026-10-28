#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace esphome::toptronic {

class TopTronic;
enum TypeName { U8, U16, U32, S8, S16, S32, S64 };  // value encoding of a datapoint
enum SensorType { SENSOR, TEXTSENSOR };             // downcast discriminator (no dynamic_cast in ESPHome builds)

static constexpr size_t GET_REQUEST_LEN = 6;
using GetRequest = std::array<uint8_t, GET_REQUEST_LEN>;
// A SET is the GET header with the SET command and a value of up to 8 bytes appended
static constexpr size_t MAX_SET_VALUE_LEN = 8;
static constexpr size_t MAX_SET_REQUEST_LEN = GET_REQUEST_LEN + MAX_SET_VALUE_LEN;
using SetRequest = std::array<uint8_t, MAX_SET_REQUEST_LEN>;
// The main loop runs every 16 ms, so a shorter spacing between two refresh GETs is not achievable
static constexpr uint32_t REFRESH_INTERVAL_MIN_MS = 16;
// CAN id prefix that marks the start frame of a message
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;

// Shared entity base. update() fires the GET wired up by the hub.
class TopTronicBase : public PollingComponent {
 public:
  void set_function_group(uint8_t function_group) { this->function_group_ = function_group; }
  void set_function_number(uint8_t function_number) { this->function_number_ = function_number; }
  void set_datapoint(uint16_t datapoint) { this->datapoint_ = datapoint; }
  uint32_t get_id();  // fg | fn << 8 | datapoint << 16; hub map key
  const GetRequest &get_request_data() const { return this->request_data_; }
  void cache_request_data();
  virtual SensorType type() = 0;
  void update() override;
  void add_on_update_callback(std::function<void()> &&callback);
  /// The hub an input sends its writes through, set by TopTronic::add_input()
  void set_hub(TopTronic *hub) { this->hub_ = hub; }
  /// Set by the hub for an input without a read entity: control() then publishes the written value itself
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  virtual const LogString *get_log_name() const = 0;
  // Set once a complete RESPONSE arrived; unlocks writes to this datapoint (reject_writes_before_read)
  void mark_read() { this->read_ = true; }
  bool was_read() const { return this->read_; }
  // Write pacing, kept per datapoint and read by the hub when it decides whether to send
  uint32_t last_write_ms() const { return this->last_write_ms_; }
  bool has_written() const { return this->written_; }
  void mark_written(uint32_t now) {
    this->last_write_ms_ = now;
    this->written_ = true;
  }

 protected:
  uint8_t function_group_;
  uint8_t function_number_;
  uint16_t datapoint_;
  GetRequest request_data_{};  // GET payload, built once at setup
  bool read_{false};
  bool optimistic_{false};
  bool written_{false};
  uint32_t last_write_ms_{0};
  TopTronic *hub_{nullptr};
  CallbackManager<void()> update_callback_;
  /// Builds the SET request for value (scaled, rounded and clamped to type) and hands it to the hub.
  /// Returns whether the hub sent it: paused, rate limited, rejected or failed on the bus return false.
  bool send_set_(float value, TypeName type);
};

class TopTronicSensor : public sensor::Sensor, public TopTronicBase {
 public:
  void set_type(TypeName type) { this->type_ = type; }
  float parse_value(const uint8_t *data, size_t len);
  size_t value_width() const;
  SensorType type() override { return SENSOR; }
  const LogString *get_log_name() const override { return this->sensor::Sensor::get_log_name(); }

 protected:
  TypeName type_;
};

#ifdef USE_NUMBER
class TopTronicNumber : public number::Number, public TopTronicBase {
 public:
  TopTronicNumber() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }
  float get_multiplier() { return this->multiplier_; }
  SensorType type() override { return SENSOR; }
  const LogString *get_log_name() const override { return this->number::Number::get_log_name(); }
  void control(float value) override;

 protected:
  TypeName type_;
  float multiplier_{1.0f};  // from YAML decimal (decimal: 1 -> x10)
};
#endif
#ifdef USE_TEXT_SENSOR
class TopTronicTextSensor : public text_sensor::TextSensor, public TopTronicBase {
 public:
  /// Returns the text for a value, or nullptr when it is not in the option list. The caller then
  /// keeps the last state instead of publishing an invalid one.
  const char *parse_value(const uint8_t *data, size_t len);
  SensorType type() override { return TEXTSENSOR; }
  const LogString *get_log_name() const override { return this->text_sensor::TextSensor::get_log_name(); }
  void add_option(uint8_t value, const char *text) { this->options_.push_back({value, text}); }

 protected:
  struct Option {
    uint8_t value;
    const char *text;  // codegen string literal, valid for the life of the program
  };
  std::vector<Option> options_;
};
#endif
#ifdef USE_SELECT
class TopTronicSelect : public select::Select, public TopTronicBase {
 public:
  TopTronicSelect() { this->set_update_interval(SCHEDULER_DONT_RUN); }
  void set_type(TypeName type) { this->type_ = type; }
  SensorType type() override { return TEXTSENSOR; }
  const LogString *get_log_name() const override { return this->select::Select::get_log_name(); }
  void add_option(uint8_t value) { this->values_.push_back(value); }

 protected:
  void control(size_t index) override;
  std::vector<uint8_t> values_;  // SET value per option index, in the same order as the options
  TypeName type_{U8};
};
#endif

class TopTronic : public Component
#ifdef USE_OTA_STATE_LISTENER
    ,
                  public ota::OTAGlobalStateListener
#endif
{
 public:
  // Also registers the hub for refresh_all(), before any setup() runs
  TopTronic(canbus::Canbus *canbus, uint16_t device_id);

  void add_sensor(TopTronicBase *sensor);
  void add_input(TopTronicBase *input);
  void register_sensor_callbacks();
  /// Sends a SET request for an input after the write guards.
  /// Returns whether the bus accepted it: paused, rate limited, rejected or failed return false.
  bool write_(TopTronicBase *input, const uint8_t *data, size_t len);
  // Queues a GET for every sensor, sent one at a time with retries (see the refresh setters)
  void update_all();
  // Runs update_all() on every hub, one hub every REFRESH_STAGGER_MS. With boot set, hubs that
  // disabled the boot refresh (boot_refresh_delay: 0) are skipped.
  void refresh_all(bool boot = false);
  void setup() override;
  void loop() override;
  void dump_config() override;
#ifdef USE_OTA_STATE_LISTENER
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *component) override;
#endif

  // Delay before the one-shot refresh_all() after boot; 0 disables it for this hub
  void set_boot_refresh_delay_ms(uint32_t ms) { this->boot_refresh_delay_ms_ = ms; }
  // Spacing between two refresh GETs, clamped to the loop period
  void set_refresh_interval_ms(uint32_t ms) {
    this->refresh_interval_ms_ = std::max<uint32_t>(ms, REFRESH_INTERVAL_MIN_MS);
  }
  // An unanswered GET is sent again after refresh_retry_interval, at most max_refresh_retries times
  void set_max_refresh_retries(uint8_t retries) { this->max_refresh_retries_ = retries; }
  void set_refresh_retry_interval_ms(uint32_t ms) { this->refresh_retry_interval_ms_ = ms; }

  // Reassembly limits; each is clamped to the storage the build reserved for the largest hub
  void set_max_pending_messages(uint8_t max_pending_messages) {
    this->max_pending_messages_ = static_cast<uint8_t>(std::min<size_t>(max_pending_messages, NUM_SLOTS));
  }
  void set_max_frames_per_message(uint8_t max_frames_per_message) {
    this->max_frames_per_message_ =
        static_cast<uint8_t>(std::min<size_t>(max_frames_per_message, MAX_FRAMES_PER_MESSAGE));
  }
  void set_max_pending_age_ms(uint32_t max_pending_age_ms) { this->max_pending_age_ms_ = max_pending_age_ms; }
  // Write safety: minimum time between two SETs of the same datapoint (0 = no limit), and whether a
  // SET is refused until its datapoint answered a GET. Write-only datapoints are never refused.
  void set_write_min_interval_ms(uint32_t ms) { this->write_min_interval_ms_ = ms; }
  void set_reject_writes_before_read(bool reject) { this->reject_writes_before_read_ = reject; }

  // CAN traffic logs, shared by every hub of the build: candump logs every frame sent or received, find_can_id
  // logs the sender of every single-frame GET (0x40) and RESPONSE (0x42) message. Both are off at boot and turn off
  // after DEBUG_AUTO_OFF_MS.
  static void set_candump_enabled(bool enabled) { set_debug_log(DEBUG_LOG_CANDUMP, enabled); }
  static void set_find_can_id_enabled(bool enabled) { set_debug_log(DEBUG_LOG_FIND_CAN_ID, enabled); }
  static bool is_candump_enabled() { return debug_logs[DEBUG_LOG_CANDUMP].enabled; }
  static bool is_find_can_id_enabled() { return debug_logs[DEBUG_LOG_FIND_CAN_ID].enabled; }
#ifdef USE_SWITCH
  // The switches of every hub follow the log state, including an automatic turn off
  void set_candump_switch(switch_::Switch *sw) { this->debug_switches_[DEBUG_LOG_CANDUMP] = sw; }
  void set_find_can_id_switch(switch_::Switch *sw) { this->debug_switches_[DEBUG_LOG_FIND_CAN_ID] = sw; }
#endif

  // Stop and restart polling and frame processing, for example during an OTA update
  void pause();
  void resume();
  bool is_paused() const { return this->paused_; }

 protected:
  // The 5-bit frame count field caps a message at 31 frames of up to 7 payload bytes; the build reserves
  // room for the largest max_frames_per_message of any hub
#ifdef TOPTRONIC_MAX_FRAMES
  static constexpr size_t MAX_FRAMES_PER_MESSAGE = TOPTRONIC_MAX_FRAMES;
#else
  static constexpr size_t MAX_FRAMES_PER_MESSAGE = 31;
#endif
  static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
  static constexpr size_t MAX_MESSAGE_LEN = MAX_FRAMES_PER_MESSAGE * MAX_CONT_FRAME_PAYLOAD;
  static_assert(MAX_MESSAGE_LEN <= UINT8_MAX, "Slot::len is a uint8_t");
  // One sender answers requests one after another; a second slot covers an abandoned message.
  // max_pending_messages raises this for the largest hub.
#ifdef TOPTRONIC_SLOTS
  static constexpr size_t NUM_SLOTS = TOPTRONIC_SLOTS;
#else
  static constexpr size_t NUM_SLOTS = 2;
#endif

  // refresh_all() covers every hub of the build
#ifdef TOPTRONIC_HUB_COUNT
  static constexpr size_t MAX_HUBS = TOPTRONIC_HUB_COUNT;
#else
  static constexpr size_t MAX_HUBS = 4;
#endif
  static StaticVector<TopTronic *, MAX_HUBS> hubs;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  static bool boot_refresh_done;                    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  enum DebugLog : uint8_t { DEBUG_LOG_CANDUMP, DEBUG_LOG_FIND_CAN_ID, DEBUG_LOG_COUNT };
  struct DebugLogState {
    uint32_t start_ms;
    bool enabled;
  };
  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
  static std::array<DebugLogState, DEBUG_LOG_COUNT> debug_logs;
  static uint32_t last_candump_log_ms;
  static uint32_t candump_dropped;  // received frames skipped by the candump rate limit since the last line
  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

  static void set_debug_log(DebugLog log, bool enabled);
  /// Turns off a debug log that has run past DEBUG_AUTO_OFF_MS; returns whether it is still on.
  static bool debug_log_active(DebugLog log, uint32_t now);
  /// Logs a frame seen on the bus or sent by this node, for whichever debug logs are on.
  static void log_debug_frame(uint32_t can_id, const uint8_t *data, size_t len, bool sent);
  /// True for a start frame with a frame count of 0, the only shape whose [1] byte is a command.
  static bool is_single_frame_message(uint32_t can_id, const uint8_t *data, size_t len) {
    return len >= 2 && (can_id >> 24) == START_OF_MESSAGE_ID && (data[0] >> 3) == 0;
  }

  struct RefreshEntry {
    TopTronicBase *sensor;
    uint32_t last_send_ms;
    uint8_t attempts;  // GETs sent for this sensor in the current refresh
  };

  struct Slot {
    uint32_t last_update_ms;
    uint8_t len;
    uint8_t tag;
    uint8_t remaining;  // continuation frames still expected, 0 when free
    uint8_t data[MAX_MESSAGE_LEN];

    bool is_live(uint32_t now, uint32_t max_age_ms) const {
      return this->remaining != 0 && now - this->last_update_ms <= max_age_ms;
    }
  };

  /// Sends a request, split into a start frame and continuation frames when it does not fit in one.
  /// Returns the first bus error, if any.
  canbus::Error send_frames_(uint32_t can_id, const uint8_t *data, size_t len);
  /// Feeds one frame from this device into reassembly. Returns true and points msg/len at the payload when
  /// a message is complete (CRC already checked and stripped); the pointer is valid until the next frame.
  bool parse_frame_(const uint8_t *data, size_t size, uint32_t can_id, const uint8_t **msg, size_t *len);
  void interpret_message_(const uint8_t *data, size_t len);
  void link_inputs_();
  void drain_refresh_();
  void finish_refresh_(bool stalled);
  void remove_refresh_entry_(TopTronicBase *sensor);
  TopTronicBase *get_sensor_(uint32_t sensor_id);

  canbus::Canbus *canbus_;
#ifdef USE_SWITCH
  std::array<switch_::Switch *, DEBUG_LOG_COUNT> debug_switches_{};
#endif
  std::unordered_map<uint32_t, TopTronicBase *> sensors_;
  std::unordered_map<uint32_t, TopTronicBase *> inputs_;
  std::array<Slot, NUM_SLOTS> slots_{};
  // Sensors whose refresh GET is still unanswered, oldest send first; sized in setup()
  FixedVector<RefreshEntry> refresh_queue_;
  // A pending message with no frame for this long is treated as lost
  uint32_t max_pending_age_ms_{5000};
  uint32_t write_min_interval_ms_{2000};
  uint32_t boot_refresh_delay_ms_{30000};
  uint32_t refresh_interval_ms_{50};
  uint32_t refresh_retry_interval_ms_{200};
  uint32_t last_refresh_send_ms_{0};
  uint32_t last_refresh_progress_ms_{0};
  uint16_t refresh_queued_{0};
  uint16_t refresh_answered_{0};
  uint16_t refresh_dropped_{0};
  uint8_t msg_counter_{0};  // frame tag of the last multi-frame message sent by this hub
  uint16_t device_id_;
  uint8_t max_pending_messages_{NUM_SLOTS};
  uint8_t max_frames_per_message_{MAX_FRAMES_PER_MESSAGE};
  uint8_t max_refresh_retries_{1};
  bool reject_writes_before_read_{true};
  bool paused_{false};
  bool refresh_in_progress_{false};
  bool refresh_requested_{false};  // update_all() during a refresh runs again once it ends
};

}  // namespace esphome::toptronic
