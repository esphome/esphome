#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"

#include <span>
#include <vector>

namespace esphome::modbus_client {

/// Shared base for the modbus_client actions. Each ACTION INSTANCE is its own modbus::ModbusClientDevice:
/// the hub routes every reply (or its lack) straight back to the action that sent it, so there is no
/// central client object and no request matching. The device address is templatable; it is stamped on the
/// device at play() time and passed to every outcome trigger, so handlers know which device they are
/// talking to even when the address changes between sends. Overlapping sends from one action with a
/// changing address report the most recently sent address - send sequentially when the address varies.
/// Re-firing an action while its identical frame is still pending follows the hub's dedup rules: a
/// duplicate read is absorbed into the pending transaction (one reply serves both), a duplicate write is
/// dropped and resolves via on_not_sent.
template<typename... Ts> class ClientActionBase : public Action<Ts...>, public modbus::ModbusClientDevice {
 public:
  TEMPLATABLE_VALUE(uint16_t, target_address)  // device address 1-247, or 0 to broadcast (no reply)

  Trigger<uint8_t, modbus::ModbusExceptionCode> *get_error_trigger() { return &this->error_trigger_; }
  Trigger<uint8_t> *get_no_response_trigger() { return &this->no_response_trigger_; }
  Trigger<uint8_t> *get_not_sent_trigger() { return &this->not_sent_trigger_; }

  /// Never reached the wire (tx queue full, cleared, or a duplicate write dropped by the hub's dedup).
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_trigger_.trigger(this->address_); }
  /// No reply. Ad-hoc sends do not auto-retry; a handler that wants a retry re-sends from on_no_response.
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_trigger_.trigger(this->address_);
    return false;
  }

 protected:
  /// Stamp the templated device address for this send; the hub routes the reply by device pointer, so a
  /// changed address never mis-routes earlier replies.
  void stamp_address_(const Ts &...x) { this->set_address(static_cast<uint8_t>(this->target_address_.value(x...))); }

  Trigger<uint8_t, modbus::ModbusExceptionCode> error_trigger_;
  Trigger<uint8_t> no_response_trigger_;
  Trigger<uint8_t> not_sent_trigger_;
};

/// modbus_client.send: fire a raw PDU (function code + data; the hub adds address and CRC). The reply is
/// delivered raw - on_response(address, request, response) - deliberately bypassing the typed dispatch, so
/// non-standard/custom transactions pass through untouched.
template<typename... Ts> class ModbusClientSendAction : public ClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, pdu)

  Trigger<uint8_t, std::span<const uint8_t>, std::span<const uint8_t>> *get_response_trigger() {
    return &this->response_trigger_;
  }

  void play(const Ts &...x) override {
    auto pdu = this->pdu_.value(x...);
    if (pdu.empty())
      return;
    this->stamp_address_(x...);
    this->send_pdu(std::span<const uint8_t>(pdu));
  }

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_trigger_.trigger(this->address_, request_pdu, response_pdu);
  }
  void on_error(std::span<const uint8_t> request_pdu, modbus::ModbusExceptionCode exception_code) override {
    this->error_trigger_.trigger(this->address_, exception_code);
  }

 protected:
  Trigger<uint8_t, std::span<const uint8_t>, std::span<const uint8_t>> response_trigger_;
};

}  // namespace esphome::modbus_client
