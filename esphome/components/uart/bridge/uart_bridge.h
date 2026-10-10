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

/// One direction of a bridge, a block at a time. A virtual source pushes blocks to on_block(); any other is read in
/// poll() until it has been quiet for a frame gap or a block is full.
class UARTBridgePipe final : public UARTSink {
 public:
  static constexpr size_t BLOCK_SIZE = 256;

  UARTBridgePipe(UARTComponent *from, UARTComponent *to) : from_(from), to_(to) {}

  void set_from_virtual(VirtualUARTComponent *from) {
    from->set_rx_sink(this);
    this->from_pushes_ = true;
  }
  void set_to_virtual() { this->to_virtual_ = true; }
  void set_from_wire() { this->from_wire_ = true; }
  void set_to_wire() { this->to_wire_ = true; }
  void setup();
  bool poll() {
    if (this->len_ == 0 && (this->from_pushes_ || this->from_->available() == 0)) {
      this->fast_.stop();
      return false;
    }
    this->send_(micros());
    // Read the clock again: a blocking write can take most of the block's wire time.
    return this->receive_(micros());
  }
  /// Returns true while a block is collected, waits or goes out.
  bool poll(uint32_t now_us) {
    this->send_(now_us);
    return this->receive_(now_us);
  }
  bool needs_loop() const { return !this->from_pushes_ || !this->to_virtual_; }
  bool from_pushes() const { return this->from_pushes_; }
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
  uint32_t batch_us_{0};
  // 0 unless the destination is a line.
  uint32_t to_baud_{0};
  uint32_t to_gap_us_{0};
  // The destination line is busy for tx_busy_us_ from tx_start_us_.
  uint32_t tx_start_us_{0};
  uint32_t tx_busy_us_{0};
  uint32_t progress_us_{0};
  uint32_t drop_log_ms_{0};
  // buf_[0, len_): the block going out (block_len_, sent_ written), then at most one pushed block behind it.
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
  // The last read was a full driver batch; more may still be in the FIFO.
  bool batched_{false};
  bool stalled_{false};
  // The block going out starts a frame; the last polled block was cut at BLOCK_SIZE.
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
