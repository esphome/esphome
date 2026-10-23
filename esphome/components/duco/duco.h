#pragma once

#include <array>
#include <initializer_list>

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome::duco {

// Function codes of the requests; the box answers a request with function + 2
static constexpr uint8_t FUNCTION_NODE = 0x0c;       // discovery, status and mode of a node
static constexpr uint8_t FUNCTION_NODE_DATA = 0x10;  // a block of a node's data (sensors, serial number)
static constexpr uint8_t FUNCTION_PARAMETER = 0x24;  // read or write a box parameter, set the clock

/// A received message; data points into the receive buffer and is valid only during the callback.
struct DucoMessage {
  uint8_t id;
  const uint8_t *data;
  uint8_t size;
};

class DucoDevice;

/// The open request of one device: the answer to it is function + 2 with the same message id.
struct DucoPendingRequest {
  DucoDevice *device;
  uint8_t id;
  uint8_t function;  // 0 while nothing is open
  bool missed;       // the request before this one got no answer; sits in padding
};

class Duco : public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;

  void set_disable_crc(bool disable_crc) { this->disable_crc_ = disable_crc; }

#ifdef DUCO_DEVICE_COUNT
  void register_device(DucoDevice *device) { this->pending_.push_back({device, 0, 0, false}); }
#endif

  /// Send a request; its answer goes to device, or nowhere if nullptr. A device's new request replaces its
  /// open one, so an answer is never lost to another device's traffic.
  void send(DucoDevice *device, uint8_t function, std::initializer_list<uint8_t> data);

 protected:
  // The longest answer to a request sent here is 25 bytes (node status), the longest request 9
  static constexpr size_t MAX_FRAME_SIZE = 32;
  static constexpr size_t MAX_REQUEST_DATA = 9;

  bool parse_byte_(uint8_t byte);
#ifdef DUCO_DEVICE_COUNT
  void update_warning_();  // warn while any device's previous request went unanswered
#endif
  void finalize_message_();
  uint8_t next_id_();

#ifdef DUCO_DEVICE_COUNT
  StaticVector<DucoPendingRequest, DUCO_DEVICE_COUNT> pending_;
#endif
  std::array<uint8_t, MAX_FRAME_SIZE> rx_buffer_{};
  uint32_t last_byte_{0};
  uint8_t rx_size_{0};
  uint8_t last_id_{10};
  bool escaped_{false};  // the last 0xAA was followed by a stuffing byte, so it starts no header
  bool disable_crc_{false};
};

/// Every entity polls the box; deriving from PollingComponent shares its vtable.
class DucoDevice : public PollingComponent, public Parented<Duco> {
 public:
  virtual void receive_response(const DucoMessage &message) = 0;
};

/// Walks the node addresses once after boot; the nodes found are listed in the config dump.
class DucoDiscovery : public DucoDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;  // one discovery step, run by the step interval
  void receive_response(const DucoMessage &message) override;

 protected:
  static constexpr uint8_t NODE_COUNT = 0x45;

  std::array<uint8_t, NODE_COUNT> node_types_{};  // 0 when no node answered at that address
  uint8_t next_node_{0};
  uint8_t attempts_{0};
};

#ifdef USE_TIME
class DucoTime : public PollingComponent, public Parented<Duco> {
 public:
  explicit DucoTime(time::RealTimeClock *clock) : clock_(clock) {}
  void update() override;

 protected:
  time::RealTimeClock *clock_;
};
#endif

}  // namespace esphome::duco
