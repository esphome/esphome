#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "genibus_frame.h"

namespace esphome::genibus {

// The APDUs of one request. A class 10 GET needs 5 bytes; the largest request planned for writing is a class 10 SET
// of an 18 byte object: 2 + 9 + 18 = 29 bytes.
static constexpr size_t MAX_REQUEST_SIZE = 32;

enum class GenibusError : uint8_t {
  GENIBUS_ERROR_NONE,
  GENIBUS_ERROR_TIMEOUT,
  GENIBUS_ERROR_CRC,
  GENIBUS_ERROR_MALFORMED,  // the APDU lengths do not add up to the frame
  GENIBUS_ERROR_WRITE_FAILED,
  GENIBUS_ERROR_DISCONNECTED,
};

/// The answer to a request; apdus points into the receive buffer and is valid only during the callback. It is empty
/// on error, and also on success when the reply has no APDU (LE = 2).
/// GENIBUS_ERROR_NONE only means a valid reply frame arrived: an APDU with an ACK code other than 0 (unknown class or
/// ID, illegal operation) is still a success for the bus and clears its warning, so the device checks each ACK.
/// GENIbus has no transaction id: a reply that comes after the timeout and the quiet period is taken for the answer
/// to the device's next request. A device checks class, object type and version of every reply, and counts a write
/// as done only after reading the value back, never on the acknowledgement alone.
struct GenibusResponse {
  GenibusError error;
  std::span<const uint8_t> apdus;
};

class GenibusDevice;

/// The request of one device that waits for the wire; size is 0 while nothing waits.
struct GenibusPending {
  GenibusDevice *device;
  std::array<uint8_t, MAX_REQUEST_SIZE> apdus;
  uint8_t size;
};

/// The protocol side of a GENIbus master. A transport derives from it, writes frames and feeds received bytes.
/// Only one transaction is on the wire at a time; each device has at most one request open.
/// The status warning is per bus: the last transaction sets or clears it.
class GenibusBus : public Component {
 public:
  void loop() override;
  void dump_config() override;

  void set_master_address(uint8_t address) { this->master_address_ = address; }

  SLOT_LIST_REQUIRES(GENIBUS_DEVICE_COUNT)
  void register_device(GenibusDevice *device) { this->pending_.push_back({device, {}, 0}); }

  /// Queue the request of device. Returns false, queueing nothing, when the bus is not connected, the request is
  /// empty or longer than MAX_REQUEST_SIZE, the device is not registered, or it already has a request open (see
  /// is_busy()). Every accepted request gets exactly one on_response(); the device's slot is free again when
  /// on_response() runs, so it may send its next request from there.
  bool send(GenibusDevice *device, std::span<const uint8_t> apdus);
  /// Whether device has a request waiting or on the wire.
  bool is_busy(const GenibusDevice *device) const;

  /// Feed received bytes, in any chunks. Call from the main loop only.
  void receive_bytes(std::span<const uint8_t> data);

  virtual bool is_connected() const = 0;

 protected:
  /// Write a whole frame; a transport that needs smaller pieces splits it.
  virtual bool write_frame(std::span<const uint8_t> frame) = 0;
  /// Call when the connection is up, after is_connected() has started to return true: every device gets
  /// on_connected() and may send its first request from there.
  void connection_established_();
  /// Call when the connection drops, after is_connected() has started to return false: every open and waiting request
  /// ends with GENIBUS_ERROR_DISCONNECTED, then every device gets on_disconnected(); requests sent from those callbacks
  /// are refused. Calls after the first one until the next connection_established_() end requests only.
  void connection_lost_();

  void handle_frame_();
  void transmit_next_();
  void finish_(GenibusError error, std::span<const uint8_t> apdus = {});

  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1000;
  // After a failed transaction, a late reply must not be taken for the answer to the next request
  static constexpr uint32_t QUIET_MS = 250;

  [[no_unique_address]] SlotList<GenibusPending, GENIBUS_DEVICE_COUNT> pending_;
  FrameAssembler rx_;
  GenibusDevice *active_{nullptr};  // the device whose request is on the wire
  uint32_t sent_at_{0};
  uint32_t quiet_since_{0};
  uint8_t next_slot_{0};  // where the round-robin search for the next request starts
  uint8_t master_address_{0};
  uint8_t active_address_{0};
  bool quiet_{false};
  bool down_notified_{false};  // whether the devices have heard of the current loss of the connection
};

/// A device on the bus; it gets the answer to its request in on_response().
class GenibusDevice : public PollingComponent, public Parented<GenibusBus> {
 public:
  virtual void on_response(const GenibusResponse &response) = 0;
  /// The connection is up; a device may read at once instead of waiting for its next update().
  virtual void on_connected() {}
  /// The connection is down and every request of this device has ended; a device may mark its values unknown.
  virtual void on_disconnected() {}

  void set_address(uint8_t address) { this->address_ = address; }
  uint8_t get_address() const { return this->address_; }

 protected:
  /// Send a class 10 GET of object/sub_id; false when send() refuses it.
  bool request_object_(uint8_t object, uint16_t sub_id);

  uint8_t address_{0};
};

}  // namespace esphome::genibus
