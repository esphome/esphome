#include "response_monitor.h"

#ifdef USE_RS485_FRAME_RESPONSE_MONITOR

#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::rs485_frame {

static const char *const TAG = "rs485_frame.response_monitor";

void ResponseMonitor::init(size_t field_count, size_t entry_count) {
  this->fields_.init(field_count);
  this->entries_.init(entry_count);
  this->on_confirmed_callbacks_.init(entry_count);
  this->on_failed_callbacks_.init(entry_count);
}

void ResponseMonitor::add_field(const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &frame_type_mask,
                                uint8_t offset, uint8_t length, bool big_endian) {
  if (this->fields_.full()) {
    ESP_LOGE(TAG, "response_fields: more fields added than declared (%zu)", this->fields_.capacity());
    return;
  }
  ResponseField &field = this->fields_.emplace_back();
  field.frame_type.assign(frame_type.begin(), frame_type.end());
  field.frame_type_mask.assign(frame_type_mask.begin(), frame_type_mask.end());
  field.offset = offset;
  field.length = length;
  field.big_endian = big_endian;
}

uint8_t ResponseMonitor::add_entry(const std::vector<uint8_t> &trigger, uint32_t window_ms, uint8_t alt_count) {
  if (this->entries_.full()) {
    ESP_LOGE(TAG, "response_monitor: more entries added than declared (%zu)", this->entries_.capacity());
    return UINT8_MAX;  // every add_*_alt/get_stat/callback call range-checks the index
  }
  ResponseMonitorEntry &entry = this->entries_.emplace_back();
  entry.trigger.init(trigger.size());
  for (uint8_t b : trigger)
    entry.trigger.push_back(b);
  entry.window_ms = window_ms;
  entry.signature.init(std::min<size_t>(alt_count, MAX_SIGNATURE_ALTS));
  // Keep the callback vectors index-aligned with entries_.
  this->on_confirmed_callbacks_.emplace_back();
  this->on_failed_callbacks_.emplace_back();
  return static_cast<uint8_t>(this->entries_.size() - 1);
}

SignatureAlt *ResponseMonitor::next_alt_(uint8_t entry_index) {
  if (entry_index >= this->entries_.size())
    return nullptr;
  FixedVector<SignatureAlt> &signature = this->entries_[entry_index].signature;
  if (signature.full()) {
    ESP_LOGE(TAG, "response_monitor entry %u: more signature alts added than declared (%zu)", entry_index,
             signature.capacity());
    return nullptr;
  }
  return &signature.emplace_back();
}

void ResponseMonitor::add_masked_int_alt(uint8_t entry_index, uint8_t field_index, uint32_t mask,
                                         const std::vector<uint32_t> &values) {
  SignatureAlt *alt = this->next_alt_(entry_index);
  if (alt == nullptr)
    return;
  alt->field_index = field_index;
  alt->type = SIGNATURE_TYPE_MASKED_INT;
  alt->mask = mask;
  for (uint32_t v : values)
    alt->int_values.push_back(v);
}

void ResponseMonitor::add_text_enum_alt(uint8_t entry_index, uint8_t field_index,
                                        std::initializer_list<const char *> values) {
  SignatureAlt *alt = this->next_alt_(entry_index);
  if (alt == nullptr)
    return;
  alt->field_index = field_index;
  alt->type = SIGNATURE_TYPE_TEXT_ENUM;
  alt->text_values.init(values.size());
  for (const char *v : values)
    alt->text_values.push_back(v);
}

void ResponseMonitor::add_changed_alt(uint8_t entry_index, uint8_t field_index, uint32_t mask) {
  SignatureAlt *alt = this->next_alt_(entry_index);
  if (alt == nullptr)
    return;
  alt->field_index = field_index;
  alt->type = SIGNATURE_TYPE_CHANGED;
  alt->mask = mask;
}

void ResponseMonitor::add_changed_gated_alt(uint8_t entry_index, uint8_t field_index, uint32_t mask,
                                            uint8_t gate_field_index, uint32_t gate_mask, uint32_t gate_value) {
  SignatureAlt *alt = this->next_alt_(entry_index);
  if (alt == nullptr)
    return;
  alt->field_index = field_index;
  alt->type = SIGNATURE_TYPE_CHANGED_GATED;
  alt->mask = mask;
  alt->has_gate = true;
  alt->gate_field_index = gate_field_index;
  alt->gate_mask = gate_mask;
  alt->gate_value = gate_value;
}

uint32_t ResponseMonitor::decode_int(const uint8_t *bytes, uint8_t length, bool big_endian) {
  // Numeric signature modes (masked_int, changed, changed_gated's gate) only ever address
  // fields carrying a command-sized value (<= 4 bytes, matching MAX_COMMAND_VALUES' element
  // width); a field declared longer than that (e.g. a display-text field) is only ever
  // meaningfully read by text_enum, so only the first 4 bytes are consulted here.
  uint8_t n = std::min<uint8_t>(length, 4);
  uint32_t v = 0;
  if (big_endian) {
    for (uint8_t i = 0; i < n; i++)
      v = (v << 8) | bytes[i];
  } else {
    for (int i = n - 1; i >= 0; i--)
      v = (v << 8) | bytes[i];
  }
  return v;
}

uint8_t ResponseMonitor::decode_text(const uint8_t *bytes, uint8_t length, char *out) {
  // Mirrors the ASCII-decode convention already used by this component's example on_frame
  // lambdas: strip the display's blink/inverse flag (bit 7) and drop non-printable bytes
  // (including the trailing NUL/pad). Trailing space padding is additionally trimmed here so
  // a fixed-width display field ("Auto Control   ") compares equal to its trimmed target
  // ("Auto Control") without requiring the YAML author to count padding spaces.
  // Scans the FULL field (up to MAX_RESPONSE_FIELD_LEN, the largest a field: can be), not
  // just MAX_TEXT_ENUM_LEN bytes of it — a 36-byte display field like the worked example's
  // display_04_0a_with_led has far more raw bytes than MAX_TEXT_ENUM_LEN, and filtering drops
  // most of them (non-printable) before the cap on OUTPUT length below ever matters.
  uint8_t n = 0;
  for (uint8_t i = 0; i < length && n < MAX_TEXT_ENUM_LEN; i++) {
    uint8_t b = bytes[i] & 0x7F;
    if (b < 0x20 || b > 0x7E)
      continue;
    out[n++] = static_cast<char>(b);
  }
  while (n > 0 && out[n - 1] == ' ')
    n--;
  out[n] = '\0';
  return n;
}

bool ResponseMonitor::field_matches_(const ResponseField &field, const std::vector<uint8_t> &payload, uint8_t *out,
                                     uint8_t &out_len) const {
  const size_t prefix_len = field.frame_type.size();
  if (payload.size() < prefix_len)
    return false;
  for (size_t i = 0; i < prefix_len; i++) {
    if (((payload[i] ^ field.frame_type[i]) & field.frame_type_mask[i]) != 0)
      return false;
  }
  if (payload.size() < size_t(field.offset) + field.length)
    return false;
  std::copy(payload.begin() + field.offset, payload.begin() + field.offset + field.length, out);
  out_len = field.length;
  return true;
}

bool ResponseMonitor::eval_alt_(const SignatureAlt &alt, const uint8_t *old_bytes, const uint8_t *new_bytes,
                                uint8_t len, bool big_endian) const {
  switch (alt.type) {
    case SIGNATURE_TYPE_MASKED_INT: {
      const uint32_t v = decode_int(new_bytes, len, big_endian) & alt.mask;
      for (uint32_t target : alt.int_values) {
        if (v == (target & alt.mask))
          return true;
      }
      return false;
    }
    case SIGNATURE_TYPE_TEXT_ENUM: {
      char text[MAX_TEXT_ENUM_LEN + 1];
      decode_text(new_bytes, len, text);
      for (const char *target : alt.text_values) {
        if (std::strcmp(text, target) == 0)
          return true;
      }
      return false;
    }
    case SIGNATURE_TYPE_CHANGED:
    case SIGNATURE_TYPE_CHANGED_GATED: {
      // Fields over 4 bytes (e.g. a 01 03/04 0A display-text field) can't be answered by
      // decode_int's 4-byte-truncated int compare -- fall back to a full-length byte
      // compare instead. mask: is only meaningful for a <=4-byte command-sized value (and is
      // rejected at config-validate time for longer fields), so it plays no role here.
      if (len > 4)
        return std::memcmp(old_bytes, new_bytes, len) != 0;
      const uint32_t old_v = decode_int(old_bytes, len, big_endian) & alt.mask;
      const uint32_t new_v = decode_int(new_bytes, len, big_endian) & alt.mask;
      return old_v != new_v;
    }
    default:
      return false;
  }
}

bool ResponseMonitor::gate_active_(SignatureAlt &alt) const {
  if (!alt.has_gate)
    return true;
  if (alt.gate_field_index >= this->fields_.size() || !this->fields_[alt.gate_field_index].ambient_valid) {
    // Gate field never observed at all -- distinct from "observed but doesn't currently
    // hold" (the common, expected case). Left unobserved forever, this alt resolves
    // NOT_APPLICABLE on every trigger indefinitely, indistinguishable from a gate that just
    // isn't holding right now -- log it once so a misconfigured/never-seen gate field is
    // diagnosable instead of silently masquerading as normal gating.
    if (!alt.gate_unobserved_logged) {
      ESP_LOGW(TAG, "changed_gated field %u: gate field %u has never been observed; treating gate as not held",
               alt.field_index, alt.gate_field_index);
      alt.gate_unobserved_logged = true;
    }
    return false;  // gate field never observed yet — treat as "precondition not met", not a match
  }
  const ResponseField &gate_field = this->fields_[alt.gate_field_index];
  const uint32_t v = decode_int(gate_field.ambient.data(), gate_field.length, gate_field.big_endian);
  return (v & alt.gate_mask) == (alt.gate_value & alt.gate_mask);
}

void ResponseMonitor::resolve_entry_(uint8_t entry_index, ResponseMonitorStat stat) {
  ResponseMonitorEntry &entry = this->entries_[entry_index];
  entry.pending = false;
  switch (stat) {
    case RESPONSE_MONITOR_STAT_SUCCESS:
      entry.success_count++;
      this->on_confirmed_callbacks_[entry_index].call();
      break;
    case RESPONSE_MONITOR_STAT_FAIL:
      entry.fail_count++;
      this->on_failed_callbacks_[entry_index].call();
      break;
    case RESPONSE_MONITOR_STAT_TIMEOUT:
      entry.timeout_count++;
      this->on_failed_callbacks_[entry_index].call();
      break;
    case RESPONSE_MONITOR_STAT_NOT_APPLICABLE:
      entry.not_applicable_count++;
      break;
    case RESPONSE_MONITOR_STAT_ORPHAN:
      entry.orphan_count++;
      break;
  }
}

void ResponseMonitor::on_trigger_sent(const std::vector<uint8_t> &payload, uint32_t now) {
  for (size_t idx = 0; idx < this->entries_.size(); idx++) {
    ResponseMonitorEntry &entry = this->entries_[idx];
    if (entry.trigger.empty() || payload.size() < entry.trigger.size())
      continue;
    if (!std::equal(entry.trigger.begin(), entry.trigger.end(), payload.begin()))
      continue;

    // A trigger firing while the entry is still pending from an earlier, unresolved
    // occurrence (a double-press or retry inside the window) must not silently overwrite
    // that occurrence's pending/deadline/saw_any_match — resolve it first, on the evidence
    // gathered so far, exactly as process_timeouts would if the window had simply elapsed.
    if (entry.pending) {
      this->resolve_entry_(idx, entry.saw_any_match ? RESPONSE_MONITOR_STAT_FAIL : RESPONSE_MONITOR_STAT_TIMEOUT);
    }

    entry.alt_active = 0;
    for (size_t j = 0; j < entry.signature.size(); j++) {
      if (this->gate_active_(entry.signature[j]))
        entry.alt_active |= static_cast<uint8_t>(1U << j);
    }
    if (entry.alt_active == 0) {
      // Every alt is a changed_gated whose gate does not currently hold — this occurrence is
      // "not applicable", not a failure, and the window is not armed at all.
      this->resolve_entry_(idx, RESPONSE_MONITOR_STAT_NOT_APPLICABLE);
      continue;
    }
    entry.pending = true;
    entry.deadline = now + entry.window_ms;
    entry.saw_any_match = false;
  }
}

void ResponseMonitor::on_frame_received(const std::vector<uint8_t> &payload, uint32_t now) {
  uint8_t new_bytes[MAX_RESPONSE_FIELD_LEN];
  for (size_t i = 0; i < this->fields_.size(); i++) {
    uint8_t len = 0;
    if (!this->field_matches_(this->fields_[i], payload, new_bytes, len))
      continue;
    ResponseField &field = this->fields_[i];
    const uint8_t *old_bytes = field.ambient_valid ? field.ambient.data() : new_bytes;

    for (size_t entry_idx = 0; entry_idx < this->entries_.size(); entry_idx++) {
      ResponseMonitorEntry &entry = this->entries_[entry_idx];
      for (size_t j = 0; j < entry.signature.size(); j++) {
        SignatureAlt &alt = entry.signature[j];
        if (alt.field_index != i)
          continue;

        if (entry.pending) {
          // Only an ACTIVE alt's field arriving counts as "the addressed field was seen" --
          // an inactive (gated-off) alt sharing this field_index is irrelevant this arm
          // cycle, and must not suppress a genuine TIMEOUT in favor of a misleading FAIL.
          if ((entry.alt_active & (1U << j)) == 0)
            continue;
          entry.saw_any_match = true;
          if (this->eval_alt_(alt, old_bytes, new_bytes, len, field.big_endian)) {
            this->resolve_entry_(entry_idx, RESPONSE_MONITOR_STAT_SUCCESS);
            break;  // entry is resolved; remaining alts for this entry need no evaluation
          }
        } else if (this->gate_active_(alt) && this->eval_alt_(alt, old_bytes, new_bytes, len, field.big_endian)) {
          // The signature this entry watches for just matched, but no trigger for it was
          // pending — e.g. someone else changed this state from the physical panel. Break
          // (not just continue) so a second alt sharing this field_index -- which would
          // independently evaluate true against the very same RX event -- does not
          // double-count one physical occurrence as two orphans.
          this->resolve_entry_(entry_idx, RESPONSE_MONITOR_STAT_ORPHAN);
          break;
        }
      }
    }

    // Refresh the ambient snapshot AFTER evaluation so `changed` compared against the
    // pre-update value, not the value this same RX just delivered.
    std::copy(new_bytes, new_bytes + len, field.ambient.begin());
    field.ambient_valid = true;
  }
  (void) now;
}

void ResponseMonitor::process_timeouts(uint32_t now) {
  for (size_t idx = 0; idx < this->entries_.size(); idx++) {
    ResponseMonitorEntry &entry = this->entries_[idx];
    if (!entry.pending)
      continue;
    // Unsigned subtraction wraps correctly across the 49-day millis rollover.
    if (now - entry.deadline < 0x80000000UL) {
      this->resolve_entry_(idx, entry.saw_any_match ? RESPONSE_MONITOR_STAT_FAIL : RESPONSE_MONITOR_STAT_TIMEOUT);
    }
  }
}

uint32_t ResponseMonitor::get_stat(uint8_t entry_index, ResponseMonitorStat stat) const {
  if (entry_index >= this->entries_.size())
    return 0;
  const ResponseMonitorEntry &entry = this->entries_[entry_index];
  switch (stat) {
    case RESPONSE_MONITOR_STAT_SUCCESS:
      return entry.success_count;
    case RESPONSE_MONITOR_STAT_FAIL:
      return entry.fail_count;
    case RESPONSE_MONITOR_STAT_TIMEOUT:
      return entry.timeout_count;
    case RESPONSE_MONITOR_STAT_NOT_APPLICABLE:
      return entry.not_applicable_count;
    case RESPONSE_MONITOR_STAT_ORPHAN:
      return entry.orphan_count;
    default:
      return 0;
  }
}

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_RESPONSE_MONITOR
