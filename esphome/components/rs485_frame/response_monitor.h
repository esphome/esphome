#pragma once

// defines.h must come before the #ifdef gate so a translation unit that opens this header
// without first including a core component header (e.g., response_monitor.cpp itself) still
// sees USE_RS485_FRAME_RESPONSE_MONITOR before the conditional is evaluated.
#include "esphome/core/defines.h"

#ifdef USE_RS485_FRAME_RESPONSE_MONITOR

#include "esphome/core/helpers.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace esphome::rs485_frame {

// Caps for response_monitor:/response_fields: schema surface (live trigger -> response
// confirmation). Separate from on_frame's MAX_FRAME_TYPE_LEN/MAX_FRAME_TYPE_ALTS:
// response_fields addresses can be longer prefixes (the 04 0A container's combined
// LED+display anchor is 14 bytes) and a response_monitor trigger can be a full resolved
// button command (preamble + up to MAX_COMMAND_VALUES 4-byte values + postamble = worst
// case 8 + 32 + 8 = 48 bytes), not just a 2-byte frame_type. Bump alongside the matching
// Python schema caps in __init__.py if these are ever raised.
//
// These are validation limits, not reservations: the field, entry, signature, trigger and
// text-value tables are all FixedVectors sized from the counts codegen passes in, so a build
// only pays for what its YAML declares. Only the small per-field prefix/mask buffers and the
// per-alt int values stay inline at their cap.
static constexpr size_t MAX_RESPONSE_FIELD_PREFIX_LEN = 16;
static constexpr size_t MAX_RESPONSE_FIELD_LEN = 40;
static constexpr size_t MAX_RESPONSE_FIELDS = 16;
static constexpr size_t MAX_RESPONSE_TRIGGER_LEN = 48;
static constexpr size_t MAX_RESPONSE_MONITOR_ENTRIES = 16;
static constexpr size_t MAX_SIGNATURE_ALTS = 4;
static constexpr size_t MAX_MASKED_INT_VALUES = 4;
static constexpr size_t MAX_TEXT_ENUM_VALUES = 8;
static constexpr size_t MAX_TEXT_ENUM_LEN = 32;

/// The four response_monitor: signature modes.
enum SignatureType {
  SIGNATURE_TYPE_MASKED_INT,     ///< Exact match against one or more known absolute values.
  SIGNATURE_TYPE_TEXT_ENUM,      ///< Exact match against a small set of known ASCII strings.
  SIGNATURE_TYPE_CHANGED,        ///< Byte range (optionally masked) differs from its pre-trigger value.
  SIGNATURE_TYPE_CHANGED_GATED,  ///< Same as CHANGED, but only evaluated when a gate field matches at trigger time.
};

/// Per-entry occurrence outcomes, one counter each, exposed to the sensor platform.
enum ResponseMonitorStat {
  RESPONSE_MONITOR_STAT_SUCCESS,         ///< Signature matched within the window.
  RESPONSE_MONITOR_STAT_FAIL,            ///< The addressed field arrived but never matched the signature.
  RESPONSE_MONITOR_STAT_TIMEOUT,         ///< Window elapsed with no RX of the addressed field at all.
  RESPONSE_MONITOR_STAT_NOT_APPLICABLE,  ///< changed_gated: no alt's gate matched at trigger time; not armed.
  RESPONSE_MONITOR_STAT_ORPHAN,          ///< Signature matched while no trigger for this entry was pending.
};

/// One named `response_fields:` entry. Pure offset/mask/endian addressing — names are
/// resolved to indices in Python at compile time (so a field declared once and shared by
/// several `response_monitor:` entries costs nothing extra at runtime), so this struct
/// carries no name at runtime.
struct ResponseField {
  StaticVector<uint8_t, MAX_RESPONSE_FIELD_PREFIX_LEN> frame_type;
  // Same length as frame_type, always populated (Python fills an all-0xFF default when
  // frame_type_mask: is omitted, so field_matches_ never needs to special-case "no mask").
  StaticVector<uint8_t, MAX_RESPONSE_FIELD_PREFIX_LEN> frame_type_mask;
  uint8_t offset{0};
  uint8_t length{0};
  bool big_endian{true};
  // Continuously-refreshed ambient snapshot of this field's bytes, used by `changed`/
  // `changed_gated`. Updated on every matching RX regardless of whether any entry is
  // currently pending -- the same bookkeeping a changed_from_ambient filter would need,
  // generalized from a single sensor to an arbitrary byte range.
  std::array<uint8_t, MAX_RESPONSE_FIELD_LEN> ambient{};
  bool ambient_valid{false};
};

/// One alternate within a response_monitor entry's `signature:` list.
struct SignatureAlt {
  uint8_t field_index{0};
  SignatureType type{SIGNATURE_TYPE_CHANGED};
  uint32_t mask{0xFFFFFFFF};                                 ///< masked_int / changed / changed_gated
  StaticVector<uint32_t, MAX_MASKED_INT_VALUES> int_values;  ///< masked_int
  // text_enum targets. Points at the string literals codegen emits (static storage duration),
  // so each value costs one pointer instead of a MAX_TEXT_ENUM_LEN buffer.
  FixedVector<const char *> text_values;
  // changed_gated only:
  bool has_gate{false};
  uint8_t gate_field_index{0};
  uint32_t gate_mask{0xFFFFFFFF};
  uint32_t gate_value{0};
  // Set the first time gate_active_() finds the gate field never observed at all (distinct
  // from "observed but doesn't currently hold") -- gates the one-time warning log, since that
  // condition otherwise never resolves and would log every single trigger forever.
  bool gate_unobserved_logged{false};
};

/// One `response_monitor:` entry: a trigger matched against outgoing TX payloads (the same
/// prefix match RS485FrameHub already uses for tx.gate.frame_type), a timeout window, and an
/// ordered signature (first alternate to evaluate true within the window wins).
struct ResponseMonitorEntry {
  FixedVector<uint8_t> trigger;
  uint32_t window_ms{0};
  FixedVector<SignatureAlt> signature;  ///< Sized to the entry's declared alt count by add_entry.

  // --- Runtime pending state, mutated only by ResponseMonitor ---
  bool pending{false};
  uint32_t deadline{0};
  bool saw_any_match{false};  ///< At least one RX matched this entry's addressed field(s) since arming.
  // Per-alt "is this alt active this arm cycle" bitmask (bit j = signature[j]) — changed_gated's
  // gate is evaluated once, at arm time, against the field's live ambient value ("reads a
  // specific masked value at trigger time"). MAX_SIGNATURE_ALTS fits in 8 bits.
  uint8_t alt_active{0};

  uint32_t success_count{0};
  uint32_t fail_count{0};
  uint32_t timeout_count{0};
  uint32_t not_applicable_count{0};
  uint32_t orphan_count{0};
};

/// Owns the response_fields:/response_monitor: config and the live trigger -> response
/// matching state machine: core signature matching, the success/fail/timeout/
/// not_applicable/orphan counters exposed to the sensor platform, and the on_confirmed:/
/// on_failed: automation callbacks. An optional retry consumer hooking on_failed: is a later,
/// separate addition — not implemented here.
class ResponseMonitor {
  static_assert(MAX_SIGNATURE_ALTS <= 8, "ResponseMonitorEntry::alt_active is an 8-bit mask");

 public:
  // Sizes the field and entry tables to exactly what the YAML declared. Called once from
  // to_code (via RS485FrameHub::enable_response_monitor) before any add_field/add_entry;
  // add_* calls beyond these counts are dropped.
  void init(size_t field_count, size_t entry_count);

  // Declares one response_fields: entry. Called once per field from to_code, in YAML
  // declaration order — that order IS the field's index (field: names are resolved to
  // indices in Python; see add_*_alt's field_index argument).
  void add_field(const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &frame_type_mask, uint8_t offset,
                 uint8_t length, bool big_endian);

  // Declares one response_monitor: entry (trigger + window) with room for alt_count
  // signature alternates. Returns its index for subsequent add_*_alt calls. trigger is the
  // fully-resolved on-wire trigger bytes (button_id: resolved in Python at compile time, or
  // the literal frame_type: prefix).
  uint8_t add_entry(const std::vector<uint8_t> &trigger, uint32_t window_ms, uint8_t alt_count);

  // Appends one signature alternate to entry_index's signature: list, in YAML order.
  void add_masked_int_alt(uint8_t entry_index, uint8_t field_index, uint32_t mask, const std::vector<uint32_t> &values);
  // values must outlive the monitor (codegen passes string literals).
  void add_text_enum_alt(uint8_t entry_index, uint8_t field_index, std::initializer_list<const char *> values);
  void add_changed_alt(uint8_t entry_index, uint8_t field_index, uint32_t mask);
  void add_changed_gated_alt(uint8_t entry_index, uint8_t field_index, uint32_t mask, uint8_t gate_field_index,
                             uint32_t gate_mask, uint32_t gate_value);

  // Called from RS485FrameHub::write_frame_ with the payload-relative view of every frame
  // actually transmitted (mirrors extract_tx_payload_'s output). Arms any entry whose
  // trigger is a prefix of payload; a changed_gated-only entry whose gate does not match any
  // alt at this moment resolves immediately as NOT_APPLICABLE instead of arming.
  void on_trigger_sent(const std::vector<uint8_t> &payload, uint32_t now);

  // Called from RS485FrameHub::process_raw_frame_ after validate_frame_() accepts an RX
  // frame, with the same payload on_frame: triggers see. Refreshes ambient snapshots and
  // resolves pending entries whose signature matches; also counts orphans (a signature match
  // observed while no trigger for that entry was pending).
  void on_frame_received(const std::vector<uint8_t> &payload, uint32_t now);

  // Called every loop() with the current time; resolves entries whose window has elapsed —
  // FAIL if the addressed field was seen at least once during the window (wrong signature),
  // TIMEOUT if it was never seen at all.
  void process_timeouts(uint32_t now);

  uint32_t get_stat(uint8_t entry_index, ResponseMonitorStat stat) const;

  // Registers a callback fired when entry_index resolves SUCCESS (on_confirmed:) or
  // FAIL/TIMEOUT (on_failed:) -- never on NOT_APPLICABLE (an expected "gate didn't hold this
  // time", not a failure) or ORPHAN (no trigger from this entry was even pending). Templated
  // per this component's callback-registration convention (accepts std::function and
  // pointer-sized forwarder structs alike; see build_callback_automation).
  template<typename F> void add_on_confirmed_callback(uint8_t entry_index, F &&callback) {
    if (entry_index < this->on_confirmed_callbacks_.size())
      this->on_confirmed_callbacks_[entry_index].add(std::forward<F>(callback));
  }
  template<typename F> void add_on_failed_callback(uint8_t entry_index, F &&callback) {
    if (entry_index < this->on_failed_callbacks_.size())
      this->on_failed_callbacks_[entry_index].add(std::forward<F>(callback));
  }

 protected:
  bool field_matches_(const ResponseField &field, const std::vector<uint8_t> &payload, uint8_t *out,
                      uint8_t &out_len) const;
  static uint32_t decode_int(const uint8_t *bytes, uint8_t length, bool big_endian);
  static uint8_t decode_text(const uint8_t *bytes, uint8_t length, char *out);
  bool eval_alt_(const SignatureAlt &alt, const uint8_t *old_bytes, const uint8_t *new_bytes, uint8_t len,
                 bool big_endian) const;
  // Appends a new alt to entry_index's signature, or returns nullptr when the index is out of
  // range or the entry already holds the alt_count it was declared with.
  SignatureAlt *next_alt_(uint8_t entry_index);
  // Evaluates whether a changed_gated alt's gate currently matches (using the live ambient
  // value of its gate field); non-gated alts are always active. Shared by on_trigger_sent
  // (arm time) and on_frame_received's orphan path (which has no arm time of its own, so it
  // uses "now" as a stand-in for "trigger time"). Takes a mutable alt (not const) because it
  // latches gate_unobserved_logged the first time the gate field turns out to have never been
  // observed at all.
  bool gate_active_(SignatureAlt &alt) const;
  // entry_index (rather than a ResponseMonitorEntry& as before on_confirmed_/on_failed_
  // callbacks existed) so this can fire the index-aligned callback for SUCCESS/FAIL/TIMEOUT.
  void resolve_entry_(uint8_t entry_index, ResponseMonitorStat stat);

  FixedVector<ResponseField> fields_;
  FixedVector<ResponseMonitorEntry> entries_;
  // Index-aligned with entries_ (grown in lockstep by add_entry). LazyCallbackManager costs
  // one pointer per entry even when on_confirmed:/on_failed: are never declared for it.
  FixedVector<LazyCallbackManager<void()>> on_confirmed_callbacks_;
  FixedVector<LazyCallbackManager<void()>> on_failed_callbacks_;
};

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_RESPONSE_MONITOR
