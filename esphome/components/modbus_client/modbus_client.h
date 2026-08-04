#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"

#include <array>
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
  /// The hub refuses some sends at the door with no callback (a duplicate write already pending, a full
  /// queue, or an empty PDU - which is how the create_*_pdu() builders reject out-of-spec input). Every
  /// send still gets exactly one outcome, so resolve refusals here via on_not_sent.
  /// Takes a span, not a PduBuffer: the builders return right-sized buffers (a read PDU is 5 bytes), and
  /// a PduBuffer parameter would widen each one to the 253-byte maximum just to cross the call.
  void send_or_resolve_(std::span<const uint8_t> pdu) {
    if (!this->send_pdu(pdu))
      this->on_not_sent(pdu);
  }

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

  void play(const Ts &...x) override { this->send_or_resolve_(this->pdu_.value(x...)); }

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_trigger_.trigger(request_pdu, response_pdu);
  }

 protected:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> response_trigger_;
};

/// Typed actions: these do NOT override the raw on_response/on_error, so the base ModbusClientDevice
/// defaults run the shared dispatch (validation gate + decode) and the typed callbacks below fire directly
/// on the action. A reply the gate diverts (not a standard-conformant transaction) fires the
/// on_custom_response trigger with the raw request/response PDUs, so non-standard replies stay handleable;
/// the spans are only valid for the duration of the trigger. (For a typed-built request the gate can only
/// divert on the response, never with an exception status - real device exceptions arrive via on_error.)
template<typename... Ts> class TypedClientActionBase : public ClientActionBase<Ts...> {
 public:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> *get_custom_response_trigger() {
    return &this->custom_response_trigger_;
  }

  void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          modbus::ResponseStatus status) override {
    this->custom_response_trigger_.trigger(request_pdu, response_pdu);
  }

  /// A device exception enters here, where the request PDU is still at hand: fire on_error directly
  /// instead of running the base dispatch, so the typed callbacks below only ever see a success status.
  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override {
    this->error_trigger_.trigger(request_pdu, exception_code);
  }

 protected:
  /// True when the reply carries no exception; exception replies fired on_error above instead.
  bool check_status_(modbus::ResponseStatus status) { return !status.has_value(); }

  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> custom_response_trigger_;
};

/// modbus_client.read_holding_registers / read_input_registers: on_response delivers the registers in
/// host byte order as `values` (only valid for the duration of the trigger).
template<typename... Ts> class ReadRegistersAction : public TypedClientActionBase<Ts...> {
 public:
  explicit ReadRegistersAction(bool holding) : holding_(holding) {}
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(uint16_t, count)

  Trigger<std::span<const uint16_t>> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    const auto function_code =
        this->holding_ ? modbus::FunctionCode::READ_HOLDING_REGISTERS : modbus::FunctionCode::READ_INPUT_REGISTERS;
    this->send_or_resolve_(
        modbus::helpers::create_read_pdu(function_code, this->start_address_.value(x...), this->count_.value(x...)));
  }
  void on_read_registers(modbus::EntityType entity_type, uint16_t start_address, std::span<const uint16_t> registers,
                         modbus::ResponseStatus status) override {
    if (this->check_status_(status))
      this->response_trigger_.trigger(registers);
  }

 protected:
  Trigger<std::span<const uint16_t>> response_trigger_;
  bool holding_;
};

/// modbus_client.read_coils / read_discrete_inputs: on_response delivers the bits as a PackedBits view
/// (bit 0 = the bit at start_address; only valid for the duration of the trigger).
template<typename... Ts> class ReadBitsAction : public TypedClientActionBase<Ts...> {
 public:
  explicit ReadBitsAction(bool coils) : coils_(coils) {}
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(uint16_t, count)

  Trigger<modbus::PackedBits> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    const auto function_code =
        this->coils_ ? modbus::FunctionCode::READ_COILS : modbus::FunctionCode::READ_DISCRETE_INPUTS;
    this->send_or_resolve_(
        modbus::helpers::create_read_pdu(function_code, this->start_address_.value(x...), this->count_.value(x...)));
  }
  void on_read_bits(modbus::EntityType entity_type, uint16_t start_address, modbus::PackedBits bits,
                    modbus::ResponseStatus status) override {
    if (this->check_status_(status))
      this->response_trigger_.trigger(bits);
  }

 protected:
  Trigger<modbus::PackedBits> response_trigger_;
  bool coils_;
};

/// modbus_client.write_single_register / write_single_coil: on_response is the acknowledgement (the ack
/// only echoes the request, so it carries no arguments).
template<typename... Ts> class WriteSingleAction : public TypedClientActionBase<Ts...> {
 public:
  explicit WriteSingleAction(bool coil) : coil_(coil) {}
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(uint16_t, value)

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    const uint16_t start = this->start_address_.value(x...);
    const uint16_t value = this->value_.value(x...);
    this->send_or_resolve_(this->coil_ ? modbus::helpers::create_write_single_coil_pdu(start, value != 0)
                                       : modbus::helpers::create_write_single_register_pdu(start, value));
  }
  void on_write_single_register(uint16_t address, uint16_t value, modbus::ResponseStatus status) override {
    if (this->check_status_(status))
      this->response_trigger_.trigger();
  }
  void on_write_single_coil(uint16_t address, bool value, modbus::ResponseStatus status) override {
    if (this->check_status_(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
  bool coil_;
};

/// modbus_client.write_multiple_registers: on_response is the acknowledgement (no arguments).
template<typename... Ts> class WriteMultipleRegistersAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(std::vector<uint16_t>, values)

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    auto values = this->values_.value(x...);
    // An empty or over-long set rejects into an empty PDU inside the builder and resolves via on_not_sent.
    this->send_or_resolve_(modbus::helpers::create_write_registers_pdu(this->start_address_.value(x...),
                                                                       std::span<const uint16_t>(values)));
  }
  void on_write_multiple_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                   modbus::ResponseStatus status) override {
    if (this->check_status_(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
};

/// modbus_client.write_multiple_coils: on_response is the acknowledgement (no arguments). The
/// values arrive as bytes (std::vector<bool> cannot bind to std::span<const bool>) and are packed into
/// the wire layout for the base's packed write_multiple_coils() overload.
template<typename... Ts> class WriteMultipleCoilsAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, values)

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    auto values = this->values_.value(x...);
    if (values.empty() || values.size() > modbus::MAX_NUM_OF_COILS_TO_WRITE) {
      // An out-of-spec set cannot be packed into the stack buffer; resolve it like any refused send.
      this->on_not_sent({});
      return;
    }
    // Transient pack on the stack: ceil(1968 / 8) = 246 bytes for the spec-maximum write.
    std::array<uint8_t, (modbus::MAX_NUM_OF_COILS_TO_WRITE + 7) / 8> buf{};
    modbus::MutablePackedBits bits(std::span<uint8_t>(buf.data(), (values.size() + 7) / 8),
                                   static_cast<uint16_t>(values.size()));
    for (size_t i = 0; i < values.size(); i++)
      bits.set(i, values[i] != 0);
    this->send_or_resolve_(modbus::helpers::create_write_coils_pdu(this->start_address_.value(x...), bits));
  }
  void on_write_multiple_coils(uint16_t start_address, modbus::PackedBits bits,
                               modbus::ResponseStatus status) override {
    if (this->check_status_(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
};

}  // namespace esphome::modbus_client
