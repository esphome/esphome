#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"  // before CANopenNode: clang-tidy gets the CO_CONFIG_* values from it
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#define OD_DEFINITION
// Use the library search path: quoted includes match our canopen.h on Windows.
#include <CANopen.h>

#include "types.h"

namespace esphome::canopen {

// CANopen on CANopenNode: an SDO client on the bus. It reads and writes objects of other CANopen devices (drives,
// sensors, I/O modules) from automations (canopen.sdo_read / canopen.sdo_write) and lambdas; the hub isn't a CANopen
// node itself (no node id, NMT, heartbeat or SDO server), so only CANopenNode's CAN driver, SDO client and OD access
// are linked.
//
// The implementation is split by feature: canopen.cpp (setup, processing), driver_can.cpp (CANopenNode's CAN driver,
// frame reception and transmission), sdo_client.cpp.
class CANopenComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_canbus(canbus::Canbus *canbus);
  void set_response_timeout(uint16_t ms) { this->response_timeout_ms_ = ms; }

  // Used by the CANopenNode driver (driver_can.cpp): sends a standard-ID frame (ident bit 11 = RTR) on the canbus.
  bool transmit(uint16_t ident, const uint8_t *data, uint8_t len);

  // --- SDO client (sdo_client.cpp) ---
  // SDO client (one channel, requests run one after another in FIFO order). Every request ends in exactly one
  // callback: accepted ones from loop() / frame processing after the transfer, rejected ones (queue full, invalid
  // arguments, stack not running) synchronously before the call returns false.

  // value read; expected_size 1/2/4 checks the width (SIZE_MISMATCH), 0 accepts any width up to 4
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t expected_size, SDOCallback callback,
                void *context);
  template<typename T>
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context) {
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
  // buffer transfers (strings, domains): segmented (every server supports it, block transfer is optional);
  // reads up to 1024 bytes.
  // write_sdo_data() doesn't copy: data must stay valid until the callback.
  bool read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context);
  bool write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                      SDOCallback callback = nullptr, void *context = nullptr);

 protected:
  void on_frame_(const canbus::CanFrame &frame);
  void receive_(uint16_t ident, const uint8_t *data, uint8_t len);
  void process_();

  canbus::Canbus *canbus_{nullptr};
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_

  uint32_t last_us_{0};
  uint16_t response_timeout_ms_{500};

  // --- SDO client (sdo_client.cpp) ---
  enum class SDOKind : uint8_t { SDO_KIND_READ, SDO_KIND_WRITE, SDO_KIND_READ_DATA, SDO_KIND_WRITE_DATA };
  struct SDORequest {
    uint8_t node_id;
    uint16_t index;
    uint8_t sub;
    SDOKind kind;
    uint8_t size;  // read: expected size (0 = any); write: value size
    uint32_t value;
    const uint8_t *data;  // WRITE_DATA, owned by the caller
    size_t len;
    SDOCallback callback;
    void *context;
  };
  static constexpr size_t SDO_QUEUE_SIZE = 16;
  static constexpr size_t SDO_MAX_UPLOAD = 1024;
  void add_sdo_client_od_();
  void config_sdo_client_(CO_config_t &cfg);
  bool init_sdo_client_();
  bool queue_sdo_(SDORequest &&request);
  void process_sdo_(uint32_t dt_us);
  bool start_sdo_();
  void finish_sdo_(SDOResult result, uint32_t abort_code);
  // OD 0x1280 (SDO client parameter), set up per request
  uint8_t sdo_cli_highest_sub_{3};
  uint32_t sdo_cli_cob_tx_{0x80000000};
  uint32_t sdo_cli_cob_rx_{0x80000000};
  uint8_t sdo_cli_node_{0};
  // The object dictionary CO_SDOclient_init() needs: only 0x1280, plus the blank last entry OD_find() expects.
  // CANopenNode writes the entry's extension pointer, so it isn't const.
  std::array<OD_obj_record_t, 4> sdo_cli_record_{};
  std::array<OD_entry_t, 2> od_entries_{};
  OD_t od_{};
  std::array<SDORequest, SDO_QUEUE_SIZE> sdo_queue_{};
  size_t sdo_head_{0};
  size_t sdo_count_{0};
  SDORequest sdo_active_{};
  bool sdo_busy_{false};
  size_t sdo_offset_{0};  // WRITE/WRITE_DATA: bytes handed to the client fifo
  // READ/READ_DATA: bytes received (also those beyond the buffer, for the size checks), kept in sdo_rx_value_ (READ)
  // or sdo_rx_buf_ (READ_DATA)
  size_t sdo_rx_len_{0};
  std::array<uint8_t, 4> sdo_rx_value_{};
  std::array<uint8_t, SDO_MAX_UPLOAD> sdo_rx_buf_{};
};

}  // namespace esphome::canopen
