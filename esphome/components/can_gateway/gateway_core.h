#pragma once

// Pure data-plane logic for the can_gateway component.
//
// This header is deliberately freestanding: no ESPHome and no ESP-IDF includes, so the exact code
// that runs in the RX interrupt on the ESP32-C6 also compiles and runs under a plain host
// toolchain for unit testing. Driver glue (TWAI node bring-up, ISR registration) lives in
// can_gateway.cpp and never in this file.
//
// Concurrency model: single core, both TWAI interrupts at the SAME
// priority. Interrupts run to completion — an ISR can preempt loop code, but nothing preempts an
// ISR. Every structure here is lock-free under exactly that model and additionally uses
// acquire/release atomics so the host tests (and any future multi-core target) stay correct.

#include <atomic>
#include <cstddef>
#include <cstdint>

// Forced inlining for the per-frame hot path: the ISR callbacks in can_gateway.cpp are placed in
// IRAM; these helpers must dissolve into those callers so no fast-path code can end up
// as a flash-resident out-of-line symbol while the cache is disabled (CONFIG_TWAI_ISR_CACHE_SAFE).
#if defined(__GNUC__) || defined(__clang__)
#define CAN_GATEWAY_CORE_INLINE inline __attribute__((always_inline))
#else
#define CAN_GATEWAY_CORE_INLINE inline
#endif

namespace esphome::can_gateway {

/// Classic CAN payload limit (ISO 11898-1); CAN FD is out of scope.
static constexpr uint8_t MAX_FRAME_DATA_LEN = 8;
/// Largest valid 11-bit (standard) CAN identifier.
static constexpr uint32_t MAX_STANDARD_ID = 0x7FF;
/// Largest valid 29-bit (extended) CAN identifier.
static constexpr uint32_t MAX_EXTENDED_ID = 0x1FFFFFFF;

/// Returned by SlotPool::acquire() when no slot is free.
static constexpr uint8_t SLOT_NONE = 0xFF;

// ---------------------------------------------------------------------------------------------
// Prepared modifications
// ---------------------------------------------------------------------------------------------

/// One prepared modification: optional ID replacement plus per-byte AND/OR payload patch.
/// The YAML entry {index, value, mask} compiles to and_mask[index] = ~mask,
/// or_value[index] = value & mask, so application is branch-free per byte:
/// byte = (byte & and_mask[i]) | or_value[i].
/// The *shape* (replace_id, new_extended, which masks differ from identity) is fixed at codegen;
/// only the values may be re-staged at runtime through PatchBanks.
struct PatchData {
  uint32_t new_can_id{0};
  bool replace_id{false};
  /// Frame type after an ID replacement (only meaningful when replace_id is set).
  bool new_extended{false};
  uint8_t and_mask[MAX_FRAME_DATA_LEN]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t or_value[MAX_FRAME_DATA_LEN]{0, 0, 0, 0, 0, 0, 0, 0};
};

/// Double-banked storage for a runtime-updatable patch (rules with an `id`).
///
/// Writer side (loop context only): stage() returns the inactive bank, copying the active bank
/// into it on the first call of a staging cycle; commit() publishes it by flipping one atomic
/// index with release ordering. Reader side (ISR): active_patch() loads the index exactly once
/// with acquire ordering. Because the ISR never runs concurrently with itself and always runs to
/// completion, a frame observes either the complete old or the complete new patch — never a torn
/// mix — and the fast path takes no lock and never retries.
class PatchBanks {
 public:
  /// ISR side: the currently published patch. Index is read once; the returned reference stays
  /// valid for the whole frame because writers only ever touch the *inactive* bank.
  CAN_GATEWAY_CORE_INLINE const PatchData &active_patch() const {
    return this->banks_[this->active_.load(std::memory_order_acquire)];
  }

  /// Loop side: bank to write staged changes into. First call in a cycle seeds it from the
  /// active bank so partial updates keep the untouched values.
  PatchData &stage() {
    uint8_t active = this->active_.load(std::memory_order_relaxed);
    if (!this->staging_) {
      this->banks_[1 - active] = this->banks_[active];
      this->staging_ = true;
    }
    return this->banks_[1 - active];
  }

  /// Loop side: atomically publish all staged changes. No-op when nothing was staged.
  void commit() {
    if (!this->staging_)
      return;
    uint8_t active = this->active_.load(std::memory_order_relaxed);
    this->active_.store(1 - active, std::memory_order_release);
    this->staging_ = false;
  }

  /// Codegen/setup only (single-threaded): set the initial patch in both banks.
  void init(const PatchData &initial) {
    this->banks_[0] = initial;
    this->banks_[1] = initial;
    this->active_.store(0, std::memory_order_release);
    this->staging_ = false;
  }

 private:
  // Invariant coupling (banks_ vs active_ vs staging_): writers only touch banks_[1 - active_],
  // the ISR only reads banks_[active_]; staging_ belongs to loop context exclusively.
  PatchData banks_[2]{};
  std::atomic<uint8_t> active_{0};
  bool staging_{false};
};

// ---------------------------------------------------------------------------------------------
// Rule table and matching
// ---------------------------------------------------------------------------------------------

/// Rule flag bits (kept in one byte per entry).
static constexpr uint8_t RULE_FLAG_EXTENDED = 1 << 0;   ///< Rule matches 29-bit frames (else 11-bit only).
static constexpr uint8_t RULE_FLAG_CHECK_RTR = 1 << 1;  ///< Rule additionally compares the RTR flag.
static constexpr uint8_t RULE_FLAG_RTR_VALUE = 1 << 2;  ///< Required RTR value when RULE_FLAG_CHECK_RTR.
static constexpr uint8_t RULE_FLAG_DROP = 1 << 3;       ///< action: drop (default is accept).
static constexpr uint8_t RULE_FLAG_HAS_PATCH = 1 << 4;  ///< A modify block is attached.

/// One filter rule, emitted by codegen into a static array.
struct RuleEntry {
  uint32_t match_id;
  uint32_t match_mask;
  uint8_t flags;
  /// Patch used when the rule is not runtime-updatable (banks == nullptr).
  PatchData static_patch{};
  /// Non-null for rules with an `id` (runtime-updatable); points at the RulePatch
  /// handle's banks. Null for plain rules.
  const PatchBanks *banks{nullptr};

  CAN_GATEWAY_CORE_INLINE bool matches(uint32_t can_id, bool extended, bool rtr) const {
    if (extended != ((this->flags & RULE_FLAG_EXTENDED) != 0))
      return false;
    if ((can_id & this->match_mask) != this->match_id)
      return false;
    if ((this->flags & RULE_FLAG_CHECK_RTR) != 0 && rtr != ((this->flags & RULE_FLAG_RTR_VALUE) != 0))
      return false;
    return true;
  }

  CAN_GATEWAY_CORE_INLINE bool is_drop() const { return (this->flags & RULE_FLAG_DROP) != 0; }
  CAN_GATEWAY_CORE_INLINE bool has_patch() const { return (this->flags & RULE_FLAG_HAS_PATCH) != 0; }

  /// The patch to apply right now: the published bank for updatable rules, the static data
  /// otherwise. Call once per frame (the bank index is read exactly once).
  CAN_GATEWAY_CORE_INLINE const PatchData &effective_patch() const {
    return this->banks != nullptr ? this->banks->active_patch() : this->static_patch;
  }
};

/// A route's compiled rule set. `rules` points into a codegen-emitted static array.
struct RouteTable {
  const RuleEntry *rules{nullptr};
  uint8_t rule_count{0};
  /// default_action: drop — frames matching no rule are dropped (allow-list mode).
  bool default_drop{false};
};

/// Fate of a processed frame, mapped 1:1 onto the route counters it must increment.
enum class FrameAction : uint8_t {
  FORWARD = 0,        ///< Transmit on the destination port (counter: forwarded on success).
  DROP_FILTERED = 1,  ///< Dropped by a rule or the default action (counter: filtered).
};

/// First matching rule in table order, or nullptr when nothing matches (first match wins;
/// standard and extended ID spaces are strictly separate).
CAN_GATEWAY_CORE_INLINE const RuleEntry *match_rule(const RouteTable &table, uint32_t can_id, bool extended, bool rtr) {
  for (uint8_t i = 0; i < table.rule_count; i++) {
    if (table.rules[i].matches(can_id, extended, rtr))
      return &table.rules[i];
  }
  return nullptr;
}

/// Apply a prepared patch in place. Payload bytes are patched only below the
/// frame's DLC and only for data frames — an RTR frame carries no data bytes to patch. DLC never
/// changes.
CAN_GATEWAY_CORE_INLINE void apply_patch(const PatchData &patch, uint32_t &can_id, bool &extended, bool rtr,
                                         uint8_t *data, uint8_t dlc) {
  if (patch.replace_id) {
    can_id = patch.new_can_id;
    extended = patch.new_extended;
  }
  if (rtr)
    return;
  uint8_t limit = dlc < MAX_FRAME_DATA_LEN ? dlc : MAX_FRAME_DATA_LEN;
  for (uint8_t i = 0; i < limit; i++) {
    data[i] = static_cast<uint8_t>((data[i] & patch.and_mask[i]) | patch.or_value[i]);
  }
}

/// Full per-frame decision for one route (match -> drop or patch). Mutates
/// header/payload in place when forwarding. Never allocates, never blocks.
CAN_GATEWAY_CORE_INLINE FrameAction process_frame(const RouteTable &table, uint32_t &can_id, bool &extended, bool rtr,
                                                  uint8_t *data, uint8_t dlc) {
  const RuleEntry *rule = match_rule(table, can_id, extended, rtr);
  if (rule == nullptr)
    return table.default_drop ? FrameAction::DROP_FILTERED : FrameAction::FORWARD;
  if (rule->is_drop())
    return FrameAction::DROP_FILTERED;
  if (rule->has_patch())
    apply_patch(rule->effective_patch(), can_id, extended, rtr, data, dlc);
  return FrameAction::FORWARD;
}

// ---------------------------------------------------------------------------------------------
// TX slot pools — also used for the per-port inject slots
// ---------------------------------------------------------------------------------------------

/// Occupancy tracker for a static array of TX slots. The slot storage itself (driver frame
/// struct + payload buffer) lives next to this in the component, sized by codegen; the pool only
/// hands out indices, so this logic stays driver-agnostic and host-testable.
///
/// Safe under the single-core model for every combination that occurs: acquire from an RX ISR or
/// from loop context (inject), release from the destination port's on_tx_done ISR. The
/// compare-free exchange makes a loop-context acquire that gets preempted by an ISR still claim
/// atomically; ISRs themselves never race each other (equal priority).
template<uint8_t N> class SlotPool {
  static_assert(N > 0 && N < SLOT_NONE, "slot count must fit the index byte");

 public:
  /// Claim a free slot; returns its index or SLOT_NONE when exhausted (caller sheds the frame
  /// and counts it — never silently).
  CAN_GATEWAY_CORE_INLINE uint8_t acquire() {
    for (uint8_t i = 0; i < N; i++) {
      if (!this->used_[i].load(std::memory_order_relaxed) &&
          !this->used_[i].exchange(true, std::memory_order_acquire)) {
        return i;
      }
    }
    return SLOT_NONE;
  }

  /// Return a slot to the pool (on_tx_done, or immediately after a filter drop).
  CAN_GATEWAY_CORE_INLINE void release(uint8_t index) { this->used_[index].store(false, std::memory_order_release); }

  bool is_used(uint8_t index) const { return this->used_[index].load(std::memory_order_relaxed); }

  static constexpr uint8_t capacity() { return N; }

  /// Approximate occupancy for dump_config(); exact only while no ISR is active.
  uint8_t in_use() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < N; i++) {
      if (this->is_used(i))
        count++;
    }
    return count;
  }

 private:
  std::atomic<bool> used_[N]{};
};

// ---------------------------------------------------------------------------------------------
// Outstanding-TX tracker (bus-off orphan reclamation)
// ---------------------------------------------------------------------------------------------

/// Reclaims TX slots orphaned by bus-off. Driver contract this rests on, established by reading
/// esp_driver_twai in IDF 5.5.4 (esp_twai_onchip.c, twai_hal_v1.c):
///  - A node owns ONE hardware TX buffer plus a FIFO pointer queue (tx_mount_queue). Frames are
///    mounted strictly oldest-first and on_tx_done reports completions in submission order.
///  - Entering bus-off HALTS the mounted frame: the HAL clears its TX-occupied flag without
///    raising TX_BUFF_FREE ("Any TX would have been halted by entering bus off"), and on
///    recovery the driver overwrites its current-TX pointer with the next queued frame — the
///    halted frame NEVER gets on_tx_done. Without reclamation its slot leaks, one per bus-off.
///  - Frames still in the queue at bus-off are RETAINED and replayed after recovery completes
///    ("node recover from busoff, restart remain tx transaction"); each replayed frame gets a
///    normal on_tx_done. They must NOT be reclaimed.
///  - While bus-off, no on_tx_done can occur (TX halted) and no new frame can be submitted
///    (twai_node_transmit rejects while bus-off). There is no flush/abort API.
///
/// The tracker mirrors the driver's FIFO: push() after every accepted hand-off, complete() on
/// every on_tx_done. Because completions arrive in submission order, any tracked entries OLDER
/// than the completed one are exactly the bus-off orphans — complete() hands them to the caller
/// for release (lazy self-healing). reclaim_head() lets the control plane free the halted head
/// eagerly while the port is still bus-off, so a fully-orphaned pool cannot deadlock injection.
///
/// Synchronization is the CALLER's job: all calls must be mutually exclusive. On the single-core
/// C6 that holds because the two TWAI ISRs run at equal priority (never preempt each other) and
/// loop-context calls run inside a critical section that masks those interrupts.
template<typename T, uint8_t N> class OutstandingTracker {
  static_assert(N > 0, "tracker needs capacity");

 public:
  /// Record an accepted hand-off to the driver. Returns false when full (cannot happen when N
  /// covers every slot that can be in flight; callers may treat it as a logic error).
  CAN_GATEWAY_CORE_INLINE bool push(T item) {
    if (this->count_ >= N)
      return false;
    this->items_[(this->head_ + this->count_) % N] = item;
    this->count_++;
    return true;
  }

  /// on_tx_done: pop `done`, reclaiming every older entry (bus-off orphans) via
  /// `reclaim(T)` first. Returns the number reclaimed. When `done` is not tracked
  /// (impossible by construction), nothing is mutated and nothing is reclaimed.
  template<typename F> CAN_GATEWAY_CORE_INLINE uint8_t complete(T done, F &&reclaim) {
    uint8_t depth = 0;
    while (depth < this->count_ && this->items_[(this->head_ + depth) % N] != done)
      depth++;
    if (depth == this->count_)
      return 0;  // not tracked: leave state untouched
    for (uint8_t i = 0; i < depth; i++) {
      reclaim(this->items_[this->head_]);
      this->head_ = (this->head_ + 1) % N;
    }
    this->head_ = (this->head_ + 1) % N;  // pop `done` itself; caller releases it
    this->count_ -= depth + 1;
    return depth;
  }

  /// Eager orphan reclaim while the port is bus-off: pops the halted head, if any. Only valid
  /// while the port state is bus-off (see the contract above); at most once per bus-off event.
  CAN_GATEWAY_CORE_INLINE bool reclaim_head(T &out) {
    if (this->count_ == 0)
      return false;
    out = this->items_[this->head_];
    this->head_ = (this->head_ + 1) % N;
    this->count_--;
    return true;
  }

  uint8_t size() const { return this->count_; }

 private:
  // Invariant coupling: items_[head_ .. head_+count_) is the FIFO of in-driver frames, oldest
  // first; mutations are serialized by the caller (see class comment).
  T items_[N]{};
  uint8_t head_{0};
  uint8_t count_{0};
};

// ---------------------------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------------------------

/// Per-route counters. Each is written from exactly one context (the route's RX ISR); the
/// control plane only reads. Monotonic, never reset (Home Assistant handles wrap).
struct RouteCounters {
  std::atomic<uint32_t> forwarded{0};
  std::atomic<uint32_t> filtered{0};
  std::atomic<uint32_t> tx_full{0};
  std::atomic<uint32_t> bus_off{0};
  std::atomic<uint32_t> disabled{0};

  /// Accounting invariant: everything the route ever received.
  uint32_t total() const {
    return this->forwarded.load(std::memory_order_relaxed) + this->filtered.load(std::memory_order_relaxed) +
           this->tx_full.load(std::memory_order_relaxed) + this->bus_off.load(std::memory_order_relaxed) +
           this->disabled.load(std::memory_order_relaxed);
  }
};

/// Per-port counters. injected/bus_err/recoveries: loop context; tx_fail/err_events:
/// that port's ISRs. bus_err is the genuine-fault signal, derived in loop_() from
/// upward TEC/REC movement (a benign arbitration loss leaves TEC/REC untouched, and
/// on the C6 TWAIFD its error flag is indistinguishable from a real one, see
/// handle_error_isr). err_events is the raw count of every reported error event
/// (genuine + benign arbitration loss) -- a bus-activity/contention diagnostic that
/// is expected to climb on any bus the gateway transmits into a lower-id flood.
struct PortCounters {
  std::atomic<uint32_t> injected{0};
  std::atomic<uint32_t> tx_fail{0};
  std::atomic<uint32_t> bus_err{0};
  std::atomic<uint32_t> err_events{0};
  std::atomic<uint32_t> recoveries{0};
};

/// Relaxed increment helper — every counter has a single writer context (see above), readers
/// only need eventual visibility.
CAN_GATEWAY_CORE_INLINE void count(std::atomic<uint32_t> &counter) { counter.fetch_add(1, std::memory_order_relaxed); }

// ---------------------------------------------------------------------------------------------
// Snapshot ring — ISR producer, loop consumer, keep-newest
// ---------------------------------------------------------------------------------------------

/// Copy of one received RX frame for the last_frame debug text sensor.
/// Captured before rule filtering: the sensor shows everything the port
/// retrieves off the wire, including frames a rule subsequently drops.
struct FrameSnapshot {
  uint32_t can_id{0};
  uint8_t dlc{0};
  bool extended{false};
  bool rtr{false};
  uint8_t data[MAX_FRAME_DATA_LEN]{};
};

/// Single-producer (ISR) / single-consumer (loop) ring that overwrites the oldest entry when
/// full (the debug sensor wants the newest frame; losing older ones is fine and is
/// counted). Every entry carries a sequence stamp (seqlock style): the producer marks it odd
/// while writing and (ticket * 2 + 2) when complete, so a consumer preempted mid-copy detects
/// the overwrite and retries with the newer head. The producer is wait-free and never reads
/// consumer state.
template<uint8_t N> class SnapshotRing {
  static_assert(N >= 2, "ring needs at least two entries so the producer never overwrites the entry being read");

 public:
  /// Producer (ISR): store one snapshot, overwriting the oldest when the ring is full.
  CAN_GATEWAY_CORE_INLINE void push(const FrameSnapshot &snapshot) {
    uint32_t ticket = this->head_.load(std::memory_order_relaxed);
    Entry &entry = this->entries_[ticket % N];
    entry.seq.store(ticket * 2 + 1, std::memory_order_release);  // odd: write in progress
    entry.value = snapshot;
    entry.seq.store(ticket * 2 + 2, std::memory_order_release);  // even: complete for this ticket
    this->head_.store(ticket + 1, std::memory_order_release);
  }

  /// Consumer (loop): copy out the newest snapshot. Returns false when the ring is empty or the
  /// producer outpaces the copy attempts (extremely busy bus — the next throttle tick retries).
  /// Accumulates the number of pushes that were never observed (reported in dump_config).
  bool read_latest(FrameSnapshot &out) {
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
      uint32_t head = this->head_.load(std::memory_order_acquire);
      if (head == 0)
        return false;
      if (head == this->last_seen_)
        return false;  // nothing new since the last drain
      uint32_t ticket = head - 1;
      const Entry &entry = this->entries_[ticket % N];
      uint32_t seq_before = entry.seq.load(std::memory_order_acquire);
      FrameSnapshot copy = entry.value;
      uint32_t seq_after = entry.seq.load(std::memory_order_acquire);
      if (seq_before == seq_after && seq_before == ticket * 2 + 2) {
        this->missed_ += (head - this->last_seen_) - 1;
        this->last_seen_ = head;
        out = copy;
        return true;
      }
      // Overwritten mid-copy; retry against the newer head.
    }
    return false;
  }

  /// Pushes the consumer never got to see (loop context only).
  uint32_t missed() const { return this->missed_; }

  /// Total frames ever pushed (loop context diagnostic).
  uint32_t pushed() const { return this->head_.load(std::memory_order_relaxed); }

 private:
  struct Entry {
    std::atomic<uint32_t> seq{0};
    FrameSnapshot value{};
  };

  Entry entries_[N]{};
  std::atomic<uint32_t> head_{0};
  // Consumer-side bookkeeping; loop context only.
  uint32_t last_seen_{0};
  uint32_t missed_{0};
};

// ---------------------------------------------------------------------------------------------
// Bus statistics — ISR producer, loop consumer
// ---------------------------------------------------------------------------------------------

/// Approximate on-wire bit count of one classic CAN frame, for the bus-load
/// statistic. Counts framing + control + CRC + ACK + EOF + IFS: a standard
/// frame is 47 bits + 8*DLC, an extended frame 67 bits + 8*DLC. Bit stuffing is
/// not modeled (worst case adds ~20 % on the stuffed region), so a busy bus with
/// dense payloads reads a few percent low — good enough for a usage gauge, and
/// deliberately branch-poor so it can live in the RX ISR.
CAN_GATEWAY_CORE_INLINE uint32_t estimate_frame_bits(bool extended, bool rtr, uint8_t dlc) {
  // 44 = SOF+ID(11)+control+CRC+ACK+EOF; +20 for the extended-ID fields; +3 IFS.
  uint32_t overhead = extended ? 64u : 44u;
  uint32_t data_bits = rtr ? 0u : 8u * dlc;
  return overhead + data_bits + 3u;
}

/// One tracked CAN ID's arrival statistics. `sum_period_us` accumulates the
/// inter-arrival gaps (wall-clock microseconds, wrap-safe per gap); the loop
/// derives the average period as sum / (count - 1). Kept in microseconds so the
/// ISR never divides and the value is already in the logged unit.
struct IdTimingEntry {
  uint32_t can_id{0};
  uint32_t count{0};
  uint32_t last_us{0};
  uint64_t sum_period_us{0};
  bool extended{false};
};

/// Per-port table of per-ID arrival timings (opt-in `id_timings`).
/// Single writer: the port's RX ISR calls record() for every received frame.
/// Single reader: the control plane reads size()/entry()/overflow() while the
/// port's interrupts are masked (single-core: the ISR cannot then preempt, so
/// the reader sees no torn 64-bit sum). Entries are append-only: an ID keeps its
/// index for the lifetime of the table, so a reader that captures size() once
/// and walks [0, size) is always consistent even as the ISR appends.
template<uint16_t N> class IdTimingTable {
  static_assert(N > 0, "id-timing table needs capacity");

 public:
  /// ISR: fold one received RX frame into the table (pre-filter, like the
  /// bus-load accounting). `now_us` is a monotonic
  /// wall-clock microsecond stamp (esp_timer_get_time()), which — unlike a CPU
  /// cycle count — keeps advancing while the core idles in WFI. Bounded work: at
  /// most N comparisons, then either an in-place update, an append, or an
  /// overflow tally when the table is full.
  CAN_GATEWAY_CORE_INLINE void record(uint32_t can_id, bool extended, uint32_t now_us) {
    for (uint16_t i = 0; i < this->used_; i++) {
      IdTimingEntry &entry = this->entries_[i];
      if (entry.can_id == can_id && entry.extended == extended) {
        entry.sum_period_us += static_cast<uint32_t>(now_us - entry.last_us);
        entry.last_us = now_us;
        entry.count++;
        return;
      }
    }
    if (this->used_ >= N) {
      this->overflow_++;
      return;
    }
    IdTimingEntry &entry = this->entries_[this->used_];
    entry.can_id = can_id;
    entry.extended = extended;
    entry.count = 1;
    entry.last_us = now_us;
    entry.sum_period_us = 0;
    // Publish the populated entry last: a reader that observes the new size()
    // then reads this index sees fully-written fields (single-core: the store
    // ordering below the ISR is program order, and the reader masks interrupts).
    this->used_++;
  }

  uint16_t size() const { return this->used_; }
  uint32_t overflow() const { return this->overflow_; }
  const IdTimingEntry &entry(uint16_t index) const { return this->entries_[index]; }

 private:
  IdTimingEntry entries_[N]{};
  uint16_t used_{0};
  uint32_t overflow_{0};
};

// ---------------------------------------------------------------------------------------------
// Bus-off recovery backoff — pure schedule, no driver calls
// ---------------------------------------------------------------------------------------------

/// Exponential backoff schedule for bus-off recovery: 100 ms, 200 ms, 400 ms ... capped at
/// 3.2 s. The escalated delay is kept across a completed recovery and resets to the base
/// delay only after the bus has stayed healthy for STABLE_RESET_MS (resetting on every
/// recovery would make a permanently dead bus cycle recover→bus-off at ~5 Hz;
/// it must settle at the cap instead). The component's loop() feeds it wall-clock millis and
/// the port state; it answers "call twai_node_recover() now?". Loop context only.
class RecoveryBackoff {
 public:
  static constexpr uint32_t INITIAL_DELAY_MS = 100;
  static constexpr uint32_t MAX_DELAY_MS = 3200;
  static constexpr uint32_t STABLE_RESET_MS = 10000;

  /// Port just entered bus-off (edge, not level): schedule the next recovery attempt.
  /// Any stable-period measurement in progress is void — the bus was not stable after all.
  void on_bus_off(uint32_t now_ms) {
    this->pending_ = true;
    this->stable_since_valid_ = false;
    this->due_at_ms_ = now_ms + this->delay_ms_;
  }

  /// True exactly once per scheduled attempt, when it is due. The caller then invokes
  /// twai_node_recover(); if the bus stays dead the next on_bus_off() doubles the delay.
  bool should_attempt(uint32_t now_ms) {
    if (!this->pending_ || static_cast<int32_t>(now_ms - this->due_at_ms_) < 0)
      return false;
    this->pending_ = false;
    uint32_t next = this->delay_ms_ * 2;
    this->delay_ms_ = next > MAX_DELAY_MS ? MAX_DELAY_MS : next;
    return true;
  }

  /// Port returned to error-active: recovery completed. The escalated delay is kept;
  /// the stable-period clock starts now.
  void on_recovered(uint32_t now_ms) {
    this->pending_ = false;
    this->stable_since_ms_ = now_ms;
    this->stable_since_valid_ = true;
  }

  /// Called from loop() while the port is healthy: once the bus has been stable for
  /// STABLE_RESET_MS since the last recovery, the schedule resets to the base delay.
  void on_stable_tick(uint32_t now_ms) {
    if (this->stable_since_valid_ && now_ms - this->stable_since_ms_ >= STABLE_RESET_MS) {
      this->delay_ms_ = INITIAL_DELAY_MS;
      this->stable_since_valid_ = false;
    }
  }

  uint32_t current_delay_ms() const { return this->delay_ms_; }
  bool pending() const { return this->pending_; }

 private:
  uint32_t delay_ms_{INITIAL_DELAY_MS};
  uint32_t due_at_ms_{0};
  uint32_t stable_since_ms_{0};
  bool pending_{false};
  bool stable_since_valid_{false};
};

}  // namespace esphome::can_gateway
