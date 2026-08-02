#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"

#include <span>
#include <vector>

namespace esphome::modbus_client {

/// Shared base for the modbus_client actions. Each ACTION INSTANCE is its own modbus::ModbusClientDevice:
/// the hub routes every reply (or its lack) straight back to the action that sent it, so there is no
/// central client object and no request matching. The device address is templatable; it is stamped on the
/// device at play() time; the hub routes each reply by device pointer, so a changed address never
/// mis-routes an earlier reply. (The address is not passed to the reply triggers - under overlapping
/// sends it could misreport, and the handler can recompute the expression it configured.)
/// Re-firing an action while its identical frame is still pending follows the hub's dedup rules: a
/// duplicate read is absorbed into the pending transaction (one reply serves both), a duplicate write is
/// dropped and resolves via on_not_sent.
template<typename... Ts> class ClientActionBase : public Action<Ts...>, public modbus::ModbusClientDevice {
 public:
  TEMPLATABLE_VALUE(uint8_t, target_address)  // device address 1-247, or 0 to broadcast (no reply)

  Trigger<std::span<const uint8_t>, modbus::ModbusExceptionCode> *get_error_trigger() { return &this->error_trigger_; }
  Trigger<> *get_no_response_trigger() { return &this->no_response_trigger_; }
  Trigger<> *get_not_sent_trigger() { return &this->not_sent_trigger_; }

  /// Never reached the wire (tx queue full, cleared, or a duplicate write dropped by the hub's dedup).
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_trigger_.trigger(); }
  /// No reply. Ad-hoc sends do not auto-retry; a handler that wants a retry re-sends from on_no_response.
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_trigger_.trigger();
    return false;
  }
  /// Stamp the templated device address before every play(): subclasses cannot forget it, and the hub
  /// routes each reply by device pointer, so a changed address never mis-routes earlier replies.
  void play_complex(const Ts &...x) override {
    this->set_address(this->target_address_.value(x...));
    Action<Ts...>::play_complex(x...);
  }

 protected:
  Trigger<std::span<const uint8_t>, modbus::ModbusExceptionCode> error_trigger_;
  Trigger<> no_response_trigger_;
  Trigger<> not_sent_trigger_;
};

/// modbus_client.send: fire a raw PDU (function code + data; the hub adds address and CRC). The reply is
/// delivered raw - on_response(request, response) - deliberately bypassing the typed dispatch, so
/// non-standard/custom transactions pass through untouched.
template<typename... Ts> class ModbusClientSendAction : public ClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, pdu)

  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> *get_response_trigger() {
    return &this->response_trigger_;
  }

  void play(const Ts &...x) override {
    auto pdu = this->pdu_.value(x...);
    if (!pdu.empty())
      this->send_pdu(std::span<const uint8_t>(pdu));
  }

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_trigger_.trigger(request_pdu, response_pdu);
  }
  void on_error(std::span<const uint8_t> request_pdu, modbus::ModbusExceptionCode exception_code) override {
    this->error_trigger_.trigger(request_pdu, exception_code);
  }

 protected:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> response_trigger_;
};

}  // namespace esphome::modbus_client
