#pragma once

#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#include "od.h"
#include "types.h"

namespace esphome::canopen {

void discard_pending_tx(CO_CANmodule_t *mod, CO_CANtx_t *buf);

// Other components configure the node in two phases:
// - Build time (Python codegen): the hub parts they use, enable_sdo_client(); it sets defines and CANopenNode build
//   flags, so it can't be switched on at runtime.
// - Before the hub's setup(), which builds the object dictionary and initialises CANopenNode: identity and device
//   information. Two places run early enough: setters called from codegen (they run in main(), before App.setup()), or
//   the setup() of a component whose setup priority is above the hub's (setup_priority::HARDWARE - 1, after the CAN
//   bus), e.g. HARDWARE - 0.5. Object dictionary entries added later are logged as errors and ignored.
// The rest is runtime API, e.g. from loop() or automations: add_frame_listener() and the callbacks (any time),
// send_frame(), SDO client requests (made before the hub's setup(), they wait for it). The hub's setup() runs before
// most components' setup() (e.g. sensors, DATA) and the default on_boot automations.
//
// Client-only mode: without a node id (NO_NODE_ID) the hub is no CANopen node, only an SDO client on the bus (plus
// raw frames). CANopenNode's node part (NMT, heartbeat, EMCY, SDO server) isn't initialised, so it isn't linked either.
//
// The implementation is split by feature: canopen.cpp (setup, processing, NMT), driver_can.cpp (CANopenNode's CAN
// driver, frame reception and transmission), device_info.cpp (identity, device information, SDO server), od.cpp
// (the object dictionary), frames.cpp (raw frames, heartbeat / EMCY callbacks) and sdo_client.cpp.
class CANopenComponent : public Component {
 public:
  // CiA 301 node ids are 1..127; 0 is reserved (NMT broadcast), so it can't clash with a real node. It's also
  // CANopenNode's "no own node" value (CO_SDOclient_init()).
  static constexpr uint8_t NO_NODE_ID = 0;

  CANopenComponent(canbus::Canbus *canbus, uint8_t node_id) : canbus_(canbus), node_id_(node_id) {}
  // no node id: SDO client only, see above
  bool is_client_only() const { return this->node_id_ == NO_NODE_ID; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // after canbus, before on_boot (600)
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

  // Other canopen nodes on the same canbus, as a ring (codegen): a frame one of them sends is also received by the
  // others, since a CAN controller doesn't receive its own frames. nullptr = no other local node.
  void set_next_local(CANopenComponent *next) { this->next_local_ = next; }
#ifdef USE_CANOPEN_NODE
  void set_heartbeat_interval(uint16_t ms) { this->hb_producer_ms_ = ms; }
  // NMT state after boot: OPERATIONAL (default) or PRE_OPERATIONAL (until an NMT master, or set_nmt_state(), starts
  // the node)
  void set_start_operational(bool operational) { this->start_operational_ = operational; }
#endif
  void set_response_timeout(uint16_t ms) { this->response_timeout_ms_ = ms; }

  // Our own NMT state (UNKNOWN before setup, and without a node id) and its changes, by an NMT master or
  // set_nmt_state()
#ifdef USE_CANOPEN_NODE
  NMTState get_nmt_state() const { return this->nmt_state_; }
  template<typename F> void add_on_nmt_state_callback(F &&callback) {
    this->nmt_state_callback_.add(std::forward<F>(callback));
  }
#else
  NMTState get_nmt_state() const { return NMTState::NMT_STATE_UNKNOWN; }
#endif
  // Moves our own node to OPERATIONAL, PRE_OPERATIONAL or STOPPED (no NMT master needed)
  bool set_nmt_state(NMTState state);

  // Used by the CANopenNode driver (driver_can.cpp) and send_frame(): sends a standard-ID frame (ident bit 11 =
  // RTR) on the canbus and, once sent, delivers it to the other local nodes. They only receive it here (CANopenNode
  // buffers, frame listeners, callbacks) and process it in their next loop(), so no node's processing runs inside
  // another's.
  bool transmit(uint16_t ident, const uint8_t *data, uint8_t len);
  // Used by the CANopenNode driver: CO_process() flushes the queued frames too, but the hub flushes them only once,
  // early in loop(): not while a received frame is processed (on ESP32 a refused frame can block for the controller's
  // enqueue timeout), nor again in loop()'s CO_process() (the SDO response timer restarts after that one flush).
  bool flush_held() const { return this->hold_flush_; }

#ifdef USE_CANOPEN_NODE
  // --- Identity and device information (device_info.cpp) ---
  // Identity (0x1018) and device information (0x1008 device name, 0x1009 hardware version, 0x100A software
  // version). Before setup() only; the strings must outlive the component. Defaults: identity 0, the node's name,
  // no hardware version, ESPHOME_VERSION.
  void set_identity(uint32_t vendor_id, uint32_t product_code, uint32_t revision_number, uint32_t serial_number) {
    this->identity_ = {vendor_id, product_code, revision_number, serial_number};
  }
  void set_device_name(const char *name) { this->device_name_ = name; }
  void set_hardware_version(const char *version) { this->hardware_version_ = version; }
  void set_software_version(const char *version) { this->software_version_ = version; }
#endif

  // --- Object dictionary (od.cpp) ---
  // Object dictionary for other components: entries are added before setup() only (see above), since CANopenNode
  // can't add entries after its init. The storage must outlive the component.
  ObjectDictionary &get_od() { return this->od_; }

  // --- Raw frames (frames.cpp) ---
  // every heartbeat / EMCY frame on the bus, from any node (our own frames aren't received; those of other local
  // nodes are)
#ifdef USE_CANOPEN_HEARTBEAT
  template<typename F> void add_on_heartbeat_callback(F &&callback) {
    this->heartbeat_callback_.add(std::forward<F>(callback));
  }
#endif
#ifdef USE_CANOPEN_EMCY
  template<typename F> void add_on_emcy_callback(F &&callback) { this->emcy_callback_.add(std::forward<F>(callback)); }
#endif
  // Raw frames the OD doesn't describe (e.g. other devices' PDOs). The callback gets every standard-ID data frame
  // with (can_id ^ id) & mask == 0, after CANopenNode has seen it. Can be called before or after setup(), also from a
  // frame callback. Adding a listener twice does nothing; each hub needs its own listener object. It can't be removed
  // and must stay alive (make it static).
  void add_frame_listener(FrameListener &listener);
  // Sends a standard-ID data frame now. Returns false for a CAN ID above 0x7FF, more than 8 bytes, or if the canbus
  // platform refuses it (e.g. TX buffers full); it isn't queued or retried, unlike CANopenNode's own frames.
  bool send_frame(uint32_t can_id, const uint8_t *data, size_t len);

#ifdef USE_CANOPEN_SDO_CLIENT
  // --- SDO client (sdo_client.cpp) ---
  // SDO client (one channel, requests run one after another in FIFO order). Every request ends in exactly one
  // callback: accepted ones from loop() / frame processing after the transfer, rejected ones (queue full, invalid
  // arguments, failed hub) synchronously before the call returns false. Requests made before setup() wait in the
  // queue (e.g. from on_boot); a failed setup() rejects them. Requests to our own node id are served from our object
  // dictionary, without CAN frames.

  // expected_size 1/2/4 must match the reply's width, 0 accepts any up to 4
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t expected_size, SDOCallback callback,
                void *context);
  template<typename T>
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context) {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "SDO values are 1, 2 or 4 bytes");
    return this->read_sdo(node_id, index, sub, sizeof(T), callback, context);
  }
  // size: 1, 2 or 4 bytes
  bool write_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint32_t value, uint8_t size,
                 SDOCallback callback = nullptr, void *context = nullptr);
  template<typename T>
  bool write_sdo(uint8_t node_id, uint16_t index, uint8_t sub, T value, SDOCallback callback = nullptr,
                 void *context = nullptr) {
    static_assert(sizeof(T) <= 4, "SDO values are at most 4 bytes");
    uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(T));
    return this->write_sdo(node_id, index, sub, raw, sizeof(T), callback, context);
  }
  // buffer transfers (strings, domains): segmented (every server supports it, block transfer is optional);
  // the buffers are the caller's and must stay valid until the callback (so not on the stack).
  // read_sdo_data(): an object longer than capacity is aborted; reply.data is data.
  bool read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t *data, size_t capacity, SDOCallback callback,
                     void *context);
  bool write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                      SDOCallback callback = nullptr, void *context = nullptr);
  // Drops all queued requests of context and aborts its transfer in flight, without callbacks.
  void cancel_sdo(void *context);
#endif

 protected:
  void on_frame_(const canbus::CanFrame &frame);
  /// Returns whether a CANopenNode object took the frame
  bool receive_(uint16_t ident, const uint8_t *data, uint8_t len);
  // A client-only hub with no request queued or running and no frame waiting to be resent. A node always has work
  // (heartbeat).
  bool idle_() const;
  // CO_process() and our node's own processing: per received frame and in loop()
  void process_node_();
  // marks the hub failed and ends the requests queued before setup()
  void fail_setup_(const LogString *reason);
  // The features' parts of setup(), called in this order: add_*_od_() add OD entries (before the OD is finalized),
  // config_*_() fill in their CO_config_t counts and entries, init_*_() run once CANopenNode is initialised.

  canbus::Canbus *canbus_;
  CANopenComponent *next_local_{nullptr};
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_
  ObjectDictionary od_;
  uint16_t response_timeout_ms_{500};
  uint8_t node_id_;
  bool hold_flush_{false};

#ifdef USE_CANOPEN_NODE
  // --- Identity and device information (device_info.cpp) ---
  void set_device_info_defaults_();
  void add_device_info_od_();
  void config_device_info_(CO_config_t &cfg);
  void dump_device_info_config_();

  LazyCallbackManager<void(NMTState)> nmt_state_callback_;
  uint32_t cob_emcy_{0x80};                // 0x1014, CANopenNode adds the node id
  uint32_t last_us_{0};                    // the node's clock (CO_process())
  uint32_t dev_type_{0};                   // 0x1000
  const char *device_name_{nullptr};       // 0x1008
  const char *hardware_version_{nullptr};  // 0x1009, only if set
  const char *software_version_{nullptr};  // 0x100A
  std::array<uint32_t, 4> identity_{};     // 0x1018:1..4
  // 0x1200 (SDO server 1): CANopenNode fills in the default COB-IDs 0x600 / 0x580 + node id
  uint32_t sdo_srv_cob_rx_{0};
  uint32_t sdo_srv_cob_tx_{0};
  uint16_t hb_producer_ms_{5000};    // 0x1017
  uint8_t err_reg_{0};               // 0x1001
  uint8_t identity_highest_sub_{4};  // 0x1018
  uint8_t sdo_srv_highest_sub_{2};   // 0x1200
  NMTState nmt_state_{NMTState::NMT_STATE_UNKNOWN};
  bool start_operational_{true};
#endif  // USE_CANOPEN_NODE

  // --- Raw frames (frames.cpp) ---
  // frame listeners and heartbeat / EMCY callbacks for a received frame, after CANopenNode
  void dispatch_frame_(const CO_CANrxMsg_t &msg);
  void check_frame_listener_(uint16_t can_id, uint16_t mask);
  FrameListener *frame_listeners_{nullptr};
  // Compiled only when on_heartbeat / on_emcy are configured, so never empty: no lazy allocation needed
#ifdef USE_CANOPEN_HEARTBEAT
  CallbackManager<void(uint8_t, NMTState)> heartbeat_callback_;
#endif
#ifdef USE_CANOPEN_EMCY
  CallbackManager<void(uint8_t, const Emergency &)> emcy_callback_;
#endif

#ifdef USE_CANOPEN_SDO_CLIENT
  // --- SDO client (sdo_client.cpp) ---
  enum class SDOKind : uint8_t { SDO_KIND_READ, SDO_KIND_WRITE, SDO_KIND_READ_DATA, SDO_KIND_WRITE_DATA };
  struct SDORequest {
    uint8_t node_id;
    uint8_t sub;
    SDOKind kind;
    uint8_t size;  // read: expected size (0 = any); write: value size
    uint16_t index;
    uint16_t len;  // data length (READ_DATA: capacity)
    union {
      uint32_t value;  // WRITE
      uint8_t *data;   // *_DATA, owned by the caller
    };
    SDOCallback callback;
    void *context;
  };
  static constexpr size_t SDO_QUEUE_SIZE = 16;
  bool sdo_idle_() const {
    return !this->sdo_busy_ && this->sdo_queue_.empty() && !this->sdo_abort_pending_ && this->sdo_abort_guard_us_ == 0;
  }
  void add_sdo_client_od_();
  void config_sdo_client_(CO_config_t &cfg);
  // client-only mode; a node's client is initialised by CO_CANopenInit()
  bool init_sdo_client_();
  bool queue_sdo_(const SDORequest &request);
  void reject_queued_sdo_();
  bool sdo_stopped_() const;
  // the active request is served from our own OD (CO_CONFIG_SDO_CLI_LOCAL), not over the bus
  bool sdo_is_local_() const {
#ifdef USE_CANOPEN_NODE
    return !this->is_client_only() && this->sdo_active_.node_id == this->node_id_;
#else
    return false;
#endif
  }
  void stop_sdo_();
  bool sdo_reply_is_stale_(const uint8_t *data, uint8_t len) const;
  void process_sdo_(uint32_t dt_us);
  // true when an overlong read ended the request
  bool drain_upload_(uint32_t dt_us, CO_SDO_return_t &ret, CO_SDO_abortCode_t &abort);
  CO_SDO_return_t start_sdo_();
  void finish_sdo_(SDOResult result, uint32_t abort_code);
  void abort_sdo_();
  // ends an abort the bus refused for a response timeout, then runs down the guard after it (ABORT_GUARD_US)
  void process_sdo_abort_(uint32_t dt_us);
  StaticRingBuffer<SDORequest, SDO_QUEUE_SIZE> sdo_queue_;
  SDORequest sdo_active_{};
  // bytes handed to the fifo (writes) or received, including those past the buffer (reads)
  size_t sdo_len_{0};
  uint32_t sdo_last_ms_{0};  // loop time the SDO client was last given
  // after a cancel: while the abort waits to be sent, the time left to send it; once sent, the time left before a
  // request may start
  uint32_t sdo_abort_guard_us_{0};
  // OD 0x1280 (SDO client parameter, with sdo_cli_highest_sub_ and sdo_cli_node_), set up per request
  uint32_t sdo_cli_cob_tx_{0x80000000};
  uint32_t sdo_cli_cob_rx_{0x80000000};
  std::array<uint8_t, 4> sdo_rx_value_{};
  uint8_t sdo_cli_highest_sub_{3};
  uint8_t sdo_cli_node_{0};
  bool sdo_busy_{false};
  bool sdo_abort_pending_{false};
  bool sdo_cancel_pending_{false};  // cancel_sdo() hit the active transfer: process_sdo_() aborts it
#endif
};

}  // namespace esphome::canopen
