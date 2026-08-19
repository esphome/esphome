#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
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
  /// Never reached the wire, from either of two sources. The hub calls this for a request it accepted
  /// and then dropped, which happens only when clear_tx_queue_for_address() retires it - a modbus
  /// device going offline, say. Everything the hub refuses at the door instead returns false from
  /// queue_pdu() with no callback at all, so send_or_resolve_() below turns those into this same
  /// callback: a full queue, a duplicate write, or an empty PDU from a rejecting builder.
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
  /// send still gets exactly one outcome (a broadcast (address 0) is the exception - never answered, it
  /// resolves through on_sent() alone), so resolve refusals here via on_not_sent.
  /// Takes a span, not a PduBuffer: the builders return right-sized buffers (a read PDU is 5 bytes), and
  /// a PduBuffer parameter would widen each one to the 253-byte maximum just to cross the call.
  void send_or_resolve_(std::span<const uint8_t> pdu) {
    if (!this->queue_pdu(pdu))
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

/// Typed actions: these do NOT override the raw on_response, so the base ModbusClientDevice default runs
/// the shared dispatch (validation gate + decode) and the typed callbacks below fire directly on the
/// action. A reply the gate diverts (not a standard-conformant transaction) fires the on_custom_response
/// trigger with the raw request/response PDUs, so non-standard replies stay handleable; the spans are only
/// valid for the duration of the trigger. (For a typed-built request the gate can only divert on the
/// response, never with an exception status - real device exceptions arrive via on_error, which
/// ClientActionBase already routes straight to its trigger, so the typed callbacks below only ever see a
/// success status.) Each typed callback still checks succeeded() before firing its trigger: that branch
/// is unreachable today, and is kept so a future change to that interception cannot silently deliver an
/// exception as a successful reply.
template<typename... Ts> class TypedClientActionBase : public ClientActionBase<Ts...> {
 public:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> *get_custom_response_trigger() {
    return &this->custom_response_trigger_;
  }
  /// Set by codegen when the config declares on_custom_response. Without it an unhandled diverted reply
  /// would fire an empty trigger and vanish, so the base's warn-once diagnostic has to stay reachable.
  void set_custom_response_handled() { this->custom_response_handled_ = true; }

  void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          modbus::ResponseStatus status) override {
    if (!this->custom_response_handled_) {
      modbus::ModbusClientDevice::on_custom_response(request_pdu, response_pdu, status);
      return;
    }
    this->custom_response_trigger_.trigger(request_pdu, response_pdu);
  }

 protected:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> custom_response_trigger_;
  bool custom_response_handled_{false};
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
    if (modbus::succeeded(status))
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
    if (modbus::succeeded(status))
      this->response_trigger_.trigger(bits);
  }

 protected:
  Trigger<modbus::PackedBits> response_trigger_;
  bool coils_;
};

/// modbus_client.write_single_register: on_response is the acknowledgement (the ack only echoes the
/// request, so it carries no arguments).
template<typename... Ts> class WriteSingleRegisterAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(uint16_t, value)

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    this->send_or_resolve_(
        modbus::helpers::create_write_single_register_pdu(this->start_address_.value(x...), this->value_.value(x...)));
  }
  void on_write_single_register(uint16_t address, uint16_t value, modbus::ResponseStatus status) override {
    if (modbus::succeeded(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
};

/// modbus_client.write_single_coil: on_response is the acknowledgement (no arguments). A coil holds one
/// bit, so the value is a bool - the wire only ever carries 0x0000 or 0xFF00.
template<typename... Ts> class WriteSingleCoilAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, start_address)
  TEMPLATABLE_VALUE(bool, value)

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    this->send_or_resolve_(
        modbus::helpers::create_write_single_coil_pdu(this->start_address_.value(x...), this->value_.value(x...)));
  }
  void on_write_single_coil(uint16_t address, bool value, modbus::ResponseStatus status) override {
    if (modbus::succeeded(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
};

/// modbus_client.write_multiple_registers: on_response is the acknowledgement (no arguments).
/// A `values:` list is emitted as a flash array and sent straight from there; only a lambda builds a
/// vector, and only when it runs. Same split as canbus's send action, and for the same reason: a static
/// list must not allocate on every play().
template<typename... Ts> class WriteMultipleRegistersAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, start_address)

  /// Static config: the registers live in flash, so play() neither allocates nor copies.
  void set_values_static(const uint16_t *values, size_t len) {
    this->values_.data = values;
    this->len_ = static_cast<ssize_t>(len);
  }
  /// Lambda config: the registers are only known at play() time. Stateless lambdas (all ESPHome
  /// generates) convert to a plain function pointer, so this stays pointer-sized.
  void set_values_template(std::vector<uint16_t> (*func)(Ts...)) {
    this->values_.func = func;
    this->len_ = -1;  // sentinel: template mode
  }

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    const uint16_t start = this->start_address_.value(x...);
    // An empty or over-long set rejects into an empty PDU inside the builder, which logs the reason;
    // the empty PDU then resolves via on_not_sent like any refused send.
    if (this->len_ >= 0) {
      this->send_or_resolve_(modbus::helpers::create_write_registers_pdu(
          start, std::span<const uint16_t>(this->values_.data, static_cast<size_t>(this->len_))));
      return;
    }
    const std::vector<uint16_t> values = this->values_.func(x...);
    this->send_or_resolve_(modbus::helpers::create_write_registers_pdu(start, std::span<const uint16_t>(values)));
  }
  void on_write_multiple_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                   modbus::ResponseStatus status) override {
    if (modbus::succeeded(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
  ssize_t len_{-1};  // -1 = template mode, >= 0 = static mode with this many registers
  union Values {
    std::vector<uint16_t> (*func)(Ts...);
    const uint16_t *data;
  } values_;
};

/// modbus_client.write_multiple_coils: on_response is the acknowledgement (no arguments).
/// A `values:` list is packed into wire layout at code-generation time and stored in flash, so play()
/// neither allocates nor packs. A lambda returns std::vector<bool> - already a bit per coil rather than
/// a byte - and is packed into a stack buffer on the way to the builder.
template<typename... Ts> class WriteMultipleCoilsAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, start_address)

  /// Static config: `packed` is the wire layout (LSB first) held in flash, `count` the number of coils.
  void set_values_static(const uint8_t *packed, size_t count) {
    this->values_.packed = packed;
    this->count_ = static_cast<ssize_t>(count);
  }
  /// Lambda config: the coils are only known at play() time.
  void set_values_template(std::vector<bool> (*func)(Ts...)) {
    this->values_.func = func;
    this->count_ = -1;  // sentinel: template mode
  }

  Trigger<> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    const uint16_t start = this->start_address_.value(x...);
    if (this->count_ >= 0) {
      const auto count = static_cast<uint16_t>(this->count_);
      this->send_or_resolve_(modbus::helpers::create_write_coils_pdu(
          start,
          modbus::PackedBits(std::span<const uint8_t>(this->values_.packed, modbus::packed_bit_bytes(count)), count)));
      return;
    }
    // The builder packs and bound-checks; an over-long set is rejected and logged there.
    this->send_or_resolve_(modbus::helpers::create_write_coils_pdu(start, this->values_.func(x...)));
  }
  void on_write_multiple_coils(uint16_t start_address, modbus::PackedBits bits,
                               modbus::ResponseStatus status) override {
    if (modbus::succeeded(status))
      this->response_trigger_.trigger();
  }

 protected:
  Trigger<> response_trigger_;
  ssize_t count_{-1};  // -1 = template mode, >= 0 = static mode with this many coils
  union Values {
    std::vector<bool> (*func)(Ts...);
    const uint8_t *packed;
  } values_;
};

/// modbus_client.read_write_multiple_registers (FC 0x17): writes one register block and reads another back in
/// one transaction (write first, per Modbus 6.17). on_response delivers the read-back words as `values`.
template<typename... Ts> class ReadWriteMultipleRegistersAction : public TypedClientActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, read_address)
  TEMPLATABLE_VALUE(uint16_t, read_count)
  TEMPLATABLE_VALUE(uint16_t, write_address)

  /// Static config: the write registers live in flash, so play() neither allocates nor copies.
  void set_values_static(const uint16_t *values, size_t len) {
    this->values_.data = values;
    this->len_ = static_cast<ssize_t>(len);
  }
  /// Lambda config: the write registers are only known at play() time.
  void set_values_template(std::vector<uint16_t> (*func)(Ts...)) {
    this->values_.func = func;
    this->len_ = -1;  // sentinel: template mode
  }

  Trigger<std::span<const uint16_t>> *get_response_trigger() { return &this->response_trigger_; }

  void play(const Ts &...x) override {
    const uint16_t read_start = this->read_address_.value(x...);
    const uint16_t read_count = this->read_count_.value(x...);
    const uint16_t write_start = this->write_address_.value(x...);
    // An out-of-range read/write count builds an empty PDU (the builder logs why), resolving via on_not_sent.
    if (this->len_ >= 0) {
      this->send_or_resolve_(modbus::helpers::create_read_write_multiple_registers_pdu(
          read_start, read_count, write_start,
          std::span<const uint16_t>(this->values_.data, static_cast<size_t>(this->len_))));
      return;
    }
    const std::vector<uint16_t> values = this->values_.func(x...);
    this->send_or_resolve_(modbus::helpers::create_read_write_multiple_registers_pdu(
        read_start, read_count, write_start, std::span<const uint16_t>(values)));
  }
  // The 0x17 response carries only the read block, so the hub dispatch delivers it as a holding-register read.
  void on_read_registers(modbus::EntityType entity_type, uint16_t start_address, std::span<const uint16_t> registers,
                         modbus::ResponseStatus status) override {
    if (modbus::succeeded(status))
      this->response_trigger_.trigger(registers);
  }

 protected:
  Trigger<std::span<const uint16_t>> response_trigger_;
  ssize_t len_{-1};  // -1 = template mode, >= 0 = static mode with this many write registers
  union Values {
    std::vector<uint16_t> (*func)(Ts...);
    const uint16_t *data;
  } values_;
};

}  // namespace esphome::modbus_client
