#include "hdlc_reliability.h"
#ifdef USE_PACKET_INTERFACE_HDLC
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.hdlc");

static constexpr uint32_t RETRANSMIT_TIMER_ID = 0;
static constexpr uint32_t ACK_TIMER_ID = 1;
static constexpr uint32_t SEND_RETRY_TIMER_ID = 2;
// A frame or acknowledgement the lower layer refused, for example while a radio was receiving, is sent again after this
static constexpr uint32_t SEND_RETRY_MS = 50;
// SABM is repeated at this interval, or the timeout if longer, until the peer answers
static constexpr uint32_t RESET_INTERVAL_MS = 1000;

// Control byte formats, with the poll/final bit (0x10) ignored
static constexpr uint8_t POLL_FINAL_BIT = 0x10;
static constexpr uint8_t SUPERVISORY_FRAME = 0x01;
static constexpr uint8_t SUPERVISORY_RR = 0x00;
static constexpr uint8_t SUPERVISORY_REJ = 0x02;
static constexpr uint8_t UNNUMBERED_SABM = 0x2F;
static constexpr uint8_t UNNUMBERED_UA = 0x63;

static uint16_t frame_crc(uint8_t control, const uint8_t *payload, size_t length) {
  uint16_t crc = crc16(&control, 1);
  return crc16(payload, length, crc);
}

HdlcReliability::HdlcReliability(PacketInterface *lower) : lower_(lower) {
  this->stack_on_(lower, CAPABILITY_RELIABILITY);
  this->max_packet_size_ = lower->get_max_packet_size() - FRAME_OVERHEAD;
}

void HdlcReliability::setup() {
  this->tx_slots_ = std::make_unique<uint8_t[]>(this->max_queue_length_ * this->max_packet_size_);
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void HdlcReliability::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HDLC Reliability on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Timeout: %" PRIu32 "ms\n"
                "  Ack Delay: %" PRIu32 "ms\n"
                "  Max Queue Length: %u\n"
                "  Max Retries: %u",
                this->name_, this->max_packet_size_, this->timeout_ms_, this->ack_delay_ms_, this->max_queue_length_,
                this->max_retries_);
}

void HdlcReliability::loop() {
  // Disabled first, as sending may deliver a reply to this layer and request the loop again
  this->disable_loop();
  if (this->state_ == LinkState::LINK_STATE_RESET_PENDING) {
    this->state_ = LinkState::LINK_STATE_RESETTING;
    this->start_timer_();
    this->send_frame_(UNNUMBERED_SABM, nullptr, 0);
  }
  if (this->state_ != LinkState::LINK_STATE_CONNECTED)
    return;
  // Queued frames go first, as each carries the acknowledgement
  if (this->send_pending_() && this->ack_due_ && !this->send_supervisory_(SUPERVISORY_RR))
    this->retry_send_();
}

void HdlcReliability::retry_send_() {
  this->set_timeout(SEND_RETRY_TIMER_ID, SEND_RETRY_MS, [this]() { this->request_loop_(); });
}

void HdlcReliability::owe_ack_(bool now) {
  if (now || this->ack_delay_ms_ == 0) {
    this->ack_due_ = true;
    this->request_loop_();
  } else if (!this->ack_pending_) {
    // Waiting lets one acknowledgement cover several frames, or a reply carry it
    this->set_timeout(ACK_TIMER_ID, this->ack_delay_ms_, [this]() {
      this->ack_due_ = true;
      this->request_loop_();
    });
  }
  this->ack_pending_ = true;
}

void HdlcReliability::ack_sent_() {
  this->ack_pending_ = false;
  this->ack_due_ = false;
  this->cancel_timeout(ACK_TIMER_ID);
}

void HdlcReliability::request_loop_() { this->enable_loop(); }

bool HdlcReliability::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  uint8_t outstanding = this->outstanding_();
  if (outstanding >= this->max_queue_length_) {
    ESP_LOGV(TAG, "%s: Send queue full", this->name_);
    return false;
  }
  uint8_t seq = this->vs_;
  data.copy_to(this->slot_(seq), size);
  this->tx_lengths_[this->slot_index_(seq)] = size;
  this->vs_ = (seq + 1) & 7;
  // While the link is being reset, the frame waits in the queue and is sent when UA arrives
  if (this->state_ == LinkState::LINK_STATE_CONNECTED) {
    this->send_pending_();
    if (outstanding == 0)
      this->start_timer_();
  }
  return true;
}

bool HdlcReliability::send_frame_(uint8_t control, const uint8_t *payload, size_t length) {
  uint16_t crc = frame_crc(control, payload, length);
  uint8_t trailer[2] = {static_cast<uint8_t>(crc), static_cast<uint8_t>(crc >> 8)};
  PacketBuffer body(payload, length);
  PacketBuffer frame(&control, 1, length != 0 ? &body : nullptr, trailer, sizeof(trailer));
  if (this->lower_->send_to_interface(frame, {}))
    return true;
  ESP_LOGV(TAG, "%s: Lower layer did not accept frame 0x%02X", this->name_, control);
  return false;
}

bool HdlcReliability::send_supervisory_(uint8_t type) {
  if (!this->send_frame_(SUPERVISORY_FRAME | (type << 2) | (this->vr_ << 5), nullptr, 0))
    return false;
  this->ack_sent_();
  return true;
}

// Sends the next queued frame not yet sent, and returns true if none are left. One frame per loop pass lets other
// components run between frames: on a radio, sending blocks, and the peer's frames are received in between. If the
// lower layer refuses the frame, it is sent again shortly.
bool HdlcReliability::send_pending_() {
  if (this->vn_ == this->vs_)
    return true;
  uint8_t control = (this->vn_ << 1) | (this->vr_ << 5);
  if (!this->send_frame_(control, this->slot_(this->vn_), this->tx_lengths_[this->slot_index_(this->vn_)])) {
    this->retry_send_();
    return false;
  }
  // Every I-frame carries an acknowledgement, so a pending RR is no longer needed
  this->ack_sent_();
  this->vn_ = (this->vn_ + 1) & 7;
  if (this->vn_ == this->vs_)
    return true;
  this->request_loop_();
  return false;
}

void HdlcReliability::resend_outstanding_() {
  this->vn_ = this->va_;
  this->send_pending_();
}

void HdlcReliability::start_timer_() {
  uint32_t interval = this->timeout_ms_;
  if (this->state_ != LinkState::LINK_STATE_CONNECTED && interval < RESET_INTERVAL_MS)
    interval = RESET_INTERVAL_MS;
  // Up to a quarter longer at random, so devices whose frames collided on a shared channel do not resend together
  interval += random_uint32() % (interval / 4 + 1);
  this->set_timeout(RETRANSMIT_TIMER_ID, interval, [this]() { this->on_timeout_(); });
}

void HdlcReliability::on_timeout_() {
  if (this->state_ != LinkState::LINK_STATE_CONNECTED) {
    this->send_frame_(UNNUMBERED_SABM, nullptr, 0);
    this->start_timer_();
    return;
  }
  if (this->outstanding_() == 0)
    return;
  if (++this->retries_ > this->max_retries_) {
    ESP_LOGW(TAG, "%s: No acknowledgement after %u retries, resetting link", this->name_, this->max_retries_);
    this->start_reset_();
    return;
  }
  ESP_LOGV(TAG, "%s: Timeout, resending %u frames", this->name_, this->outstanding_());
  this->resend_outstanding_();
  this->start_timer_();
}

// Unacknowledged frames are discarded; frames queued from now on are numbered from 0 and sent once the link is up.
// Resetting a connected link can lose data both ways, which is reported to the layers above.
void HdlcReliability::start_reset_() {
  bool was_connected = this->state_ == LinkState::LINK_STATE_CONNECTED;
  this->state_ = LinkState::LINK_STATE_RESET_PENDING;
  this->va_ = this->vs_ = this->vn_ = 0;
  this->head_ = 0;
  this->vr_ = 0;
  this->retries_ = 0;
  this->rej_sent_ = false;
  this->ack_sent_();
  this->cancel_timeout(RETRANSMIT_TIMER_ID);
  this->request_loop_();
  if (was_connected)
    this->session_callback_.call(false);
}

void HdlcReliability::link_up_() {
  ESP_LOGD(TAG, "%s: Link connected", this->name_);
  this->state_ = LinkState::LINK_STATE_CONNECTED;
  this->retries_ = 0;
  this->cancel_timeout(RETRANSMIT_TIMER_ID);
  if (this->outstanding_() != 0) {
    this->resend_outstanding_();
    this->start_timer_();
  }
}

void HdlcReliability::process_ack_(uint8_t nr) {
  uint8_t acked = (nr - this->va_) & 7;
  if (acked > this->outstanding_()) {
    ESP_LOGV(TAG, "%s: Ignoring invalid N(R) %u", this->name_, nr);
    return;
  }
  if (acked == 0)
    return;
  // The peer may have had frames that were refused when last resent
  if (acked > ((this->vn_ - this->va_) & 7))
    this->vn_ = nr;
  this->head_ = (this->head_ + acked) % this->max_queue_length_;
  this->va_ = nr;
  this->retries_ = 0;
  if (this->outstanding_() == 0) {
    this->cancel_timeout(RETRANSMIT_TIMER_ID);
  } else {
    this->start_timer_();
  }
}

void HdlcReliability::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  // Lower layers deliver packets in one piece; the payload is passed up without copying
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "%s: Packet split across buffers is not supported", this->name_);
    return;
  }
  auto frame = data.as_span();
  if (frame.size() < FRAME_OVERHEAD) {
    this->note_received_(false);
    return;
  }
  size_t length = frame.size() - FRAME_OVERHEAD;
  uint8_t control = frame[0];
  const uint8_t *payload = frame.data() + 1;
  uint16_t crc = frame[length + 1] | (frame[length + 2] << 8);
  if (frame_crc(control, payload, length) != crc) {
    ESP_LOGV(TAG, "%s: CRC error, frame dropped", this->name_);
    this->note_received_(false);
    return;
  }
  this->note_received_(true);

  uint8_t command = control & ~POLL_FINAL_BIT;
  if (command == UNNUMBERED_SABM) {
    if (this->state_ == LinkState::LINK_STATE_CONNECTED) {
      // The peer has restarted, so frames it has not acknowledged are lost
      ESP_LOGD(TAG, "%s: Link reset by peer", this->name_);
      this->start_reset_();
    }
    this->vr_ = 0;
    this->rej_sent_ = false;
    this->send_frame_(UNNUMBERED_UA, nullptr, 0);
    // Both sides now start from 0, so the link is up even if our own SABM is unanswered
    this->link_up_();
    return;
  }
  if (command == UNNUMBERED_UA) {
    if (this->state_ == LinkState::LINK_STATE_RESETTING)
      this->link_up_();
    return;
  }
  // Numbered frames are ignored until the link is reset; the peer answers our SABM first
  if (this->state_ != LinkState::LINK_STATE_CONNECTED)
    return;

  uint8_t nr = control >> 5;
  if ((control & 1) == 0) {
    this->process_ack_(nr);
    this->receive_info_(control, payload, length, meta_data);
    return;
  }
  if ((control & 3) != SUPERVISORY_FRAME)
    return;
  this->process_ack_(nr);
  if (((control >> 2) & 3) == SUPERVISORY_REJ && this->outstanding_() != 0) {
    ESP_LOGV(TAG, "%s: Peer rejected from %u", this->name_, nr);
    this->resend_outstanding_();
    this->start_timer_();
  }
}

void HdlcReliability::receive_info_(uint8_t control, const uint8_t *payload, size_t length,
                                    const PacketMetaData &meta_data) {
  uint8_t ns = (control >> 1) & 7;
  if (ns != this->vr_) {
    // A gap or a duplicate; ask once for a resend from the expected frame, then just acknowledge at once
    if (!this->rej_sent_) {
      this->rej_sent_ = this->send_supervisory_(SUPERVISORY_REJ);
      if (this->rej_sent_)
        return;
    }
    this->owe_ack_(true);
    return;
  }
  this->vr_ = (this->vr_ + 1) & 7;
  this->rej_sent_ = false;
  // The acknowledgement is sent from loop(), so a reply sent from the callback can carry it instead
  this->owe_ack_(false);
  this->on_receive_from_interface_(PacketBuffer(payload, length), meta_data);
}

}  // namespace esphome::packet_interface
#endif
