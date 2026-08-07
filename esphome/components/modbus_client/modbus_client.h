#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"

#include <span>

namespace esphome::modbus_client {

/// Shared base for the modbus_client actions. Each ACTION INSTANCE is its own modbus::ModbusClientDevice:
/// the hub routes every reply (or its lack) straight back to the action that sent it, so there is no
/// central client object and no request matching. The device address is templatable; it is stamped on the
/// device at play() time; the hub routes each reply by device pointer, so a changed address never
/// mis-routes an earlier reply. (The address is not passed to the reply triggers - under overlapping
/// sends it could misreport, and the handler can recompute the expression it configured.)
template<typename... Ts> class ClientActionBase : public Action<Ts...>, public modbus::ModbusClientDevice {
 public:
  TEMPLATABLE_VALUE(uint8_t, target_address)  // the modbus device address

  Trigger<std::span<const uint8_t>> *get_sent_trigger() { return &this->sent_trigger_; }
  Trigger<std::span<const uint8_t>, modbus::ExceptionCode> *get_error_trigger() { return &this->error_trigger_; }
  Trigger<std::span<const uint8_t>> *get_no_response_trigger() { return &this->no_response_trigger_; }
  Trigger<std::span<const uint8_t>> *get_not_sent_trigger() { return &this->not_sent_trigger_; }

  /// The retry decision for on_no_response: given the request PDU, return true to have the hub re-queue
  /// the frame. Set from the lambda form or a then: automation's nested retry lambda; may coexist with
  /// the no_response trigger (actions run, then this decides the retry).
  using retry_func_t = bool (*)(std::span<const uint8_t>);
  void set_retry(retry_func_t f) { this->retry_func_ = f; }

  /// The frame was written to the wire: fires once per transmission, before any reply, and never for a
  /// send that ended in on_not_sent. request_pdu is the PDU sent (function code + data).
  void on_sent(std::span<const uint8_t> request_pdu) override { this->sent_trigger_.trigger(request_pdu); }
  /// Never reached the wire (tx queue full, cleared, or a duplicate write dropped by the hub's dedup).
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_trigger_.trigger(request_pdu); }
  /// A Modbus exception reply. Lives here beside its trigger so every action subclass gets the pairing:
  /// register_client_action() wires on_error for all of them, so a derived class must not have to
  /// remember the override.
  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override {
    this->error_trigger_.trigger(request_pdu, exception_code);
  }
  /// No reply within send_wait_time. Run the on_no_response actions (empty in the pure-lambda form),
  /// then let the retry lambda, if set, decide whether the hub re-queues the frame (true = retry). The
  /// two coexist: a then: automation can also carry a retry lambda. No lambda = no retry.
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_trigger_.trigger(request_pdu);
    if (this->retry_func_ != nullptr)
      return this->retry_func_(request_pdu);
    return false;
  }
  /// Stamp the templated device address before every play(): subclasses cannot forget it, and the hub
  /// routes each reply by device pointer, so a changed address never mis-routes earlier replies.
  void play_complex(const Ts &...x) override {
    this->set_address(this->target_address_.value(x...));
    Action<Ts...>::play_complex(x...);
  }

 protected:
  Trigger<std::span<const uint8_t>> sent_trigger_;
  Trigger<std::span<const uint8_t>, modbus::ExceptionCode> error_trigger_;
  Trigger<std::span<const uint8_t>> no_response_trigger_;
  Trigger<std::span<const uint8_t>> not_sent_trigger_;
  retry_func_t retry_func_{nullptr};
};

/// modbus_client.send: fire a raw PDU (function code + data; the hub adds address and CRC). The reply is
/// delivered raw - on_response(request, response) - deliberately bypassing the typed dispatch, so
/// non-standard/custom transactions pass through untouched.
/// The PDU is a stack-allocated modbus::helpers::PduBuffer, so a pdu lambda can build one with the
/// modbus::helpers::create_*_pdu() builders and return it directly (smaller builder results convert).
/// A PduBuffer drops bytes past modbus::MAX_PDU_SIZE without reporting it (the hub's oversize check
/// cannot fire - that limit is the capacity), so an over-long lambda-built PDU is silently truncated.
template<typename... Ts> class ModbusClientSendAction : public ClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(modbus::helpers::PduBuffer, pdu)

  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> *get_response_trigger() {
    return &this->response_trigger_;
  }

  void play(const Ts &...x) override {
    auto pdu = this->pdu_.value(x...);
    const std::span<const uint8_t> span(pdu.data(), pdu.size());
    // The hub refuses some sends at the door with no callback (an empty PDU, a duplicate write already
    // pending, a full queue). Every send still gets exactly one outcome, so resolve those via on_not_sent.
    if (!this->send_pdu(span))
      this->on_not_sent(span);
  }

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_trigger_.trigger(request_pdu, response_pdu);
  }

 protected:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> response_trigger_;
};

}  // namespace esphome::modbus_client
