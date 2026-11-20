#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VBUS_TX
#include <array>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

namespace esphome::vbus {

class VBus;

class VBusTransmitter final : public Component, public uart::UARTDevice {
 public:
  explicit VBusTransmitter(VBus *receiver) : receiver_(receiver) {}
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  void set_source(uint16_t source) { this->source_ = source; }
  void set_destination(uint16_t destination) { this->destination_ = destination; }
  void set_lease_duration(uint32_t duration) { this->lease_duration_ms_ = duration; }
  template<typename F> void add_on_bus_grant_callback(F &&callback) {
    this->grant_callback_.add(std::forward<F>(callback));
  }

  bool request_bus();
  bool has_bus() const;
  bool send_datagram(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value, bool expect_reply = true);
  void release_bus(uint32_t grant_id);

  void observe_rx(size_t available);
  void observe_byte(uint8_t byte);
  void on_header(uint8_t protocol, uint16_t source, uint16_t destination);
  void on_datagram(uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id, uint32_t value);

 protected:
  enum class State : uint8_t { STATE_IDLE, STATE_WAIT_GRANT, STATE_CONTEND, STATE_OWNED };
  bool can_send_() const;
  void send_(uint16_t command, uint16_t value_id, uint32_t value, bool expect_reply = false);
  void stop_();

  VBus *receiver_;
  uint16_t source_{0x0020};
  uint16_t destination_{0};
  uint32_t lease_duration_ms_{500};
  State state_{State::STATE_IDLE};
  uint32_t requested_at_{0};
  uint32_t granted_at_{0};
  uint32_t observed_at_{0};
  uint32_t packet_at_{0};
  uint32_t sent_at_{0};
  uint16_t sent_command_{0};
  uint16_t sent_value_id_{0};
  uint32_t sent_value_{0};
  uint32_t grant_id_{0};
  size_t rx_unread_{0};
  bool observed_{false};
  bool rx_fresh_{false};
  bool packet_fresh_{false};
  bool packet_pending_{false};
  bool sent_{false};
  bool release_pending_{false};
  bool awaiting_reply_{false};
  bool stopped_{false};
  CallbackManager<void(uint32_t)> grant_callback_;
};

}  // namespace esphome::vbus
#endif
