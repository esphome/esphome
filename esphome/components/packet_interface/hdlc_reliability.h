#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_HDLC
#include <array>

#include "packet_interface.h"

namespace esphome::packet_interface {

/**
 * A reliability layer that delivers packets in order, without loss or duplicates, between two peers.
 *
 * It follows HDLC asynchronous balanced mode, with sequence numbers modulo 8 and go-back-N retransmission:
 * - I-frames carry a payload, the sequence number N(S) and an acknowledgement N(R) of the peer's frames.
 * - RR frames acknowledge without a payload; REJ frames ask the peer to resend from N(R).
 * - SABM resets both peers' sequence numbers, and UA answers it.
 *
 * Each frame is the HDLC control byte, the payload, then a CRC-16 of both in little endian order.
 * Unacknowledged frames are resent after the timeout; after max_retries the link is reset.
 */
class HdlcReliability : public PacketInterface {
 public:
  /// Most frames that can be unacknowledged at once, set by the 3 bit sequence numbers.
  static constexpr uint8_t MAX_WINDOW = 7;
  /// Control byte plus CRC-16.
  static constexpr size_t FRAME_OVERHEAD = 3;

  static constexpr size_t packet_size(size_t lower_size) { return remaining_packet_size(lower_size, FRAME_OVERHEAD); }
  static constexpr size_t send_queue_size(size_t lower_size, uint8_t queue_length) {
    return size_t{queue_length} * packet_size(lower_size);
  }

  /// @param tx_slots send_queue_size() bytes for the send queue; setup() fails if it is nullptr
  /// @param max_queue_length the queue_length the send queue was sized for
  HdlcReliability(PacketInterface *lower, uint8_t *tx_slots, uint8_t max_queue_length);

  void setup() override;
  void loop() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;
  /// No room without a send queue, or when the layer below leaves none for a packet.
  size_t get_send_capacity() const override {
    return this->has_queue_() ? this->max_queue_length_ - this->outstanding_() : 0;
  }

  void set_ack_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }
  void set_ack_delay(uint32_t ack_delay_ms) { this->ack_delay_ms_ = ack_delay_ms; }
  void set_max_retries(uint8_t retries) { this->max_retries_ = retries; }

  bool is_connected() const { return this->state_ == LinkState::LINK_STATE_CONNECTED; }

 protected:
  enum class LinkState : uint8_t {
    LINK_STATE_RESET_PENDING,  // SABM to be sent from loop()
    LINK_STATE_RESETTING,      // SABM sent, waiting for UA
    LINK_STATE_CONNECTED,
  };

  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  void receive_info_(uint8_t control, const uint8_t *payload, size_t length, const PacketMetaData &meta_data);
  bool send_frame_(uint8_t control, const uint8_t *payload, size_t length);
  bool send_supervisory_(uint8_t type);
  bool send_pending_();
  void resend_outstanding_();
  void retry_send_();
  /// An RR is owed: at once, or after the ack delay unless an I-frame carries it first.
  void owe_ack_(bool now);
  void ack_sent_();
  void process_ack_(uint8_t nr);
  void on_timeout_();
  void start_reset_();
  void link_up_();
  void start_timer_();
  void request_loop_();
  bool has_queue_() const { return this->tx_slots_ != nullptr && this->max_packet_size_ != 0; }
  uint8_t outstanding_() const { return (this->vs_ - this->va_) & 7; }
  uint8_t slot_index_(uint8_t seq) const { return (this->head_ + ((seq - this->va_) & 7)) % this->max_queue_length_; }
  uint8_t *slot_(uint8_t seq) { return this->tx_slots_ + this->slot_index_(seq) * this->max_packet_size_; }

  PacketInterface *lower_;
  // max_queue_length_ slots of max_packet_size_ bytes, a ring starting at head_ which holds frame va_
  uint8_t *tx_slots_;                              // null if the PSRAM allocation failed
  std::array<uint16_t, MAX_WINDOW> tx_lengths_{};  // indexed like the slots
  uint32_t timeout_ms_{200};
  uint32_t ack_delay_ms_{0};
  const uint8_t max_queue_length_;
  uint8_t max_retries_{5};
  uint8_t retries_{0};
  uint8_t head_{0};
  uint8_t vs_{0};  // sequence number of the next new frame
  uint8_t vn_{0};  // next frame to send; frames from va_ up to it have been sent
  uint8_t va_{0};  // oldest unacknowledged frame
  uint8_t vr_{0};  // next frame expected from the peer
  LinkState state_{LinkState::LINK_STATE_RESET_PENDING};
  bool ack_pending_ : 1 {false};  // an RR is owed
  bool ack_due_ : 1 {false};      // and should be sent now
  bool rej_sent_ : 1 {false};
  bool peer_frame_seen_ : 1 {false};  // a numbered frame has arrived since the link came up
  bool oversize_warned_ : 1 {false};
  bool reset_wait_logged_ : 1 {false};  // the queue filled while waiting for this reset to be answered
  bool destination_warned_ : 1 {false};
};

}  // namespace esphome::packet_interface
#endif
