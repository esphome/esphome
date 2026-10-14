#include "hdmi_cec.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hdmi_cec {

ESPHOME_LOG_TAG(TAG, "hdmi_cec");

// Bit timing from the HDMI 1.4 CEC specification, in microseconds
static constexpr uint32_t START_BIT_MIN_US = 3500;  // a low period at least this long is a start bit
static constexpr uint32_t START_BIT_NOM_US = 3700;
static constexpr uint32_t START_BIT_HIGH_US = 800;
static constexpr uint32_t HIGH_BIT_MIN_US = 400;  // shortest low period of a logical 1
static constexpr uint32_t HIGH_BIT_US = 600;      // nominal low period of a logical 1
static constexpr uint32_t LOW_BIT_US = 1500;      // nominal low period of a logical 0
static constexpr uint32_t SAFE_SAMPLE_US = 1050;  // the line is sampled here; a longer low period is a logical 0
static constexpr uint32_t TOTAL_BIT_US = 2400;
static constexpr uint32_t SIGNAL_FREE_TIME_AFTER_RECEIVE = TOTAL_BIT_US * 5;  // also for a new initiator
static constexpr uint32_t SIGNAL_FREE_TIME_AFTER_XMIT_FAIL = TOTAL_BIT_US * 3;
static constexpr uint32_t SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS = TOTAL_BIT_US * 7;
static constexpr uint32_t ABORTED_FRAME_US = TOTAL_BIT_US * 15;  // no edge for this long: our frame was cut short
static constexpr uint8_t MAX_ATTEMPTS = 8;  // CEC allows 5, but a busy bus counts as an attempt here too

Frame::Frame(uint8_t initiator_addr, uint8_t destination_addr, std::span<const uint8_t> payload) {
  this->data_[0] = ((initiator_addr & 0xf) << 4) | (destination_addr & 0xf);
  this->size_ = 1 + payload.size();
  std::copy(payload.begin(), payload.end(), this->data_.begin() + 1);
}

void HDMICEC::setup() {
  // The line is open drain with a weak pull-up: driven low to send, released to read
  this->pin_->setup();
  this->isr_pin_ = this->pin_->to_isr();
  this->set_pin_input_high_();
  this->required_idle_period_ = SIGNAL_FREE_TIME_AFTER_RECEIVE;
  this->pin_->attach_interrupt(HDMICEC::gpio_isr_s, this, gpio::INTERRUPT_ANY_EDGE);
  // loop() only has work once the interrupt queues a frame or send() queues one
  this->disable_loop();
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HDMI-CEC:\n"
                "  Address: 0x%X\n"
                "  Monitor mode: %s\n"
                "  Promiscuous mode: %s",
                this->address_, YESNO(this->monitor_mode_), YESNO(this->promiscuous_mode_));
  LOG_PIN("  Pin: ", this->pin_);
}

void HDMICEC::loop() {
  const uint8_t dropped = atomic_get(this->dropped_frames_);
  if (dropped != this->reported_dropped_frames_) {
    ESP_LOGW(TAG, "%u received frame%s dropped (queue full or oversized)",
             static_cast<uint8_t>(dropped - this->reported_dropped_frames_),
             dropped - this->reported_dropped_frames_ == 1 ? LOG_STR_LITERAL("") : LOG_STR_LITERAL("s"));
    this->reported_dropped_frames_ = dropped;
  }
  // One inbound frame per pass keeps the time spent in the automations bounded
  if (const Frame *frame = this->frames_queue_.front()) {
#ifdef ESPHOME_LOG_HAS_DEBUG
    char line[format_hex_pretty_size(Frame::MAX_LENGTH)];
    ESP_LOGD(TAG, "Received: %s", format_hex_pretty_to(line, frame->bytes().data(), frame->size()));
#endif
#ifdef HDMI_CEC_MESSAGE_CALLBACK_COUNT
    this->message_callbacks_.call(frame->initiator_addr(), frame->destination_addr(), frame->payload());
#endif
    this->frames_queue_.pop_front();
  }
  if (!this->is_transmit_idle_()) {
    this->transmit_message_();
  }
  if (this->frames_queue_.is_empty() && this->is_transmit_idle_()) {
    this->disable_loop();
  }
}

bool HDMICEC::send(uint8_t source, uint8_t destination, std::span<const uint8_t> payload) {
  if (this->monitor_mode_) {
    ESP_LOGW(TAG, "Monitor mode, not sending 0x%X->0x%X", source, destination);
    return false;
  }
  if (source > 0xf || destination > 0xf) {
    ESP_LOGE(TAG, "Send abort: logical address 0x%X->0x%X out of range", source, destination);
    return false;
  }
  if (payload.size() > Frame::MAX_PAYLOAD_LENGTH) {
    ESP_LOGE(TAG, "Send abort 0x%X->0x%X: payload of %zu bytes exceeds %zu", source, destination, payload.size(),
             Frame::MAX_PAYLOAD_LENGTH);
    return false;
  }
  Frame *frame = this->send_queue_.back();
  if (frame == nullptr) {
    ESP_LOGW(TAG, "Send queue full, frame 0x%X->0x%X dropped", source, destination);
    return false;
  }
  *frame = Frame(source, destination, payload);
  this->send_queue_.push_back();
  this->enable_loop();
  return true;
}

void HDMICEC::transmit_message_() {
  const uint32_t now = micros();
  const uint32_t since_last_edge = now - atomic_get(this->last_edge_us_);
  if (this->transmitting_) {
    // The interrupt sees our own frame; wait for its end of message bit, or give up when the
    // bus went quiet without one
    if (!atomic_get(this->eom_received_) && since_last_edge < ABORTED_FRAME_US) {
      return;
    }
    this->transmitting_ = false;
    atomic_set(this->eom_received_, false);  // the end of our own frame is not a foreign one
    const Frame *frame = this->send_queue_.front();
    // A broadcast is acknowledged by nobody; a directed frame on every byte
    const bool bytes_ok = this->n_bytes_received_ == frame->size();
    const bool acks_ok = this->n_acks_received_ == (frame->is_broadcast() ? 0 : frame->size());
    if (bytes_ok && acks_ok) {
      ESP_LOGD(TAG, "Send frame success in %u attempt%s", this->transmit_attempts_,
               this->transmit_attempts_ > 1 ? LOG_STR_LITERAL("s") : LOG_STR_LITERAL(""));
    } else if (!bytes_ok) {
      ESP_LOGD(TAG, "Send frame incorrect on attempt %u, saw %u bytes but expected %zu", this->transmit_attempts_,
               this->n_bytes_received_, frame->size());
    } else {
      ESP_LOGD(TAG, "Send frame not acknowledged on attempt %u", this->transmit_attempts_);
    }
    this->finish_attempt_(bytes_ok && acks_ok);
  } else if (atomic_get(this->eom_received_)) {
    // Another device finished a frame; the standard asks a new initiator to wait after it
    this->required_idle_period_ = SIGNAL_FREE_TIME_AFTER_RECEIVE;
    atomic_set(this->eom_received_, false);
  }

  const Frame *frame = this->send_queue_.front();
  // An edge within the signal free time means the bus is in use or not yet free for us; a start
  // bit refused by a held line drives no edge, so that attempt is timed on its own
  if (frame == nullptr || since_last_edge < this->required_idle_period_ ||
      now - this->last_attempt_us_ < this->required_idle_period_) {
    return;
  }

  this->last_attempt_us_ = now;
  this->transmit_attempts_++;
#ifdef ESPHOME_LOG_HAS_DEBUG
  if (this->transmit_attempts_ == 1) {
    char line[format_hex_pretty_size(Frame::MAX_LENGTH)];
    ESP_LOGD(TAG, "Sending: %s", format_hex_pretty_to(line, frame->bytes().data(), frame->size()));
  }
#endif
  // The interrupt counts the bytes and acknowledge bits of our own frame
  this->n_bytes_received_ = 0;
  this->n_acks_received_ = 0;
  atomic_set(this->eom_received_, false);
  this->transmitting_ = true;
  if (!this->transmit_frame_(frame)) {
    // The line was held, or another initiator sent at the same time
    this->transmitting_ = false;
    this->finish_attempt_(false);
  }
  // Otherwise the acknowledge result arrives through the interrupt
}

void HDMICEC::finish_attempt_(bool sent_ok) {
  this->required_idle_period_ = sent_ok ? SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS : SIGNAL_FREE_TIME_AFTER_XMIT_FAIL;
  if (!sent_ok) {
    if (this->transmit_attempts_ < MAX_ATTEMPTS) {
      return;  // the signal free time has to pass before the retry
    }
    ESP_LOGW(TAG, "Frame not sent after %u attempts, dropped", this->transmit_attempts_);
  }
  this->transmit_attempts_ = 0;
  this->send_queue_.pop_front();
}

bool HDMICEC::transmit_frame_(const Frame *frame) {
  if (!this->send_start_bit_()) {
    return false;
  }
  const auto bytes = frame->bytes();
  for (size_t byte_inx = 0; byte_inx < bytes.size(); byte_inx++) {
    const uint8_t current_byte = bytes[byte_inx];
    // Most significant bit first. The four initiator address bits watch for a collision with
    // another initiator that started at the same moment; a lower address wins by holding the line
    for (int8_t i = 7; i >= 0; i--) {
      const bool bit_value = (current_byte >> i) & 0x1;
      if (!this->send_bit_(bit_value) && byte_inx == 0 && i >= 4) {
        ESP_LOGD(TAG, "Bus collision on sending initiator address 0x%X on attempt %u", frame->initiator_addr(),
                 this->transmit_attempts_);
        return false;
      }
    }
    // End of message bit, then the acknowledge bit which the receiver may pull low
    this->send_bit_(byte_inx == bytes.size() - 1);
    this->send_bit_(true);
  }
  return true;
}

bool HDMICEC::send_start_bit_() {
  if (!this->isr_pin_.digital_read()) {
    ESP_LOGD(TAG, "Bus occupied before start bit on attempt %u", this->transmit_attempts_);
    return false;
  }
  this->set_pin_output_low_();
  delay_microseconds_safe(START_BIT_NOM_US);
  // Release the line and check that nobody else keeps it low
  this->set_pin_input_high_();
  delay_microseconds_safe(START_BIT_HIGH_US / 2);
  bool value = this->isr_pin_.digital_read();
  delay_microseconds_safe(START_BIT_HIGH_US / 2);
  value &= this->isr_pin_.digital_read();
  if (!value) {
    ESP_LOGD(TAG, "Bus collision on sending start bit on attempt %u", this->transmit_attempts_);
  }
  return value;
}

bool HDMICEC::send_bit_(bool bit_value) {
  const uint32_t start_us = micros();
  this->set_pin_output_low_();
  delay_microseconds_safe(bit_value ? HIGH_BIT_US : LOW_BIT_US);
  this->set_pin_input_high_();
  bool clear = true;
  if (bit_value) {
    // Sample in the middle of the safe sample period: a line still low means another initiator
    // is sending a logical 0 at the same time
    uint32_t elapsed = micros() - start_us;
    if (elapsed < SAFE_SAMPLE_US) {
      delay_microseconds_safe(SAFE_SAMPLE_US - elapsed);
    }
    clear = this->isr_pin_.digital_read();
  }
  const uint32_t elapsed = micros() - start_us;
  if (elapsed < TOTAL_BIT_US) {
    delay_microseconds_safe(TOTAL_BIT_US - elapsed);
  }
  return clear;
}

void IRAM_ATTR HDMICEC::send_ack_() {
  // Holding the line keeps other interrupts waiting for 1.5 ms, but the acknowledge has to start
  // right at this falling edge
  this->set_pin_output_low_();
  delay_microseconds_safe(LOW_BIT_US);
  this->set_pin_input_high_();
}

void IRAM_ATTR HDMICEC::gpio_isr_s(HDMICEC *self) { self->gpio_isr_(); }

void IRAM_ATTR HDMICEC::gpio_isr_() {
  const uint32_t now = micros();
  const bool level = this->isr_pin_.digital_read();
  if (level == this->prev_pin_level_) {
    return;  // a mode change of the pin, not a bus edge
  }
  this->prev_pin_level_ = level;
  atomic_set(this->last_edge_us_, now);

  if (!level) {
    // Falling edge: the start of a low period
    if (now - this->last_falling_edge_us_ > START_BIT_NOM_US + TOTAL_BIT_US) {
      // The bus was quiet; whatever was gathered belongs to an aborted frame
      this->reset_receiver_();
    }
    this->last_falling_edge_us_ = now;
    if (this->recv_ack_queued_ && !this->monitor_mode_) {
      // Acknowledge by stretching this low period
      this->send_ack_();
      this->recv_ack_queued_ = false;
    }
    return;
  }

  // Rising edge: the low period tells the bit value
  const uint32_t pulse_duration = now - this->last_falling_edge_us_;
  if (pulse_duration > START_BIT_MIN_US) {
    this->reset_receiver_();
    this->receiver_state_ = ReceiverState::RECEIVER_STATE_RECEIVING_BYTE;
    // Without a free slot the frame is neither acknowledged nor delivered, so its initiator retries
    this->recv_frame_ = this->frames_queue_.back();
    if (this->recv_frame_ != nullptr) {
      this->recv_frame_->clear();
    }
    return;
  }
  if (pulse_duration < HIGH_BIT_MIN_US / 4) {
    return;  // glitch
  }
  const bool value = pulse_duration <= SAFE_SAMPLE_US;

  switch (this->receiver_state_) {
    case ReceiverState::RECEIVER_STATE_IDLE:
      break;

    case ReceiverState::RECEIVER_STATE_RECEIVING_BYTE:
      this->recv_byte_ = (this->recv_byte_ << 1) | value;
      if (++this->recv_bit_counter_ == 8) {
        if (this->recv_length_++ == 0) {
          this->recv_header_ = this->recv_byte_;
        }
        if (this->recv_frame_ != nullptr && !this->recv_frame_->push_back(this->recv_byte_)) {
          this->recv_frame_ = nullptr;  // longer than the bus allows: corrupt, do not deliver it
        }
        this->recv_bit_counter_ = 0;
        this->receiver_state_ = ReceiverState::RECEIVER_STATE_WAITING_FOR_EOM;
      }
      break;

    case ReceiverState::RECEIVER_STATE_WAITING_FOR_EOM:
      // A frame for us is acknowledged on the next bit, unless there is no room for it (so its
      // initiator retries) or it is our own: a polling message from a device claiming our
      // logical address must not be acknowledged
      if (this->recv_frame_ != nullptr && (this->recv_header_ & 0xf) == this->address_ &&
          (this->recv_header_ >> 4) != this->address_) {
        this->recv_ack_queued_ = true;
      }
      this->recv_eom_ = value;
      this->receiver_state_ = ReceiverState::RECEIVER_STATE_WAITING_FOR_ACK;
      break;

    case ReceiverState::RECEIVER_STATE_WAITING_FOR_ACK:
      if (this->transmitting_ && !atomic_get(this->eom_received_)) {
        // Our own frame: a low acknowledge bit means the addressed device took the byte
        // (for a broadcast it means some device refused it)
        this->n_bytes_received_++;
        if (!value) {
          this->n_acks_received_++;
        }
      }
      if (!this->recv_eom_) {
        this->receiver_state_ = ReceiverState::RECEIVER_STATE_RECEIVING_BYTE;
        break;
      }
      atomic_set(this->eom_received_, true);  // publishes the tally above to loop()
      // The application only gets frames that are not our own and not a polling message (header
      // only), which the acknowledge answered already, and that are addressed to us unless
      // promiscuous
      const uint8_t destination = this->recv_header_ & 0xf;
      const bool wanted = this->recv_length_ > 1 && (this->recv_header_ >> 4) != this->address_ &&
                          (this->promiscuous_mode_ || destination == 0xf || destination == this->address_);
      if (!wanted) {
        // nothing to deliver
      } else if (this->recv_frame_ == nullptr) {
        atomic_set<uint8_t>(this->dropped_frames_, atomic_get(this->dropped_frames_) + 1);
      } else {
        this->frames_queue_.push_back();
        this->enable_loop_soon_any_context();
      }
      this->reset_receiver_();
      break;
  }
}

void IRAM_ATTR HDMICEC::reset_receiver_() {
  this->receiver_state_ = ReceiverState::RECEIVER_STATE_IDLE;
  this->recv_bit_counter_ = 0;
  this->recv_length_ = 0;
  this->recv_ack_queued_ = false;
  this->recv_frame_ = nullptr;
}

}  // namespace esphome::hdmi_cec
