#pragma once

#include <array>
#include <cstring>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"  // before CANopenNode: clang-tidy gets the CO_CONFIG_* values from it
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#define OD_DEFINITION
// Use the library search path: quoted includes match our canopen.h on Windows.
#include <CANopen.h>

#include "types.h"

namespace esphome::canopen {

// Drops the buffer's frame if it still waits to be sent (driver_can.cpp).
void discard_pending_tx(CO_CANmodule_t *mod, CO_CANtx_t *buf);

// A CANopenNode SDO client: reads and writes objects of CANopen devices (drives, sensors, I/O modules) from
// automations and lambdas. Not a CANopen node (no node id, NMT, heartbeat or SDO server). Split by feature: canopen.cpp
// (setup, processing), driver_can.cpp (CANopenNode's CAN driver, frame reception and transmission), sdo_client.cpp.
class CANopenComponent : public Component {
 public:
  explicit CANopenComponent(canbus::Canbus *canbus) : canbus_(canbus) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  // after the CAN bus, before the default on_boot automations (600)
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

  void set_response_timeout(uint16_t ms) { this->response_timeout_ms_ = ms; }

  // Used by the CANopenNode driver (driver_can.cpp): sends a standard-ID frame (ident bit 11 = RTR) on the canbus.
  bool transmit(uint16_t ident, const uint8_t *data, uint8_t len);

  // --- SDO client (sdo_client.cpp) ---
  // One channel, requests in FIFO order (made before setup(), they wait for it). Each ends in exactly one callback:
  // from loop() / frame processing after the transfer, or for a rejected one (queue full, invalid arguments, failed
  // hub) synchronously before the call returns false; requests made before a failed setup() are rejected from it.

  // value read; expected_size 1/2/4 checks the width (SIZE_MISMATCH), 0 accepts any width up to 4
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t expected_size, SDOCallback callback,
                void *context);
  template<typename T>
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context) {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "SDO values are 1, 2 or 4 bytes");
    return this->read_sdo(node_id, index, sub, sizeof(T), callback, context);
  }
  // value write of size 1/2/4 bytes (little endian)
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
  // buffer transfers (strings, domains), segmented; the caller's buffers must stay valid until the callback (so not on
  // the stack). read_sdo_data(): an object longer than capacity is aborted; reply.data is data.
  bool read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t *data, size_t capacity, SDOCallback callback,
                     void *context);
  bool write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                      SDOCallback callback = nullptr, void *context = nullptr);

 protected:
  void on_frame_(const canbus::CanFrame &frame);
  // no request queued or running and no frame waiting to be resent
  bool idle_() const;
  // marks the hub failed and ends the requests queued before setup()
  void fail_setup_(const LogString *reason);

  canbus::Canbus *canbus_;
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_

  // --- SDO client (sdo_client.cpp) ---
  enum class SDOKind : uint8_t { SDO_KIND_READ, SDO_KIND_WRITE, SDO_KIND_READ_DATA, SDO_KIND_WRITE_DATA };
  struct SDORequest {
    uint8_t node_id;
    uint8_t sub;
    SDOKind kind;
    uint8_t size;  // read: expected size (0 = any); write: value size
    uint16_t index;
    union {
      uint32_t value;  // WRITE
      uint8_t *data;   // READ_DATA, WRITE_DATA (only read): owned by the caller
    };
    size_t len;  // READ_DATA: capacity of data
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
  // runs the upload and drains the client fifo; true when an overlong read ended the request here
  bool drain_upload_(uint32_t dt_us, CO_SDO_return_t &ret, CO_SDO_abortCode_t &abort);
  bool start_sdo_();
  void finish_sdo_(SDOResult result, uint32_t abort_code);
  // the OD CO_SDOclient_init() needs: 0x1280 and the blank last entry (not const: CANopenNode writes the entry)
  std::array<OD_obj_record_t, 4> sdo_cli_record_{};
  std::array<OD_entry_t, 2> od_entries_{};
  OD_t od_{};
  StaticRingBuffer<SDORequest, SDO_QUEUE_SIZE> sdo_queue_;
  SDORequest sdo_active_{};
  // WRITE/WRITE_DATA: bytes handed to the client fifo; READ/READ_DATA: bytes received, also those beyond the buffer
  size_t sdo_len_{0};
  uint32_t last_ms_{0};  // loop time the SDO client was last given
  // OD 0x1280 (SDO client parameter, with sdo_cli_highest_sub_ and sdo_cli_node_), set up per request
  uint32_t sdo_cli_cob_tx_{0x80000000};
  uint32_t sdo_cli_cob_rx_{0x80000000};
  std::array<uint8_t, 4> sdo_rx_value_{};
  uint16_t response_timeout_ms_{500};
  uint8_t sdo_cli_highest_sub_{3};
  uint8_t sdo_cli_node_{0};
  bool sdo_busy_{false};
};

}  // namespace esphome::canopen
