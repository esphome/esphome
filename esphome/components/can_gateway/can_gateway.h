#pragma once

#ifdef USE_ESP32

#include "gateway_core.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <esp_twai.h>
#include <esp_twai_onchip.h>
#include <esp_twai_types.h>
#include <freertos/FreeRTOS.h>

// Codegen-sized static pools: esphome/core/defines.h carries the
// static-analysis defaults, __init__.py emits the real values via add_define().

namespace esphome::can_gateway {

class CanGateway;
class GatewayPort;
class GatewayRoute;

// Contract-level rule flag bits emitted by codegen (see the symbol contract in
// __init__.py). Bits 0-4 are identical to the core RULE_FLAG_* bits and pass
// straight into RuleEntry::flags; bits 5-6 are consumed by add_rule() when it
// builds the prepared PatchData.
static constexpr uint8_t RULE_FLAG_OUT_EXT = 1 << 5;     ///< Frame type after modification.
static constexpr uint8_t RULE_FLAG_REPLACE_ID = 1 << 6;  ///< Modification rewrites ID / frame type.

/// One TX slot: the driver frame plus its payload storage. The driver TX queue
/// keeps a *pointer* to `frame` until on_tx_done, so slots live in static
/// arrays owned by the route (forwarding) or the port (inject).
/// `route`/`port`/`index` let the destination port's on_tx_done ISR return the
/// slot to the right pool.
struct TxSlot {
  twai_frame_t frame{};
  uint8_t payload[MAX_FRAME_DATA_LEN]{};
  GatewayRoute *route{nullptr};  // owning route (forward slots), or ...
  GatewayPort *port{nullptr};    // ... owning port (inject slots)
  uint8_t index{0};
};

/// Runtime-updatable patch handle for rules with an `id`.
/// set_byte()/set_can_id() stage into the inactive bank; commit() publishes
/// atomically. Loop context only; config validation guarantees only declared
/// entries are touched, and the byte's codegen-fixed mask is re-applied on
/// every stage.
class RulePatch {
 public:
  void set_byte(uint8_t index, uint8_t value) {
    if (index >= MAX_FRAME_DATA_LEN)
      return;
    PatchData &staged = this->banks_.stage();
    staged.or_value[index] = value & static_cast<uint8_t>(~staged.and_mask[index]);
  }
  void set_can_id(uint32_t can_id) {
    PatchData &staged = this->banks_.stage();
    // The rule's output frame type is fixed at codegen; mask runtime-staged
    // IDs to that type's width so a templated value can never make the
    // hardware truncate a too-wide ID on the wire. Static values are bounded
    // at validation time already.
    staged.new_can_id = can_id & (staged.new_extended ? MAX_EXTENDED_ID : MAX_STANDARD_ID);
  }
  void commit() { this->banks_.commit(); }

 protected:
  friend class GatewayRoute;
  void init_(const PatchData &initial) { this->banks_.init(initial); }
  const PatchBanks *banks_ptr_() const { return &this->banks_; }

 private:
  PatchBanks banks_;
};

/// One directed route. Owns its compiled rule table, its counters, and the
/// TX slot pool frames travel in.
class GatewayRoute {
 public:
  GatewayRoute(GatewayPort *from, GatewayPort *to, uint8_t rule_count, bool default_accept);

  /// Codegen: append one compiled rule (see the symbol contract in __init__.py
  /// for the argument encoding). Runs at construction time, before setup().
  void add_rule(uint32_t match_id, uint32_t match_mask, uint8_t flags, uint32_t new_id, uint64_t and_mask,
                uint64_t or_value, RulePatch *patch);

  const RouteCounters &counters() const { return this->counters_; }
  /// A diagnostics consumer (sensor hub) reads this route's counters; the
  /// hardware filter offload must then stay off so every frame is counted.
  void mark_observed() { this->observed_ = true; }

 protected:
  friend class CanGateway;
  friend class GatewayPort;

  GatewayPort *from_;
  GatewayPort *to_;
  bool observed_{false};
  FixedVector<RuleEntry> rules_;
  RouteTable table_{};
  RouteCounters counters_{};
  // Invariant coupling: pool_ tracks occupancy of slots_; the driver holds
  // pointers into slots_ until on_tx_done.
  SlotPool<CAN_GATEWAY_TX_SLOTS> pool_{};
  TxSlot slots_[CAN_GATEWAY_TX_SLOTS]{};
};

/// One TWAI controller. Created by codegen in declaration order — port
/// index 0 becomes TWAI0, index 1 TWAI1 (driver allocates controllers in
/// creation order).
class GatewayPort {
 public:
  GatewayPort(uint8_t index, int8_t rx_pin, int8_t tx_pin, uint32_t bit_rate)
      : index_(index), rx_pin_(rx_pin), tx_pin_(tx_pin), bit_rate_(bit_rate) {}

  void set_listen_only(bool listen_only) { this->listen_only_ = listen_only; }
  void set_self_test(bool self_test) { this->self_test_ = self_test; }
  void set_open_drain_tx(bool open_drain_tx) { this->open_drain_tx_ = open_drain_tx; }
  void set_tx_queue_depth(uint8_t tx_queue_depth) { this->tx_queue_depth_ = tx_queue_depth; }

  template<typename F> void add_on_bus_off_callback(F &&callback) {
    this->bus_off_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_recovered_callback(F &&callback) {
    this->recovered_callback_.add(std::forward<F>(callback));
  }

  /// Frame injection. Loop context only, never blocks.
  /// Returns false (with a throttled warning) when the port cannot transmit
  /// (listen-only, bus-off), no inject slot is free, or the TX queue is full.
  bool inject(uint32_t can_id, bool extended, bool rtr, const uint8_t *data, uint8_t len);

  bool is_bus_off() const {
    return this->state_.load(std::memory_order_relaxed) == static_cast<uint8_t>(TWAI_ERROR_BUS_OFF);
  }
  const PortCounters &counters() const { return this->counters_; }
  uint32_t bit_rate() const { return this->bit_rate_; }
  /// TEC/REC gauges for the sensor hub, via twai_node_get_info().
  bool read_error_counters(uint16_t &tec, uint16_t &rec) const;

#ifdef USE_CAN_GATEWAY_STATS
  /// Cumulative estimated on-wire bits seen (RX) and driven (TX) on this port's
  /// bus. Monotonic; the reader derives a load percentage from the delta over
  /// its own interval. Written only in ISR context, read in loop.
  uint32_t rx_bits() const { return this->rx_bits_.load(std::memory_order_relaxed); }
  uint32_t tx_bits() const { return this->tx_bits_.load(std::memory_order_relaxed); }
#endif

  // Fast path, ISR context (definitions in can_gateway.cpp, IRAM_ATTR).
  // Not public API: these are the driver-callback entry points and must only
  // be invoked by the TWAI driver through the registered callbacks.
  void handle_rx_isr();
  void handle_tx_done_isr(const twai_tx_done_event_data_t *edata);
  void handle_state_change_isr(const twai_state_change_event_data_t *edata);
  void handle_error_isr(twai_error_flags_t err_flags);

 protected:
  friend class CanGateway;
  friend class GatewayRoute;

  /// Node bring-up: create, filter-offload, register callbacks, enable.
  /// `route_out` is the port's future outbound route, passed in because
  /// route_out_ itself is only wired once every port is up.
  bool start_(const GatewayRoute *route_out, const std::atomic<bool> *gateway_enabled, uint8_t interrupt_priority);
  /// Program the hardware mask filter when the outbound rule set collapses
  /// onto it and no diagnostics consumer needs to see rejected frames.
  void try_hw_filter_offload_(const GatewayRoute *route);
  /// Control-plane duties: recovery backoff, edge automations,
  /// diagnostics publishing. Called from CanGateway::loop().
  void loop_(uint32_t now_ms);

#ifdef USE_CAN_GATEWAY_STATS
  /// Log one bus-statistics line for this port: load since the last
  /// call, error counters, TEC/REC, and — when enabled — the per-ID timing
  /// table. Loop context; called from CanGateway::loop() on the log interval.
  void log_statistics_(uint32_t now_ms);
#endif

  uint8_t index_;
  int8_t rx_pin_;
  int8_t tx_pin_;
  uint32_t bit_rate_;
  bool listen_only_{false};
  bool self_test_{false};
  bool open_drain_tx_{false};
  uint8_t tx_queue_depth_{8};

  twai_node_handle_t node_{nullptr};
  bool hw_filter_engaged_{false};
  /// Written by on_state_change (ISR), read by the fast path (peer bus-off
  /// check) and the control plane (recovery).
  std::atomic<uint8_t> state_{static_cast<uint8_t>(TWAI_ERROR_ACTIVE)};
  uint8_t last_loop_state_{static_cast<uint8_t>(TWAI_ERROR_ACTIVE)};
  RecoveryBackoff backoff_{};
  PortCounters counters_{};
  /// The (single) route whose `from` is this port; nullptr for
  /// destination-only ports. Wired in CanGateway::setup().
  GatewayRoute *route_out_{nullptr};
  const std::atomic<bool> *gateway_enabled_{nullptr};
  SlotPool<CAN_GATEWAY_INJECT_SLOTS> inject_pool_{};
  TxSlot inject_slots_[CAN_GATEWAY_INJECT_SLOTS]{};
  uint32_t last_inject_warn_ms_{0};
  /// Previous TEC/REC read, for deriving bus_err from their upward movement in
  /// loop_() (loop context only; see the bus_err comment there).
  static constexpr uint32_t ERR_POLL_INTERVAL_MS = 500;
  uint16_t last_tec_{0};
  uint16_t last_rec_{0};
  bool err_counters_seeded_{false};
  uint32_t last_err_poll_ms_{0};
  /// FIFO mirror of every frame handed to THIS port's driver node (forwards
  /// from the inbound route + own injects), for bus-off orphan reclamation
  /// (see OutstandingTracker's driver-contract comment). Mutated from the
  /// peer's RX ISR, this port's tx_done ISR, and loop context under mux_.
  OutstandingTracker<TxSlot *, CAN_GATEWAY_TX_SLOTS + CAN_GATEWAY_INJECT_SLOTS> outstanding_{};
  /// Serializes loop-context tracker access against both TWAI ISRs
  /// (single-core: masking interrupts is sufficient; ISRs never lock).
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  /// One eager head-reclaim per bus-off event (loop context only).
  bool bus_off_head_reclaimed_{true};
  /// Return a slot to its owning pool (route forward pool or port inject
  /// pool). Dissolves into its ISR callers (always_inline, no flash symbol).
  static CAN_GATEWAY_CORE_INLINE void release_slot_(TxSlot *slot) {
    if (slot->route != nullptr) {
      slot->route->pool_.release(slot->index);
    } else {
      slot->port->inject_pool_.release(slot->index);
    }
  }
  /// Release a bus-off orphan back to its pool and account it (tx_fail).
  void reclaim_orphan_(TxSlot *orphan);
  LazyCallbackManager<void()> bus_off_callback_{};
  LazyCallbackManager<void()> recovered_callback_{};

#ifdef USE_CAN_GATEWAY_STATS
  /// Estimated on-wire bits, accumulated in the RX ISR (rx) and the tx_done ISR
  /// (tx). Single writer each; the control plane only reads.
  std::atomic<uint32_t> rx_bits_{0};
  std::atomic<uint32_t> tx_bits_{0};
  /// Loop-context bookkeeping for the periodic log's load delta.
  uint32_t last_log_rx_bits_{0};
  uint32_t last_log_tx_bits_{0};
  uint32_t last_log_ms_{0};
  bool log_seeded_{false};
#ifdef USE_CAN_GATEWAY_ID_STATS
  /// Per-ID arrival timings (opt-in). Allocated once at setup in internal RAM;
  /// null when id_timings is off, so the ISR path is one pointer check.
  IdTimingTable<CAN_GATEWAY_ID_STATS_MAX> *id_timing_{nullptr};
#endif
#endif

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *bus_off_sensor_{nullptr};
#endif
#ifdef USE_CAN_GATEWAY_SNAPSHOT
  static constexpr uint8_t SNAPSHOT_RING_SIZE = 4;
  SnapshotRing<SNAPSHOT_RING_SIZE> *snapshot_ring_{nullptr};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_frame_sensor_{nullptr};
  uint32_t snapshot_throttle_ms_{1000};
  uint32_t last_snapshot_publish_ms_{0};
#endif
#endif
};

/// A timed (cyclic) frame sender (`cyclic_sends` in YAML). Transmits the latest
/// staged payload on its port every `interval_ms`, entirely from the main loop
/// (like `inject`): it never touches the RX fast path and makes no
/// latency promise. "Preprepared" data — the payload is staged off-path by
/// set_data() and the timer only hands the most recent bytes to the driver, so
/// no value is computed per tick. Update, start/stop and send are all loop
/// context, so the payload needs no cross-context synchronization.
class CyclicSend {
 public:
  CyclicSend(GatewayPort *port, uint32_t can_id, bool extended, bool rtr, uint32_t interval_ms, bool enabled)
      : port_(port), can_id_(can_id), interval_ms_(interval_ms), extended_(extended), rtr_(rtr), enabled_(enabled) {}

  /// Codegen: initial static payload. Loop-context runtime updates go through
  /// set_data() (identical semantics).
  void set_data(const uint8_t *data, uint8_t len) {
    if (len > MAX_FRAME_DATA_LEN)
      len = MAX_FRAME_DATA_LEN;
    for (uint8_t i = 0; i < len; i++)
      this->data_[i] = data[i];
    this->len_ = len;
  }
  void start() { this->enabled_ = true; }
  void stop() { this->enabled_ = false; }
  bool is_running() const { return this->enabled_; }

  /// Called from CanGateway::loop(); transmits when due.
  void loop_(uint32_t now_ms) {
    if (!this->enabled_ || now_ms - this->last_ms_ < this->interval_ms_)
      return;
    // Advance the schedule by exactly one interval so the average period tracks
    // the configured value despite the ~loop-period granularity of when we run.
    // If the loop stalled for more than one interval, resync to now instead of
    // firing on the next few loops to catch up (no catch-up burst).
    this->last_ms_ += this->interval_ms_;
    if (now_ms - this->last_ms_ >= this->interval_ms_)
      this->last_ms_ = now_ms;
    this->port_->inject(this->can_id_, this->extended_, this->rtr_, this->data_, this->rtr_ ? 0 : this->len_);
  }

  uint32_t can_id() const { return this->can_id_; }
  uint32_t interval_ms() const { return this->interval_ms_; }
  const GatewayPort *port() const { return this->port_; }

 protected:
  GatewayPort *port_;
  uint32_t can_id_;
  uint32_t interval_ms_;
  uint32_t last_ms_{0};
  uint8_t data_[MAX_FRAME_DATA_LEN]{};
  uint8_t len_{0};
  bool extended_;
  bool rtr_;
  bool enabled_;
};

/// The gateway component: owns both ports, wires the routes, runs the control
/// plane. The data plane never enters this class after setup().
class CanGateway : public Component {
 public:
  explicit CanGateway(uint8_t interrupt_priority) : interrupt_priority_(interrupt_priority) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void add_port(GatewayPort *port) { this->ports_.push_back(port); }
  void add_route(GatewayRoute *route) { this->routes_.push_back(route); }

#ifdef USE_CAN_GATEWAY_CYCLIC
  void add_cyclic_send(CyclicSend *cyclic) { this->cyclic_sends_.push_back(cyclic); }
#endif
#ifdef USE_CAN_GATEWAY_STATS
  /// Enable the periodic statistics log; 0 leaves only the dump_config summary.
  void set_stats_log_interval(uint32_t interval_ms) { this->stats_log_interval_ms_ = interval_ms; }
  /// Request per-port per-ID timing tables (allocated in setup()).
  void set_id_timings_enabled(bool enabled) { this->id_timings_ = enabled; }
#endif

  /// Gates forwarding only; diagnostics, recovery and inject keep working.
  void set_enabled(bool enabled) { this->enabled_.store(enabled, std::memory_order_relaxed); }
  bool is_enabled() const { return this->enabled_.load(std::memory_order_relaxed); }

#ifdef USE_BINARY_SENSOR
  void set_bus_off_binary_sensor(GatewayPort *port, binary_sensor::BinarySensor *sensor) {
    port->bus_off_sensor_ = sensor;
  }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_frame_text_sensor(GatewayPort *port, text_sensor::TextSensor *sensor, uint32_t throttle_ms);
#endif

 protected:
  uint8_t interrupt_priority_;
  std::atomic<bool> enabled_{true};
  StaticVector<GatewayPort *, 2> ports_;
  StaticVector<GatewayRoute *, 2> routes_;
#ifdef USE_CAN_GATEWAY_CYCLIC
  StaticVector<CyclicSend *, CAN_GATEWAY_CYCLIC_COUNT> cyclic_sends_;
#endif
#ifdef USE_CAN_GATEWAY_STATS
  uint32_t stats_log_interval_ms_{0};
  uint32_t last_stats_log_ms_{0};
  bool id_timings_{false};
#endif
};

// ---------------------------------------------------------------------------
// Entity platforms
// ---------------------------------------------------------------------------

#ifdef USE_SWITCH
/// Gateway enable switch. Boot state follows restore_mode
/// (default RESTORE_DEFAULT_ON); without a switch the gateway boots enabled.
class CanGatewaySwitch : public switch_::Switch, public Component, public Parented<CanGateway> {
 public:
  void setup() override {
    optional<bool> restored = this->get_initial_state_with_restore_mode();
    // restore_mode: DISABLED yields no value — take no action and publish
    // nothing, leaving the gateway at its boot default (enabled).
    if (restored.has_value())
      this->write_state(*restored);
  }
  /// Runs before CanGateway::setup() (HARDWARE priority), so a persisted OFF
  /// state is applied before the controllers come up — otherwise frames would
  /// cross during the boot window between the two setups. Safe this early:
  /// the ESP32 arch initializes preferences in app_main, before any
  /// component's setup().
  float get_setup_priority() const override { return setup_priority::HARDWARE + 1.0f; }

 protected:
  void write_state(bool state) override {
    this->parent_->set_enabled(state);
    this->publish_state(state);
  }
};
#endif

#ifdef USE_SENSOR
/// Polling hub for route counters, port counters, and TEC/REC gauges.
class CanGatewaySensorHub : public PollingComponent {
 public:
  /// Sensor slot indices. Must match ALL_KINDS in sensor.py exactly — the
  /// Python side emits these indices positionally, and its test suite locks
  /// the order. Extend both together, appending only.
  enum KindIndex : uint8_t {
    KIND_FORWARDED = 0,
    KIND_FILTERED,
    KIND_TX_FULL,
    KIND_BUS_OFF,
    KIND_DISABLED,
    KIND_INJECTED,
    KIND_TX_FAIL,
    KIND_BUS_ERR,
    KIND_RECOVERIES,
    KIND_TEC,
    KIND_REC,
    KIND_BUS_LOAD,
    KIND_COUNT,
  };

  void set_route(GatewayRoute *route) {
    this->route_ = route;
    // Counters become externally observable; the hardware filter offload
    // must stay off so hardware-rejected frames keep being counted.
    route->mark_observed();
  }
  void set_port(GatewayPort *port) { this->port_ = port; }
  void set_counter_sensor(uint8_t kind, sensor::Sensor *sensor) {
    if (kind < KIND_COUNT)
      this->sensors_[kind] = sensor;
  }

  void update() override;
  void dump_config() override;

 protected:
  GatewayRoute *route_{nullptr};
  GatewayPort *port_{nullptr};
  sensor::Sensor *sensors_[KIND_COUNT]{};
#ifdef USE_CAN_GATEWAY_STATS
  /// Delta bookkeeping for the bus_load gauge (loop context).
  uint32_t last_bus_load_rx_bits_{0};
  uint32_t last_bus_load_tx_bits_{0};
  uint32_t last_bus_load_ms_{0};
  bool bus_load_seeded_{false};
#endif
};
#endif

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

/// Shared payload plumbing for the data-carrying actions: either a pointer to
/// codegen-emitted static data (lives in flash, no RAM copy) or a templated
/// producer. Loop context; the temporary vector is the documented cost of
/// templated data, mirroring canbus.send.
template<typename... Ts> class DataPayload {
 public:
  void set_data_static(const uint8_t *data, uint8_t len) {
    this->static_data_ = data;
    this->static_len_ = static_cast<int8_t>(len);
  }
  // Stateless lambdas (generated by ESPHome) convert to function pointers.
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->template_func_ = func;
    this->static_len_ = -1;
  }

 protected:
  /// Resolve the payload and hand it to `sink(data, len)`.
  template<typename F> void with_payload_(F &&sink, const Ts &...x) {
    if (this->static_len_ >= 0) {
      sink(this->static_data_, static_cast<uint8_t>(this->static_len_));
      return;
    }
    std::vector<uint8_t> data = this->template_func_(x...);
    uint8_t len = data.size() > MAX_FRAME_DATA_LEN ? MAX_FRAME_DATA_LEN : static_cast<uint8_t>(data.size());
    sink(data.data(), len);
  }

  const uint8_t *static_data_{nullptr};
  int8_t static_len_{0};  ///< >= 0: static mode with length; -1: template mode.
  std::vector<uint8_t> (*template_func_)(Ts...){nullptr};
};

template<typename... Ts> class SetPatchAction : public Action<Ts...>, public Parented<RulePatch> {
 public:
  /// Codegen: size the byte list before the add_*_byte calls (count is fixed
  /// by the YAML).
  void init_bytes(uint8_t count) { this->bytes_.init(count); }
  void add_static_byte(uint8_t index, uint8_t value) {
    this->bytes_.push_back(ByteEntry{index, TemplatableValue<uint8_t, Ts...>(value)});
  }
  // Stateless lambdas (generated by ESPHome) implicitly convert to function pointers.
  void add_templated_byte(uint8_t index, uint8_t (*value)(Ts...)) {
    this->bytes_.push_back(ByteEntry{index, TemplatableValue<uint8_t, Ts...>(value)});
  }
  void set_can_id(uint32_t can_id) { this->can_id_ = TemplatableValue<uint32_t, Ts...>(can_id); }
  void set_can_id_template(uint32_t (*can_id)(Ts...)) { this->can_id_ = TemplatableValue<uint32_t, Ts...>(can_id); }

  void play(const Ts &...x) override {
    if (this->can_id_.has_value())
      this->parent_->set_can_id(this->can_id_.value().value(x...));
    for (size_t i = 0; i < this->bytes_.size(); i++)
      this->parent_->set_byte(this->bytes_[i].index, this->bytes_[i].value.value(x...));
    this->parent_->commit();
  }

 protected:
  struct ByteEntry {
    uint8_t index;
    TemplatableValue<uint8_t, Ts...> value;
  };
  FixedVector<ByteEntry> bytes_;
  optional<TemplatableValue<uint32_t, Ts...>> can_id_{};
};

template<typename... Ts>
class InjectAction : public Action<Ts...>, public Parented<GatewayPort>, public DataPayload<Ts...> {
 public:
  void set_frame(uint32_t can_id, bool extended, bool rtr) {
    this->can_id_ = can_id;
    this->extended_ = extended;
    this->rtr_ = rtr;
  }

  void play(const Ts &...x) override {
    this->with_payload_(
        [this](const uint8_t *data, uint8_t len) {
          this->parent_->inject(this->can_id_, this->extended_, this->rtr_, data, len);
        },
        x...);
  }

 protected:
  uint32_t can_id_{0};
  bool extended_{false};
  bool rtr_{false};
};

// ---------------------------------------------------------------------------
// Cyclic-send actions: update the staged payload, start, or stop a timed
// sender. Loop context, like the timer itself.
// ---------------------------------------------------------------------------

template<typename... Ts>
class SetCyclicDataAction : public Action<Ts...>, public Parented<CyclicSend>, public DataPayload<Ts...> {
 public:
  void play(const Ts &...x) override {
    this->with_payload_([this](const uint8_t *data, uint8_t len) { this->parent_->set_data(data, len); }, x...);
  }
};

template<typename... Ts> class StartCyclicAction : public Action<Ts...>, public Parented<CyclicSend> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class StopCyclicAction : public Action<Ts...>, public Parented<CyclicSend> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

}  // namespace esphome::can_gateway

#endif  // USE_ESP32
