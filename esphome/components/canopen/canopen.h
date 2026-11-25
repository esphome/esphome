#pragma once

#include <array>
#include <cstring>
#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"  // CO_CONFIG_* for clang-tidy, before CANopenNode
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#define OD_DEFINITION
// Not quoted: that matches our canopen.h on Windows.
#include <CANopen.h>

#include "types.h"

namespace esphome::canopen {

void discard_pending_tx(CO_CANmodule_t *mod, CO_CANtx_t *buf);

// CANopenNode SDO client and raw frames: no node id, NMT, heartbeat or SDO server.
class CANopenComponent : public Component {
 public:
  explicit CANopenComponent(canbus::Canbus *canbus) : canbus_(canbus) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  // after canbus, before on_boot (600)
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

  void set_response_timeout(uint16_t ms) { this->response_timeout_ms_ = ms; }

  // for the CANopenNode driver; ident bit 11 is RTR
  bool transmit(uint16_t ident, const uint8_t *data, uint8_t len);

  // One channel in FIFO order; requests made before setup() wait for it. Each gets exactly one SDOCallback.

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
  // Buffers must stay valid until the callback; a read_sdo_data() object longer than capacity is aborted.
  // Transfers are limited to 64 KiB: a longer write is rejected, a larger capacity is clamped.
  bool read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t *data, size_t capacity, SDOCallback callback,
                     void *context);
  bool write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                      SDOCallback callback = nullptr, void *context = nullptr);
  // Drops all queued requests of context and aborts its transfer in flight, without callbacks.
  void cancel_sdo(void *context);

  // Raw frames the SDO client doesn't use (e.g. other devices' PDOs). The callback gets every standard-ID data frame
  // with (can_id ^ id) & mask == 0, after CANopenNode has seen it. Can be called before or after setup(), also from a
  // frame callback. Adding a listener twice does nothing; each hub needs its own listener object. It can't be removed
  // and must stay alive (make it static).
  void add_frame_listener(FrameListener &listener);
  // Sends a standard-ID data frame now. Returns false for an extended CAN ID, more than 8 bytes, or if the canbus
  // platform refuses it (e.g. TX buffers full); it isn't queued or retried, unlike CANopenNode's own frames.
  bool send_frame(uint32_t can_id, const uint8_t *data, size_t len);
  // every heartbeat / EMCY frame on the bus, from any node
#ifdef USE_CANOPEN_HEARTBEAT
  template<typename F> void add_on_heartbeat_callback(F &&callback) {
    this->heartbeat_callback_.add(std::forward<F>(callback));
  }
#endif
#ifdef USE_CANOPEN_EMCY
  template<typename F> void add_on_emcy_callback(F &&callback) { this->emcy_callback_.add(std::forward<F>(callback)); }
#endif

 protected:
  void on_frame_(const canbus::CanFrame &frame);
  // frame listeners and heartbeat / EMCY callbacks for a received frame, after CANopenNode
  void dispatch_frame_(const CO_CANrxMsg_t &msg);
  void check_frame_listener_(uint16_t can_id, uint16_t mask);
  bool idle_() const;
  void fail_setup_(const LogString *reason);

  canbus::Canbus *canbus_;
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_

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
  void add_sdo_client_od_();
  bool init_sdo_client_();
  bool queue_sdo_(const SDORequest &request);
  void reject_queued_sdo_();
  bool sdo_reply_is_stale_(const uint8_t *data, uint8_t len) const;
  void process_sdo_(uint32_t dt_us);
  // true when an overlong read ended the request
  bool drain_upload_(uint32_t dt_us, CO_SDO_return_t &ret, CO_SDO_abortCode_t &abort);
  CO_SDO_return_t start_sdo_();
  void finish_sdo_(SDOResult result, uint32_t abort_code);
  void abort_sdo_();
  // ends an abort the bus refused for a response timeout, then runs down the guard after it (ABORT_GUARD_US)
  void process_sdo_abort_(uint32_t dt_us);
  // the 0x1280 entry for CO_SDOclient_init(); not const, CANopenNode writes it
  std::array<OD_obj_record_t, 4> sdo_cli_record_{};
  OD_entry_t sdo_cli_entry_{};
  StaticRingBuffer<SDORequest, SDO_QUEUE_SIZE> sdo_queue_;
  SDORequest sdo_active_{};
  // bytes handed to the fifo (writes) or received, including those past the buffer (reads)
  size_t sdo_len_{0};
  uint32_t last_ms_{0};  // when time was last charged to the SDO client
  // after a cancel: while the abort waits to be sent, the time left to send it; once sent, the time left before a
  // request may start
  uint32_t sdo_abort_guard_us_{0};
  // OD 0x1280 fields, set per request
  uint32_t sdo_cli_cob_tx_{0x80000000};
  uint32_t sdo_cli_cob_rx_{0x80000000};
  std::array<uint8_t, 4> sdo_rx_value_{};
  uint16_t response_timeout_ms_{500};
  uint8_t sdo_cli_highest_sub_{3};
  uint8_t sdo_cli_node_{0};
  bool sdo_busy_{false};
  bool sdo_abort_pending_{false};   // the abort of a cancelled transfer waits to be sent
  bool sdo_cancel_pending_{false};  // cancel_sdo() hit the active transfer: process_sdo_() aborts it

  FrameListener *frame_listeners_{nullptr};
  // Compiled only when on_heartbeat / on_emcy are configured, so never empty: no lazy allocation needed
#ifdef USE_CANOPEN_HEARTBEAT
  CallbackManager<void(uint8_t, NMTState)> heartbeat_callback_;
#endif
#ifdef USE_CANOPEN_EMCY
  CallbackManager<void(uint8_t, const Emergency &)> emcy_callback_;
#endif
};

}  // namespace esphome::canopen
