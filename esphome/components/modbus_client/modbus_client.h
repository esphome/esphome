#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <array>
#include <span>
#include <vector>

namespace esphome::modbus_client {

// Most pending per-send response handlers a client tracks at once. A send whose reply handler would exceed
// this is dropped with a warning; ad-hoc traffic on a single bus never approaches it (the bus is one
// transaction in flight at a time).
static constexpr size_t MAX_PENDING_RESPONSES = 8;

/// Implemented by whatever wants the reply to a specific send routed back to it (the modbus_client.send
/// action). Kept non-template so the templated action can implement it. The three outcomes mirror the
/// modbus::ModbusClientDevice terminal callbacks: a data reply, an exception reply, or no reply.
class ModbusResponseHandler {
 public:
  // request_pdu / response_pdu are the request and reply PDUs (function code + data), only valid for the call.
  virtual void handle_modbus_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) = 0;
  virtual void handle_modbus_error(std::span<const uint8_t> request_pdu,
                                   modbus::ModbusExceptionCode exception_code) = 0;
  virtual void handle_modbus_no_response() = 0;
  virtual void handle_modbus_not_sent() = 0;

 protected:
  ~ModbusResponseHandler() = default;
};

/// A user-instantiated modbus client for ad-hoc request/response from a lambda or automation, without
/// hand-rolling a modbus::ModbusClientDevice subclass. Declared with an id, a hub and a device address; a
/// `modbus_client.send` action fires a PDU and the reply is delivered to the client-level on_response /
/// on_error / on_no_response callbacks and (if the send registered one) to a per-send handler. It is its
/// own hub device, so it must outlive the in-flight transaction - being a persistent component (a Pvariable
/// global) satisfies that. The on_* names track the YAML triggers and map straight onto the modbus device
/// callbacks (on_response / on_error / on_no_response / on_not_sent).
class ModbusCallbackClient : public modbus::ModbusClientDevice, public Component {
 public:
  void dump_config() override;

  /// Send an ad-hoc PDU (function code + data). The hub prepends the configured device address and appends
  /// the CRC. The response is routed back to this object's on_response/on_error/on_no_response callbacks below.
  void send(std::span<const uint8_t> pdu) { this->send_pdu(pdu); }
  /// Send an ad-hoc PDU and route this specific reply (or its lack) to `handler`. If an identical request
  /// is already pending the new send is skipped (the shared response path could not tell the two apart).
  void send_with_handler(std::span<const uint8_t> pdu, ModbusResponseHandler *handler);

  // Registration methods are templatized so they accept both std::function/lambdas (C++ users) and the
  // pointer-sized forwarder structs the YAML triggers register (no heap allocation for the automation path).
  template<typename F> void add_on_response(F &&callback) { this->response_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_error(F &&callback) { this->error_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_no_response(F &&callback) {
    this->no_response_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_not_sent(F &&callback) { this->not_sent_callback_.add(std::forward<F>(callback)); }

  // A data reply: hand the request and response PDUs (function code + data, no address/CRC) to on_response.
  // Both spans are only valid for the duration of the call.
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_callback_.call(request_pdu, response_pdu);
    if (auto *handler = this->take_pending_(request_pdu))
      handler->handle_modbus_response(request_pdu, response_pdu);
  }
  // An exception reply: the hub decodes the exception code from the fixed-format error response and hands it
  // here with the failed request PDU (the exception response carries nothing else useful).
  void on_error(std::span<const uint8_t> request_pdu, modbus::ModbusExceptionCode exception_code) override {
    this->error_callback_.call(request_pdu, exception_code);
    if (auto *handler = this->take_pending_(request_pdu))
      handler->handle_modbus_error(request_pdu, exception_code);
  }
  // No reply. Ad-hoc one-shots do not auto-retry; a caller that wants a retry re-sends from on_no_response.
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_callback_.call(request_pdu);
    if (auto *handler = this->take_pending_(request_pdu))
      handler->handle_modbus_no_response();
    return false;
  }
  // Never reached the wire (queue full, cleared, refused duplicate). Distinct from a timeout: fire the
  // client-level on_not_sent and resolve the per-send handler, so a pending entry never leaks.
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_callback_.call(request_pdu);
    if (auto *handler = this->take_pending_(request_pdu))
      handler->handle_modbus_not_sent();
  }

 protected:
  // Find the pending send whose request PDU matches `request` (exact bytes), remove it, and return its
  // handler (or nullptr). Defined in the .cpp. Removing before the caller invokes the handler keeps a
  // re-entrant send from the handler safe and avoids the response span aliasing a freed entry.
  ModbusResponseHandler *take_pending_(std::span<const uint8_t> request);

  /// A send awaiting its reply: the exact request PDU (to match the reply against) and the handler to notify.
  /// The PDU lives in a SmallInlineBuffer - inline for the common short frames, heap only for large writes.
  struct PendingSend {
    SmallInlineBuffer<modbus::MODBUS_FRAME_INLINE_SIZE> request;
    ModbusResponseHandler *handler{nullptr};
  };
  // Fixed storage (no per-send heap for the list itself); swap-erased on resolve. pending_count_ elements valid.
  std::array<PendingSend, MAX_PENDING_RESPONSES> pending_{};
  size_t pending_count_{0};

  LazyCallbackManager<void(std::span<const uint8_t>, std::span<const uint8_t>)> response_callback_{};
  LazyCallbackManager<void(std::span<const uint8_t>, modbus::ModbusExceptionCode)> error_callback_{};
  LazyCallbackManager<void(std::span<const uint8_t>)> no_response_callback_{};
  LazyCallbackManager<void(std::span<const uint8_t>)> not_sent_callback_{};
};

template<typename... Ts>
class ModbusClientSendAction : public Action<Ts...>,
                               public Parented<ModbusCallbackClient>,
                               public ModbusResponseHandler {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, pdu)

  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> *get_response_trigger() {
    return &this->response_trigger_;
  }
  Trigger<std::span<const uint8_t>, modbus::ModbusExceptionCode> *get_error_trigger() { return &this->error_trigger_; }
  Trigger<> *get_no_response_trigger() { return &this->no_response_trigger_; }
  Trigger<> *get_not_sent_trigger() { return &this->not_sent_trigger_; }
  void set_has_response_handlers(bool has_handlers) { this->has_response_handlers_ = has_handlers; }

  void play(Ts... x) override {
    auto pdu = this->pdu_.value(x...);
    if (pdu.empty())
      return;
    // Only register a per-send handler when an on_response/on_error/on_no_response is configured; otherwise
    // this is a plain fire-and-forget send with no pending bookkeeping. The reply arrives later
    // (fire-and-continue), so the triggers expose only the reply - not the outer automation's arguments.
    if (this->has_response_handlers_) {
      this->parent_->send_with_handler(std::span<const uint8_t>(pdu), this);
    } else {
      this->parent_->send(std::span<const uint8_t>(pdu));
    }
  }

  void handle_modbus_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_trigger_.trigger(request_pdu, response_pdu);
  }
  void handle_modbus_error(std::span<const uint8_t> request_pdu, modbus::ModbusExceptionCode exception_code) override {
    this->error_trigger_.trigger(request_pdu, exception_code);
  }
  void handle_modbus_no_response() override { this->no_response_trigger_.trigger(); }
  void handle_modbus_not_sent() override { this->not_sent_trigger_.trigger(); }

 protected:
  Trigger<std::span<const uint8_t>, std::span<const uint8_t>> response_trigger_;
  Trigger<std::span<const uint8_t>, modbus::ModbusExceptionCode> error_trigger_;
  Trigger<> no_response_trigger_;
  Trigger<> not_sent_trigger_;
  bool has_response_handlers_{false};
};

}  // namespace esphome::modbus_client
