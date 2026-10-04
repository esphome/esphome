#include "modbus.h"

#ifdef USE_MODBUS_TCP

#include "mbap.h"
#include "mbap_link.h"
#include "modbus_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::modbus {

static const char *const TAG = "modbus";

static constexpr uint32_t FORWARD_STUCK_US = 2000000;
static constexpr size_t WIRE_SIZE = 260;

// The client sends requests on uart_id. The server answers on peer_id.
static constexpr int CLIENT = 0;
static constexpr int SERVER = 1;

enum class ForwardPhase : uint8_t {
  FORWARD_PHASE_IDLE,
  FORWARD_PHASE_SEND,
  FORWARD_PHASE_WAIT,
  FORWARD_PHASE_REPLY,
};

struct ForwardState {
  struct Wire {
    uart::UARTComponent *uart{nullptr};
    MbapLink *link{nullptr};
    GPIOPin *flow{nullptr};
    RtuTiming timing{};
    uint32_t baud{1};
    uint32_t max_frame_us{0};
    uint8_t buf[WIRE_SIZE]{};
    uint16_t len{0};
    uint16_t last_tx_len{0};
    uint32_t last_rx_us{0};
    uint32_t last_tx_us{0};
  };

  Wire side[2];
  uint8_t req[MAX_FRAME_SIZE]{};
  uint8_t resp[MAX_FRAME_SIZE]{};
  uint8_t next[MAX_FRAME_SIZE]{};
  uint16_t req_len{0};
  uint16_t resp_len{0};
  uint16_t next_len{0};
  ForwardPhase phase{ForwardPhase::FORWARD_PHASE_IDLE};
  // An RTU client sent a newer request, so the reply to this one is not delivered.
  bool abandoned{false};
  uint32_t wait_us{2000000};
  uint32_t turnaround_us{200000};
  uint32_t sent_us{0};
  uint32_t reply_due_us{0};
  // After a timeout on an RTU server bus a late reply may still arrive. Nothing is sent until it has passed.
  uint32_t quiet_from_us{0};
  uint32_t quiet_us{0};
  // The quiet window follows a logged time-out, so what the server sends in it is a late reply.
  bool late_hint{false};
  uint32_t log_ms{0};
  uint32_t blocked_since_us{0};
};

static uint32_t frame_us(const ForwardState::Wire &wire, uint32_t len) {
  return static_cast<uint32_t>((static_cast<uint64_t>(len) * wire.timing.bits_per_char * 1000000ull) / wire.baud);
}

// A frame may arrive in bursts as the UART buffer fills, so only a longer gap ends or abandons it.
static uint32_t rx_gap_us(const ForwardState::Wire &wire) {
  return std::max(wire.timing.frame_delay_us, wire.timing.long_rx_buffer_delay_us);
}

static void pump_rtu(ForwardState::Wire &wire, uint32_t &log_ms) {
  if (wire.uart == nullptr || !wire.uart->is_connected()) {
    wire.len = 0;
    return;
  }
  size_t room = sizeof(wire.buf) - wire.len;
  if (room == 0 || wire.uart->available() == 0) {
    return;
  }
  size_t n = std::min(room, wire.uart->available());
  if (!wire.uart->read_array(wire.buf + wire.len, n)) {
    log_throttled(log_ms, LOG_STR("Read failed"));
    return;
  }
  wire.len += static_cast<uint16_t>(n);
  wire.last_rx_us = micros();
}

static bool consume(ForwardState::Wire &wire, size_t used) {
  if (used > wire.len) {
    return false;
  }
  wire.len = static_cast<uint16_t>(wire.len - used);
  if (wire.len != 0) {
    std::memmove(wire.buf, wire.buf + used, wire.len);
  }
  return true;
}

static bool take_rtu_wire(ForwardState::Wire &wire, uint8_t *dst, uint16_t *out_len, bool response, uint32_t &log_ms) {
  if (wire.len == 0) {
    return false;
  }
  const bool known = wire.len >= 2 && !helpers::is_function_code_unknown_length(wire.buf[1]);
  if (known) {
    uint16_t need =
        response ? helpers::server_frame_length(wire.buf, wire.len) : helpers::client_frame_length(wire.buf, wire.len);
    if (need >= 4 && need <= MAX_FRAME_SIZE && wire.len >= need) {
      if (crc16(wire.buf, need) == 0) {
        std::memcpy(dst, wire.buf, need);
        *out_len = need;
        consume(wire, need);
        return true;
      }
      log_throttled(log_ms, LOG_STR("Modbus CRC failed, frame dropped"));
      wire.len = 0;
      return false;
    }
    // The known length is not here yet. RTU ends on the 3.5-character gap.
    if (micros() - wire.last_rx_us < rx_gap_us(wire)) {
      return false;
    }
    log_throttled(log_ms, LOG_STR("Incomplete Modbus frame dropped"));
    wire.len = 0;
    return false;
  }
  if (micros() - wire.last_rx_us < rx_gap_us(wire)) {
    return false;
  }
  if (wire.len >= 4 && wire.len <= MAX_FRAME_SIZE && crc16(wire.buf, wire.len) == 0) {
    std::memcpy(dst, wire.buf, wire.len);
    *out_len = wire.len;
    consume(wire, wire.len);
    return true;
  }
  if (wire.len >= 4 && wire.len <= MAX_FRAME_SIZE) {
    log_throttled(log_ms, LOG_STR("Modbus CRC failed, frame dropped"));
  } else {
    log_throttled(log_ms, LOG_STR("Incomplete Modbus frame dropped"));
  }
  wire.len = 0;
  return false;
}

static bool take_request(ForwardState::Wire &wire, uint8_t *dst, uint16_t *out_len, uint16_t *txn, uint32_t &log_ms) {
  if (wire.link != nullptr) {
    return wire.link->take_gateway(wire.uart, dst, MAX_FRAME_SIZE, out_len, txn, false);
  }
  *txn = 0;
  return take_rtu_wire(wire, dst, out_len, false, log_ms);
}

static bool take_response(ForwardState::Wire &wire, uint8_t *dst, uint16_t *out_len, uint32_t &log_ms) {
  if (wire.link != nullptr) {
    uint16_t txn = 0;
    return wire.link->take_gateway(wire.uart, dst, MAX_FRAME_SIZE, out_len, &txn, true);
  }
  return take_rtu_wire(wire, dst, out_len, true, log_ms);
}

static void drop_pending(ForwardState::Wire &wire) {
  wire.len = 0;
  if (wire.link != nullptr) {
    wire.link->clear_rx();
  }
  if (wire.uart != nullptr) {
    drain_uart(wire.uart);
  }
}

static GatewayResult write_side(ForwardState::Wire &wire, const uint8_t *rtu, uint16_t len, bool as_response) {
  if (wire.uart == nullptr || len < 4) {
    return GatewayResult::GATEWAY_RESULT_DROPPED;
  }
  if (wire.link == nullptr) {
    // The line must be silent for 3.5 characters after the last frame sent and the last byte received.
    const uint32_t start = micros();
    if (wire.last_tx_us != 0 &&
        start - wire.last_tx_us < frame_us(wire, wire.last_tx_len) + wire.timing.frame_delay_us) {
      return GatewayResult::GATEWAY_RESULT_LATER;
    }
    if (wire.last_rx_us != 0 && start - wire.last_rx_us < wire.timing.frame_delay_us) {
      return GatewayResult::GATEWAY_RESULT_LATER;
    }
    if (wire.flow != nullptr) {
      // RS-485 driver enable: on for the frame, off once it has left the UART.
      wire.flow->digital_write(true);
      wire.uart->write_array(rtu, len);
      wire.uart->flush();
      wire.flow->digital_write(false);
      wire.last_tx_len = 0;
    } else {
      wire.uart->write_array(rtu, len);
      wire.last_tx_len = len;
    }
    const uint32_t now = micros();
    wire.last_tx_us = now == 0 ? 1 : now;
    return GatewayResult::GATEWAY_RESULT_SENT;
  }
  return wire.link->send_gateway(wire.uart, rtu, len, as_response);
}

static bool send_stuck(ForwardState *st) {
  const uint32_t now = micros();
  if (st->blocked_since_us == 0) {
    st->blocked_since_us = now == 0 ? 1 : now;
    return false;
  }
  return now - st->blocked_since_us >= FORWARD_STUCK_US;
}

static bool from_rtu_client(const ForwardState *st) { return st->side[CLIENT].link == nullptr; }

// A reply carries the request's function code, or that code plus 0x80 for an exception, and the
// request's address. A TCP reply is paired by its transaction id and already carries the request's unit.
static bool reply_matches(const ForwardState *st) {
  return st->resp_len >= 5 && st->resp[0] == st->req[0] && (st->resp[1] & 0x7F) == st->req[1];
}

static bool in_quiet(ForwardState *st) {
  if (st->quiet_us == 0) {
    return false;
  }
  if (micros() - st->quiet_from_us < st->quiet_us) {
    return true;
  }
  st->quiet_us = 0;
  return false;
}

static void finish(ForwardState *st) {
  st->resp_len = 0;
  st->abandoned = false;
  st->blocked_since_us = 0;
  if (st->next_len != 0) {
    std::memcpy(st->req, st->next, st->next_len);
    st->req_len = st->next_len;
    st->next_len = 0;
    st->phase = ForwardPhase::FORWARD_PHASE_SEND;
    return;
  }
  st->req_len = 0;
  st->phase = ForwardPhase::FORWARD_PHASE_IDLE;
}

static void time_out(ForwardState *st) {
  const bool logged = log_throttled(st->log_ms, LOG_STR("No reply from the server, request dropped"));
  if (st->side[SERVER].link == nullptr) {
    st->quiet_from_us = micros();
    st->quiet_us = st->side[SERVER].max_frame_us;
    st->late_hint = logged;
  }
  // An RTU client times out on its own. A TCP client learns that only the server is silent.
  if (st->abandoned || from_rtu_client(st)) {
    finish(st);
    return;
  }
  st->resp[0] = st->req[0];
  st->resp[1] = st->req[1] | 0x80;
  st->resp[2] = static_cast<uint8_t>(ExceptionCode::GATEWAY_TARGET_DEVICE_FAILED_TO_RESPOND);
  append_rtu_crc(st->resp, 3);
  st->resp_len = 5;
  st->phase = ForwardPhase::FORWARD_PHASE_REPLY;
}

static bool side_down(const ForwardState::Wire &wire) {
  if (wire.uart == nullptr || !wire.uart->is_connected()) {
    return true;
  }
  return wire.link != nullptr && wire.link->dropping();
}

static void arm_wire(ForwardState::Wire &wire, uart::UARTComponent *uart, bool tcp, GPIOPin *flow) {
  wire.uart = uart;
  wire.flow = tcp ? nullptr : flow;
  if (uart != nullptr) {
    wire.baud = std::max<uint32_t>(1u, uart->get_baud_rate());
    wire.timing = rtu_timing(uart);
    wire.max_frame_us = frame_us(wire, MAX_FRAME_SIZE) + wire.timing.frame_delay_us;
  }
  if (tcp) {
    wire.link = new MbapLink(MbapLink::GATEWAY);
  }
}

void ModbusForwardHub::setup() {
  Modbus::setup();
  this->state_ = new ForwardState();
  if (this->peer_flow_control_pin_ != nullptr) {
    this->peer_flow_control_pin_->setup();
  }
  this->state_->wait_us = this->send_wait_time_us_;
  this->state_->turnaround_us = this->turnaround_us_;
  arm_wire(this->state_->side[CLIENT], this->parent_, this->local_tcp_, this->flow_control_pin_);
  arm_wire(this->state_->side[SERVER], this->peer_, this->peer_tcp_, this->peer_flow_control_pin_);
}

void ModbusForwardHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus forward:\n"
                "  Client (uart_id): %s\n"
                "  Server (peer_id): %s\n"
                "  Send Wait Time: %" PRIu32 " ms\n"
                "  Turnaround Time: %" PRIu32 " ms",
                this->local_tcp_ ? LOG_STR_LITERAL("Modbus TCP") : LOG_STR_LITERAL("Modbus RTU"),
                this->peer_tcp_ ? LOG_STR_LITERAL("Modbus TCP") : LOG_STR_LITERAL("Modbus RTU"),
                this->send_wait_time_us_ / 1000, this->turnaround_us_ / 1000);
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  LOG_PIN("  Peer Flow Control Pin: ", this->peer_flow_control_pin_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  // The timing an RTU side derives from its UART settings.
  for (int i = 0; i < 2 && this->state_ != nullptr; i++) {
    const ForwardState::Wire &wire = this->state_->side[i];
    if (wire.link == nullptr) {
      ESP_LOGVV(TAG, "  %s side: frame gap %" PRIu32 " us, max frame %" PRIu32 " us",
                i == CLIENT ? LOG_STR_LITERAL("Client") : LOG_STR_LITERAL("Server"), wire.timing.frame_delay_us,
                wire.max_frame_us);
    }
  }
#endif
}

void ModbusForwardHub::loop() {
  if (this->state_ == nullptr) {
    return;
  }
  ForwardState *st = this->state_;
  for (ForwardState::Wire &wire : st->side) {
    if (wire.link != nullptr) {
      wire.link->pump(wire.uart);
    } else {
      pump_rtu(wire, st->log_ms);
    }
  }

  if (st->phase != ForwardPhase::FORWARD_PHASE_IDLE && (side_down(st->side[CLIENT]) || side_down(st->side[SERVER]))) {
    log_throttled(st->log_ms, LOG_STR("Link down, dropped the Modbus frame in flight"));
    st->next_len = 0;
    finish(st);
    return;
  }

  // The server only replies. Anything it sends outside a wait is late or stray, never a request.
  if (st->phase != ForwardPhase::FORWARD_PHASE_WAIT) {
    if (st->late_hint && st->side[SERVER].len != 0 && in_quiet(st)) {
      st->late_hint = false;
      ESP_LOGW(TAG, "Reply came after %" PRIu32 " ms; send_wait_time is %" PRIu32 " ms",
               (micros() - st->sent_us) / 1000, st->wait_us / 1000);
    }
    drop_pending(st->side[SERVER]);
  }

  // An RTU client that sends again has given up on the request in flight. A TCP client's further
  // requests stay queued on its link and are served in order.
  if (st->phase != ForwardPhase::FORWARD_PHASE_IDLE && from_rtu_client(st)) {
    uint16_t len = 0;
    uint16_t txn = 0;
    if (take_request(st->side[CLIENT], st->next, &len, &txn, st->log_ms)) {
      if (st->phase == ForwardPhase::FORWARD_PHASE_SEND) {
        // Nothing has reached the server yet, so the newer request goes instead.
        std::memcpy(st->req, st->next, len);
        st->req_len = len;
        st->blocked_since_us = 0;
      } else {
        log_throttled(st->log_ms, LOG_STR("New request from the client, the open reply is dropped"));
        st->next_len = len;
        st->abandoned = true;
      }
    }
  }

  if (st->phase == ForwardPhase::FORWARD_PHASE_IDLE) {
    uint16_t txn = 0;
    if (!take_request(st->side[CLIENT], st->req, &st->req_len, &txn, st->log_ms)) {
      return;
    }
    st->phase = ForwardPhase::FORWARD_PHASE_SEND;
    st->blocked_since_us = 0;
  }

  if (st->phase == ForwardPhase::FORWARD_PHASE_SEND) {
    // Only a write can be broadcast. A read to address 0 would get no reply from anyone.
    if (st->req[0] == BROADCAST_ADDRESS && !helpers::is_function_code_broadcastable(st->req[1])) {
      log_throttled(st->log_ms, LOG_STR("Read to address 0 dropped"));
      finish(st);
      return;
    }
    if (in_quiet(st)) {
      return;
    }
    const GatewayResult wrote = write_side(st->side[SERVER], st->req, st->req_len, false);
    if (wrote == GatewayResult::GATEWAY_RESULT_LATER) {
      if (send_stuck(st)) {
        log_throttled(st->log_ms, LOG_STR("Send stuck, dropped the Modbus frame"));
        finish(st);
      }
      return;
    }
    // Address 0 is a broadcast. It is forwarded and not answered.
    if (wrote == GatewayResult::GATEWAY_RESULT_DROPPED || st->req[0] == BROADCAST_ADDRESS) {
      if (wrote == GatewayResult::GATEWAY_RESULT_SENT && st->side[SERVER].link == nullptr) {
        // The servers act on a broadcast without replying. The next request waits the turnaround delay.
        st->quiet_from_us = micros();
        st->quiet_us = frame_us(st->side[SERVER], st->req_len) + st->turnaround_us;
        st->late_hint = false;
      }
      finish(st);
      return;
    }
    st->blocked_since_us = 0;
    st->sent_us = micros();
    // On RTU the wait starts once the request has left the wire.
    st->reply_due_us = st->wait_us + (st->side[SERVER].link == nullptr ? frame_us(st->side[SERVER], st->req_len) : 0);
    st->phase = ForwardPhase::FORWARD_PHASE_WAIT;
  }

  if (st->phase == ForwardPhase::FORWARD_PHASE_WAIT) {
    if (take_response(st->side[SERVER], st->resp, &st->resp_len, st->log_ms)) {
      if (!reply_matches(st)) {
        // The server's timeout keeps running, as for a serial client.
        log_throttled(st->log_ms, LOG_STR("Reply does not match the request, dropped"));
        st->resp_len = 0;
      } else if (st->abandoned) {
        finish(st);
        return;
      } else {
        st->phase = ForwardPhase::FORWARD_PHASE_REPLY;
      }
    }
    if (st->phase == ForwardPhase::FORWARD_PHASE_WAIT) {
      // send_wait_time ends when the reply starts. A reply still arriving on an RTU server gets
      // at most one longest frame time more, so noise cannot hold the wait open.
      const uint32_t waited = micros() - st->sent_us;
      if (waited < st->reply_due_us ||
          (st->side[SERVER].len != 0 && waited < st->reply_due_us + st->side[SERVER].max_frame_us)) {
        return;
      }
      time_out(st);
      if (st->phase != ForwardPhase::FORWARD_PHASE_REPLY) {
        return;
      }
    }
  }

  if (st->phase == ForwardPhase::FORWARD_PHASE_REPLY) {
    if (st->abandoned) {
      finish(st);
      return;
    }
    const GatewayResult wrote = write_side(st->side[CLIENT], st->resp, st->resp_len, true);
    if (wrote == GatewayResult::GATEWAY_RESULT_LATER) {
      if (send_stuck(st)) {
        log_throttled(st->log_ms, LOG_STR("Send stuck, dropped the Modbus frame"));
        finish(st);
      }
      return;
    }
    finish(st);
  }
}

}  // namespace esphome::modbus

#endif  // USE_MODBUS_TCP
