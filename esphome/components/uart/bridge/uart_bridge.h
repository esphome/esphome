#pragma once

#include "esphome/components/uart/uart_component.h"
#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstddef>
#include <cstdint>

namespace esphome::uart {

/// One direction of a bridge, a block at a time.
/// Source: a virtual one pushes each block to on_block(). Any other is read in poll() until it has been quiet for a
/// frame gap, or a block is full. On a hardware line the gap is 3.5 characters and at least 1750 us, as in the modbus
/// hub; other UARTs (USB, TCP, BLE) hand over bytes in chunks, so they get the hub's 50 ms.
/// Destination: a virtual one takes each block in one write_array(). On a hardware line, a block that starts a frame
/// waits until the line has been quiet for a frame gap; one that goes on after a block cut at 256 bytes does not. A
/// UART that reports its room gets what fits now and the rest on the next passes; one that cannot report it gets the
/// whole block at once. A block for a destination that is not connected, or that takes nothing for a second, is
/// dropped.
class UARTBridgePipe final : public UARTSink {
 public:
  /// The largest Modbus RTU frame.
  static constexpr size_t BLOCK_SIZE = 256;

  UARTBridgePipe(UARTComponent *from, UARTComponent *to) : from_(from), to_(to) {}

  void set_from_virtual(VirtualUARTComponent *from) {
    from->set_rx_sink(this);
    this->from_pushes_ = true;
  }
  void set_to_virtual() { this->to_virtual_ = true; }
  void set_from_wire() { this->from_wire_ = true; }
  void set_to_wire() { this->to_wire_ = true; }
  /// Reads the framing of both ends.
  void setup();
  /// Main loop entry. An idle pass only asks a polled source for bytes.
  bool poll() {
    if (this->len_ == 0 && (this->from_pushes_ || this->from_->available() == 0)) {
      this->fast_.stop();
      return false;
    }
    // micros(): above 19200 baud the frame gap is 1.75 ms.
    this->send_(micros());
    // Read the clock again: a write into a UART that cannot report its room blocks for most of the block.
    return this->receive_(micros());
  }
  /// One pass at the given time. Returns true while a block is collected, waits or goes out.
  bool poll(uint32_t now_us) {
    this->send_(now_us);
    return this->receive_(now_us);
  }
  /// False when both ends are virtual: then every block goes straight through.
  bool needs_loop() const { return !this->from_pushes_ || !this->to_virtual_; }
  bool from_pushes() const { return this->from_pushes_; }
  /// The quiet time that ends a block from a polled source.
  uint32_t frame_gap_us() const { return this->gap_us_; }

  void on_block(const uint8_t *data, size_t len) override;

 protected:
  void send_(uint32_t now_us);
  bool receive_(uint32_t now_us);
  void collect_(uint32_t now_us);
  void clear_();
  void drop_(size_t len, const LogString *why);

  UARTComponent *from_;
  UARTComponent *to_;
  uint32_t last_rx_us_{0};
  uint32_t gap_us_{0};
  // The wait after a full driver batch.
  uint32_t batch_us_{0};
  // Destination framing and gap; 0 unless it is a hardware line.
  uint32_t to_baud_{0};
  uint32_t to_gap_us_{0};
  // The destination's line is busy for tx_busy_us_ from tx_start_us_, with what was written at its wire time.
  uint32_t tx_start_us_{0};
  uint32_t tx_busy_us_{0};
  uint32_t progress_us_{0};
  uint32_t drop_log_ms_{0};
  // buf_[0, len_): the block that goes out (block_len_ bytes, sent_ of them written), then at most one pushed block
  // waiting behind it. While a polled source is read, block_len_ is 0.
  uint16_t len_{0};
  uint16_t block_len_{0};
  uint16_t sent_{0};
  uint16_t batch_bytes_{0};
  HighFrequencyLoopRequester fast_;
  uint8_t to_bits_{0};
  bool from_pushes_{false};
  bool to_virtual_{false};
  bool from_wire_{false};
  bool to_wire_{false};
  // The last read was a full driver batch, so more of the frame may still be in the UART's FIFO.
  bool batched_{false};
  // The destination has taken nothing for FAST_WAIT_US.
  bool stalled_{false};
  // The block that goes out starts a frame (every pushed block does); the last polled block was cut at BLOCK_SIZE.
  bool new_frame_{true};
  bool cut_{false};
  uint8_t buf_[BLOCK_SIZE]{};
};

/// Copies bytes both ways between two UARTs, a block at a time.
class UARTBridge final : public Component {
 public:
  UARTBridge(UARTComponent *a, UARTComponent *b) : a_to_b_(a, b), b_to_a_(b, a) {}

  void set_virtual_a(VirtualUARTComponent *a) {
    this->a_to_b_.set_from_virtual(a);
    this->b_to_a_.set_to_virtual();
  }
  void set_virtual_b(VirtualUARTComponent *b) {
    this->b_to_a_.set_from_virtual(b);
    this->a_to_b_.set_to_virtual();
  }
  void set_wire_a() {
    this->a_to_b_.set_from_wire();
    this->b_to_a_.set_to_wire();
  }
  void set_wire_b() {
    this->b_to_a_.set_from_wire();
    this->a_to_b_.set_to_wire();
  }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  UARTBridgePipe a_to_b_;
  UARTBridgePipe b_to_a_;
};

}  // namespace esphome::uart
