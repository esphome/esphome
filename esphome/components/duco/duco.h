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
  uint8_t function;
  uint8_t id;
  const uint8_t *data;
  uint8_t size;
};

class DucoDevice;

class Duco : public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;

  void set_disable_crc(bool disable_crc) { this->disable_crc_ = disable_crc; }

  /// Send a request; the answers carrying its message id go to device until it returns true.
  uint8_t send(DucoDevice *device, uint8_t function, std::initializer_list<uint8_t> data);

 protected:
  static constexpr size_t MAX_FRAME_SIZE = 64;
  static constexpr size_t MAX_REQUEST_DATA = 16;
  static constexpr uint8_t PENDING_SLOTS = 8;

  bool parse_byte_(uint8_t byte);
  void finalize_message_();
  uint8_t next_id_();

  std::array<DucoDevice *, PENDING_SLOTS> pending_device_{};
  std::array<uint8_t, PENDING_SLOTS> pending_id_{};
  std::array<uint8_t, MAX_FRAME_SIZE> rx_buffer_{};
  uint32_t last_byte_{0};
  uint8_t rx_size_{0};
  uint8_t last_id_{10};
  bool removed_last_{false};
  bool disable_crc_{false};
};

class DucoDevice : public Parented<Duco> {
 public:
  /// Return true once the request is answered, so later messages with the same id are dropped.
  virtual bool receive_response(const DucoMessage &message) = 0;
};

class DucoDiscovery : public DucoDevice, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  bool receive_response(const DucoMessage &message) override;

 protected:
  static constexpr uint8_t NODE_COUNT = 0x45;

  void request_next_node_();

  std::array<uint8_t, NODE_COUNT> node_types_{};  // 0 when no node answered at that address
  uint8_t next_node_{0};
  uint8_t request_id_{0};
  bool waiting_{false};
};

#ifdef USE_TIME
class DucoTime : public DucoDevice, public PollingComponent {
 public:
  explicit DucoTime(time::RealTimeClock *clock) : clock_(clock) {}
  void update() override;
  bool receive_response(const DucoMessage &message) override { return message.function == FUNCTION_PARAMETER + 2; }

 protected:
  time::RealTimeClock *clock_;
};
#endif

}  // namespace esphome::duco
