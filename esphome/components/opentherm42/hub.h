#pragma once

#include <array>
#include <memory>
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "datalink.h"
#include "flag_bits.h"

namespace esphome::opentherm42 {

// §5.2/§5.3: which conversation is currently in flight, so handle_response_() knows how to interpret
// the reply. Every kind maps to exactly one data-id; see build_next_request_()/handle_response_().
enum class RequestKind : uint8_t {
  // §5.3.1 Class 1, ID 0: the master/boiler status exchange -- the protocol's mandatory heartbeat.
  STATUS,
  // §5.3.1 Class 1, ID 1: control setpoint.
  CONTROL_SETPOINT,
};

class OpenTherm42Hub;

// How a SimpleSensorInfo's raw frame bytes convert to the value passed to sensor::Sensor::publish_state().
enum class SimpleValueKind : uint8_t {
  F88,    // frame.value_f88()
  S16,    // frame.value_s16() -- a plain signed integer, not fixed-point, despite also being 16 bits
  U16,    // frame.value_u16()
  U8_LB,  // frame.value_lb -- only the low byte carries data
  U8_HB,  // frame.value_hb -- only the high byte carries data
};

// Describes one single-value, non-bit-decomposed read-only data-id: which id to READ_DATA, how to
// convert the response into a float, which member holds its sensor pointer, and what to name it in log
// messages. OpenTherm42Hub::SIMPLE_SENSORS is the table every class after Class 1 registers its "plain"
// reads into; find_simple_sensor_() is the generic fallback build_next_request_()/handle_response_()/
// invalidate_response_() dispatch to for every RequestKind not given bespoke handling.
struct SimpleSensorInfo {
  RequestKind kind;
  uint8_t id;
  SimpleValueKind value_kind;
  sensor::Sensor *OpenTherm42Hub::*member;
  const char *log_name;
};

// Every schedulable id -- including STATUS/CONTROL_SETPOINT and the startup-only ids -- is one
// Entry, scanned by pull_next_due_entry_(). There is no more separate startup-gate/reserved-heartbeat
// tier at the C++ level: `update_every` means "due once every N passes" (an entry is due on pass P
// iff `pass_counter_ % update_every == 0` -- see entries_'s declaration comment), and every id
// differs from every other one only in which cadence it's configured with. Since 0 % N == 0 for
// any N, every entry is due on pass 0, which is what makes the sweep-duration sensor's math work
// with no extra bookkeeping (see sweep_length_passes_'s declaration comment). `dirty` is set by
// control()-driven write ids (via set_write_value()/set_sensor_feed_write_value()) to jump the
// queue immediately (ASAP) rather than waiting for the entry's next due pass -- see
// build_next_request_()'s ASAP scan. `consecutive_data_invalid` is only meaningful for kinds
// should_invalidate_now_() gates (not TSP/FHB, whose slots share one RequestKind across many
// independent data-ids, so a single per-kind counter can't distinguish which slot last succeeded).
struct Entry {
  RequestKind kind;
  uint32_t update_every{1};
  bool dirty{false};
  uint8_t consecutive_data_invalid{0};
};

// §5.3.2 Class 2, IDs 93/94/95: a brand-identification string, assembled one ASCII character per
// conversation (index in the request's HB, character count in the response's HB, character itself in
// the response's LB). 51 = the largest legal index (49, per the ID 93/94/95 table's HB range) plus one
// content byte plus a null terminator.
struct BrandRead {
  text_sensor::TextSensor *sensor{nullptr};
  uint8_t next_index{0};
  std::array<char, 51> buffer{};
};

// §5.3.6 Class 6: one user-configured transparent-boiler-parameter slot. TSP values are opaque and
// manufacturer-specific (the protocol has no idea what they mean), so unlike every other class there's
// no fixed set of ids to expose -- the user names and indexes whichever slots their boiler documents.
struct TspSlot {
  uint8_t data_id;  // 11 (main), 89 (ventilation/heat-recovery), or 106 (Solar Storage)
  uint8_t index;    // TSP-index, 0..255
  number::Number *number{nullptr};
};

// §5.3.7 Class 7: one user-configured fault-history-buffer slot. Like TSP, purely read-only here.
struct FhbSlot {
  uint8_t data_id;  // 13 (main), 91 (ventilation/heat-recovery), or 108 (Solar Storage)
  uint8_t index;    // FHB-index, 0..255
  sensor::Sensor *sensor{nullptr};
};

// Declares set_<name>_switch()/set_<name>_binary_sensor(), storing the pointer at a fixed bit position
// within one of the hub's FlagWriteBits/FlagReadBits members. Used for every flag8 byte's individual
// bits across every class -- see flag_bits.h.
#define OT42_FLAG_WRITE_BIT(name, byte, bit) \
  void set_##name##_switch(switch_::Switch *s) { this->byte.bits[bit] = s; }
#define OT42_FLAG_READ_BIT(name, byte, bit) \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *s) { this->byte.bits[bit] = s; }
// Declares set_<name>_number()/set_<name>_sensor()/set_<name>_binary_sensor()/set_<name>_select()
// for a standalone (non-flag-byte) entity backed by a single named member pointer. Number entities are
// always OpenTherm42Number or OpenTherm42SensorFeedNumber in practice, but the member only ever needs
// the generic number::Number interface (publish_state()/invalidate_entity()), so this deliberately takes
// the base pointer type rather than either concrete one -- hub.h must stay buildable for configs that
// don't use any opentherm42 number at all, so it can never include either subclass's header (both
// include hub.h themselves, for their hub-callback constructor -- same reasoning as TspSlot::number
// below). See set_write_value()/set_sensor_feed_write_value() for how the hub instead receives values
// pushed from those constructors, without needing to read anything back off the entity.
#define OT42_SET_NUMBER(name, member) \
  void set_##name##_number(number::Number *n) { this->member = n; }
#define OT42_SET_SENSOR(name, member) \
  void set_##name##_sensor(sensor::Sensor *s) { this->member = s; }
#define OT42_SET_BINARY_SENSOR(name, member) \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *s) { this->member = s; }
#define OT42_SET_SELECT(name, member) \
  void set_##name##_select(select::Select *s) { this->member = s; }
// For a standalone (non-flag-byte) switch, e.g. one packed into the same byte as other non-flag
// fields (Class 8, ID 99's Manual DHW push2 bit) where OT42_FLAG_WRITE_BIT's whole-byte FlagWriteBits
// doesn't apply.
#define OT42_SET_SWITCH(name, member) \
  void set_##name##_switch(switch_::Switch *s) { this->member = s; }
// For the indexed-character-read accumulator struct (see IndexedStringRead below) used by the
// Class 2 brand-identification strings.
#define OT42_SET_TEXT_SENSOR(name, member) \
  void set_##name##_text_sensor(text_sensor::TextSensor *s) { this->member.sensor = s; }
// For a plain, standalone text_sensor pointer -- e.g. a small named-enum code shown as its spec
// name instead of a raw integer.
#define OT42_SET_PLAIN_TEXT_SENSOR(name, member) \
  void set_##name##_text_sensor(text_sensor::TextSensor *s) { this->member = s; }

// OpenTherm 4.2 master, implementing the OT/+ (OpenTherm/plus) digital protocol only -- §1.2: the
// two-microprocessor variant, as opposed to OT/- (OpenTherm/Lite), the PWM-signal variant for
// analogue-only products. §6 notes OT/- has been demoted to legacy/reference-only status since v4.1
// and should not be used in new designs -- this component never implements it. Talks directly to a
// single boiler -- see the OpenTherm Protocol Specification v4.2, §4.3.2: this component implements
// the master role only, not the optional gateway (chained intermediate device) role.
class OpenTherm42Hub : public Component {
 public:
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

  // How many consecutive DATA_INVALID responses (§4.4.1: "the data ID is recognised... but the data
  // requested is not available or invalid") a given id may accumulate before its entity is actually
  // invalidated. Real hardware has been observed answering DATA_INVALID for a data-id a handful of
  // times before reverting to a normal READ_ACK/WRITE_ACK, with no apparent cause -- invalidating on
  // every single DATA_INVALID made affected entities flap to Unknown and back constantly. 0 (the
  // default) means invalidate immediately, matching the behavior before this option existed.
  // Deliberately does NOT apply to UNKNOWN_DATA_ID (§4.4.1: the boiler doesn't recognise the data
  // identifier at all) or any datalink-level failure (timeout, frame error) -- those mean the boiler
  // has either never heard of this id or the bus itself is unreliable, neither of which this grace
  // period is meant to paper over. See should_invalidate_now_().
  void set_max_data_invalid(uint32_t max_data_invalid) { this->max_data_invalid_ = max_data_invalid; }
  // Synthetic diagnostic entity -- see sweep_length_passes_'s declaration comment. Not tied to any
  // real OpenTherm data-id, unconditionally available regardless of what else is configured.
  // Deliberately not named with the sensor_and_informational_data_* prefix used elsewhere in this
  // file, since that prefix names a real spec chapter (§5.3.4 Class 4) this entity has nothing to
  // do with.
  OT42_SET_SENSOR(sweep_duration, sweep_duration_sensor_)
  // Same nature as the sweep duration sensor above, but for a single pass (one full scan of
  // entries_, however many of them happened to be due on it) rather than a whole sweep -- see
  // pass_start_ms_'s declaration comment.
  OT42_SET_SENSOR(pass_duration, pass_duration_sensor_)
  // Synthetic diagnostic entity: true if any conversation during the most recently completed sweep
  // was rejected or failed at the datalink level (timeout, Manchester/parity/stop-bit error,
  // DATA_INVALID, UNKNOWN_DATA_ID, or an unexpected message type) -- including ones
  // should_invalidate_now_() chose not to actually invalidate an entity for (a DATA_INVALID within
  // max_data_invalid's grace period), since those are exactly the kind of error that's otherwise
  // only visible in the log. See sweep_had_error_'s declaration comment for how this is tracked.
  OT42_SET_BINARY_SENSOR(sweep_had_errors, sweep_had_errors_binary_sensor_)

  // §5.2's mandatory heartbeat (id=0) -- unconditionally required in config (see
  // opentherm42/__init__.py), since STATUS is unconditionally scheduled regardless of which, if
  // any, of its 15 switch/binary_sensor bits are configured. Called at Python wiring time, before
  // any component's setup() (including this hub's own) -- entries_ doesn't exist yet at that point
  // (it's populated by build_schedule_(), which only runs from this hub's own setup()), so stage
  // the value in pending_group_update_every_ like every other group option below, rather than
  // looking up an Entry immediately.
  void set_control_and_status_information_boiler_status_update_every(uint32_t update_every) {
    this->pending_group_update_every_.emplace_back(RequestKind::STATUS, update_every);
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // §5.3.1 Class 1, IDs 1/8/71: numeric setpoints.
  OT42_SET_NUMBER(control_and_status_information_control_setpoint, control_setpoint_number_)
  // Called by every OpenTherm42Number's control()/setup() (every write-capable number except ids
  // 24/27/37/38/78/79 -- see set_sensor_feed_write_value() for those) to push the value that
  // build_next_request_() should send next for that data-id, and mark its Entry dirty so it's sent
  // immediately (ASAP) rather than waiting for its next due pass. Every id handled here is already
  // unconditionally scheduled from build_schedule_() (each one always has a real value by the time
  // build_next_request_() can run, thanks to initial_value/flash restore -- see OpenTherm42Number's
  // class comment), so there's no dynamic scheduling to do here, just dirtying.
  void set_write_value(uint8_t id, float value);
  // Called from OpenTherm42Number::setup() (every id set_write_value() handles) once hub_ is
  // guaranteed to have already run build_schedule_() -- see set_sensor_feed_update_every()'s
  // declaration comment for why this can't happen at wiring time instead. The three R/W pairs
  // (56/57/87) update their READ side's Entry, not the write side's -- update_every is
  // conceptually about how often the boiler's own answer gets refreshed, and the write side is
  // otherwise driven by the ASAP dirty bit on every control() plus its own steady-state cadence as
  // a fallback, same as everything else.
  void set_number_update_every(uint8_t id, uint32_t update_every);

  // Generic counterpart for every id dispatched through the SIMPLE_SENSORS table (see
  // find_simple_sensor_by_id_()) -- one shared setter instead of ~44 individually-named ones, since
  // they all funnel into the exact same id-keyed lookup mechanism already. Called at wiring time
  // (sensor.new_sensor() doesn't go through cg.register_component()) -- staged here and consumed
  // once by build_schedule_()'s SIMPLE_SENSORS loop, same reasoning as the bespoke setters above.
  void set_simple_sensor_update_every(uint8_t id, uint32_t update_every) {
    this->pending_simple_sensor_update_every_.emplace_back(id, update_every);
  }

  // Called by OpenTherm42SensorFeedNumber::control() (ids 24/27/37/38/78/79 only) every time the user
  // commands a new value. The first call for a given id adds its WRITE RequestKind to the schedule,
  // since before that there's nothing legitimate to send -- see that class's comment for why these
  // ids get no config-time default. Later calls just update the value.
  void set_sensor_feed_write_value(uint8_t id, float value);
  // Called from OpenTherm42SensorFeedNumber::setup() (ids 27/38/78/79 only -- 24/37 have no
  // READ-DATA counterpart, so update_every isn't exposed in their config at all, see that class's
  // set_update_every()) once hub_ is guaranteed to have already run build_schedule_()
  // (setup_priority::HARDWARE above beats every entity's default setup_priority::DATA). Updates the
  // READ side's Entry -- that's the conversation a cadence actually governs here, per the same
  // reasoning as set_number_update_every() above.
  void set_sensor_feed_update_every(uint8_t id, uint32_t update_every);

  // §5.3.4 Class 4: the 15 counter/hour ids above are all "R W" with reset-by-writing-zero optional for
  // the boiler. Called by OpenTherm42ResetCounterButton::press_action() with the data-id its config
  // maps to; queued on demand, same priority tier as Class 3's remote requests and Class 6's TSP
  // writes. ID 98 (RF sensor status information) and ID 111 (Electricity production, read-only, can't
  // be reset) are intentionally not part of this list.
  void reset_counter(uint8_t data_id) {
    this->reset_counter_pending_ = true;
    this->reset_counter_data_id_ = data_id;
  }

 protected:
  // §4.3.1: minimum time between the end of one conversation and the start of the next.
  static constexpr uint32_t MASTER_WAIT_TIME_MS = 100;
  // §4.3.1: the legal boiler answering-time window is 20-400 ms from the end of the master's
  // transmission; 400 ms is the longest a compliant boiler is allowed to take.
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 400;

  // Populates entries_ from whichever entities got configured -- called once from setup(). Also
  // computes sweep_length_passes_ (the max update_every across every entry) once entries_ is
  // final -- see that field's declaration comment.
  void build_schedule_();
  // Builds the next request to send: the one-off pending_ intercepts first (unchanged from before
  // this redesign), then an ASAP scan over every entry for a dirty write, then
  // pull_next_due_entry_()'s pass-based pull. loop() sends whatever this returns every single time
  // it's called (gated only by MASTER_WAIT_TIME_MS), so there is always something to send -- if
  // pull_next_due_entry_() finds nothing due, STATUS is resent as a filler (always a safe,
  // spec-legitimate thing to send, and immediate, so §4.3.1's 1.15 s MCI ceiling is never at risk).
  Frame build_next_request_();
  // Builds the frame for a given entry's RequestKind -- the per-kind switch every caller (the ASAP
  // scan, the MCI filler, and the ordinary pass-pull) shares, since the frame is identical either
  // way; only the reason for sending it differs.
  Frame build_entry_request_(RequestKind kind);
  // Appends a new entry with the given cadence (default 1, i.e. due every pass -- used for kinds
  // seeded unconditionally in build_schedule_() before their real cadence is staged/applied) -- see
  // Entry's declaration comment.
  void add_entry_(RequestKind kind, uint32_t update_every = 1);
  // Linear scan over entries_ for the given kind -- small enough (a few dozen entries at most) that
  // a linear scan is simpler and cheaper than any indexed lookup. Returns nullptr if not (yet)
  // scheduled (only possible for the sensor-feed write ids before their first real value arrives).
  Entry *find_entry_(RequestKind kind);
  // Scans forward from cursor_ for the next entry due on pass_counter_, advancing cursor_ past
  // every entry it checks. When cursor_ reaches entries_.size() with the current pass's due-list
  // fully drained, resets cursor_ to 0, increments pass_counter_, and checks the sweep boundary
  // (see sweep_length_passes_/sweep_start_ms_) before continuing to scan the new pass's due-list in
  // the same call -- bounded to one extra lap, so pass_counter_ never advances more than once per
  // call. Returns an empty optional if a full lap finds nothing due at all (only possible if no
  // active entry has update_every == 1) -- build_next_request_() falls back to a STATUS filler in
  // that case; the very next call resumes from the new pass_counter_, so a sparse schedule like
  // this self-corrects within a few calls rather than needing to search further ahead here.
  optional<Frame> pull_next_due_entry_();
  // Interprets a received frame according to which request it answers; logs and discards it if the
  // boiler replied with a message type that isn't legal for that data-id. Dispatches to the three
  // handlers below (split out of this function to stay under clang-tidy's statement-count limit),
  // then falls back to the generic SIMPLE_SENSORS handling for every kind none of them claimed.
  void handle_response_(const Frame &frame);
  // §5.3.1/§5.3.2 Class 1/2: boiler status/fault/ventilation/solar-storage flags and this
  // component's own announced identity (MASTER_CONFIG/MASTER_OPENTHERM_VERSION/
  // MASTER_PRODUCT_VERSION) and the boiler's reported product/version identification. Returns
  // false (handling nothing) for any other kind, so handle_response_() can try the next handler.
  bool handle_response_status_and_identity_(const Frame &frame, MessageType type);
  // Brand strings, Class 3 remote-request feedback, the master-provided sensor-feed numbers
  // (room/outside temperature, humidity, CO2), and the Day-of-week/Time/Date/Year read & write
  // pairs. Same false-for-unclaimed-kinds contract as the handler above.
  bool handle_response_feeds_and_time_(const Frame &frame, MessageType type);
  // Boiler fan speed, Class 5 remote-parameter flags/bounds and DHW/CH/ventilation setpoints,
  // TSP/FHB slot round-robins, cooling control, and the Class 8 remote-override entities. Same
  // false-for-unclaimed-kinds contract as the handlers above.
  bool handle_response_setpoints_and_parameters_(const Frame &frame, MessageType type);
  // On a failed conversation, every read-only entity that conversation would have updated must show
  // unknown rather than keep stale data.
  void invalidate_response_(RequestKind kind);
  // Whether a rejected conversation should actually invalidate its entity/entities right now -- see
  // set_max_data_invalid()'s declaration comment for the reasoning. Mutates the matching entry's
  // consecutive_data_invalid counter (incrementing on DATA_INVALID, which is why this isn't const).
  // Every handle_response_() rejection branch (except TSP/FHB, whose slots share one RequestKind
  // across many independent data-ids, so a single per-kind counter can't distinguish which slot
  // last succeeded) gates its invalidate_*() call(s) on this.
  bool should_invalidate_now_(RequestKind kind, MessageType type);
  // Formats a short "Name (id=N)"-style description of the given request kind into buf, for the raw
  // datalink error log in loop() -- that log fires before any frame is parsed, so unlike
  // handle_response_()/invalidate_response_() it has no message-type context of its own to name the
  // failed conversation by.
  void describe_request_kind_(RequestKind kind, char *buf, size_t buf_len) const;
  // Debug instrumentation: logs every outgoing frame build_next_request_() produces (one call site
  // per early-return branch plus the main switch's tail) and every incoming frame
  // handle_response_() receives, in each case naming the RequestKind (via describe_request_kind_()
  // above), the message type (via message_type_to_string()), and the raw id/HB/LB bytes -- lets the
  // wire communication be cross-checked against what the spec requires without a logic analyzer.
  void log_outgoing_frame_(const Frame &frame) const;
  // Looks up a single-value, non-bit-decomposed read-only sensor's data-id/sensor pointer/log name --
  // the fallback every class after Class 1 dispatches "plain" reads through. Returns nullptr for kinds
  // with bespoke handling (bit-decomposed, write-only, dual-mode, ...).
  const SimpleSensorInfo *find_simple_sensor_(RequestKind kind) const;
  // Same lookup, keyed by data-id instead of RequestKind -- reset_counter() only knows the id its
  // button config maps to, not the RequestKind, so it needs this to find the matching table entry.
  const SimpleSensorInfo *find_simple_sensor_by_id_(uint8_t id) const;
  // Defined (sized) in hub.cpp: its pointer-to-member entries need this class complete, and an
  // out-of-class member definition has the same access to protected members as a member function does.
  static const SimpleSensorInfo SIMPLE_SENSORS[];

  InternalGPIOPin *in_pin_{nullptr};
  InternalGPIOPin *out_pin_{nullptr};

  std::unique_ptr<OpenThermDataLink> datalink_;

  uint32_t last_conversation_end_ms_{0};
  RequestKind pending_request_kind_{RequestKind::STATUS};

  // See set_max_data_invalid()'s declaration comment. 0 disables the grace period (default).
  uint32_t max_data_invalid_{0};

  // Every schedulable id -- see Entry's declaration comment. Populated once at setup() (plus the
  // rare sensor-feed write ids' one-time dynamic append), so std::vector's growth never touches the
  // heap again afterward in the common case.
  std::vector<Entry> entries_;
  // Which pass the scheduler is currently on, and where within entries_ the current pass's due-list
  // scan has reached -- see pull_next_due_entry_(). Both start at 0, meaning pass 0 (every entry is
  // due, since P % N == 0 whenever P == 0) begins at the very first scheduling decision.
  uint32_t pass_counter_{0};
  size_t cursor_{0};
  // One sweep = enough passes for every currently-configured entry to be attempted at least once.
  // Since every entry is due on pass 0 and an entry with update_every=N is next due at pass N, the
  // slowest entry (the one with the largest update_every) defines how long a sweep is -- computed
  // once in build_schedule_(), since update_every values are fixed at config time. A sweep boundary
  // (pass_counter_ a multiple of this) is therefore also exactly when the slowest entry becomes due
  // again -- nothing extra to detect. "Sweep complete" means every entry was attempted at least
  // once, not necessarily answered successfully -- a DATA_INVALID/timeout just waits for its own
  // next due pass, same as any other outcome (see should_invalidate_now_()).
  uint32_t sweep_length_passes_{1};
  uint32_t sweep_start_ms_{0};
  sensor::Sensor *sweep_duration_sensor_{nullptr};
  // Start time of the pass currently in progress -- reset every time pull_next_due_entry_() wraps
  // cursor_ back to 0 (i.e. every pass_counter_ increment), regardless of whether that pass also
  // happened to cross a sweep boundary. Unlike sweep_start_ms_, this always advances once per pass.
  uint32_t pass_start_ms_{0};
  sensor::Sensor *pass_duration_sensor_{nullptr};
  // Set by OT42_LOG_REJECTION()/OT42_LOG_REJECTION_ALWAYS() (every rejected conversation) and by
  // loop()'s DataLinkState::ERROR case (every datalink-level failure), accumulating across the
  // sweep currently in progress. Published to sweep_had_errors_binary_sensor_ and reset to false at
  // every sweep boundary (see pull_next_due_entry_()), so it always reflects only the most recently
  // *completed* sweep, same lifecycle as sweep_duration_sensor_.
  bool sweep_had_error_{false};
  binary_sensor::BinarySensor *sweep_had_errors_binary_sensor_{nullptr};

  // Staged by set_simple_sensor_update_every() at wiring time, consumed once by build_schedule_()'s
  // SIMPLE_SENSORS loop, then cleared -- only needed transiently during setup(), so shrink_to_fit()
  // afterward gives the memory back rather than holding it forever.
  std::vector<std::pair<uint8_t, uint32_t>> pending_simple_sensor_update_every_;
  // Staged by every hub-level group's set_..._update_every() at wiring time (entries_ doesn't exist
  // yet when those run -- see their declaration comments above), consumed once by build_schedule_()
  // after entries_ is built, then cleared the same way as pending_simple_sensor_update_every_ above.
  std::vector<std::pair<RequestKind, uint32_t>> pending_group_update_every_;

  // Raw values from the §5.2 mandatory conversations -- exposed as real entities once Class 2
  // (Commit 5) lands.
  uint8_t boiler_status_{0};

  // §5.3.1 Class 1 entities.
  FlagWriteBits master_status_write_;
  FlagReadBits boiler_status_read_;

  number::Number *control_setpoint_number_{nullptr};
  // §5.3.1 Class 1, IDs 1/8/71 (write side): see set_write_value()'s declaration comment.
  float control_setpoint_write_value_{0};

  bool reset_counter_pending_{false};
  uint8_t reset_counter_data_id_{0};
};

}  // namespace esphome::opentherm42
