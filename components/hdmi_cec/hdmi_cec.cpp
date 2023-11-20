#include "hdmi_cec.h"

#include "esphome/core/log.h"

namespace esphome {
namespace hdmi_cec {

static const char *const TAG = "hdmi_cec";
// receiver constants
static const uint32_t START_BIT_MIN_US = 3500;
static const uint32_t HIGH_BIT_MIN_US = 400;
static const uint32_t HIGH_BIT_MAX_US = 800;
// transmitter constants
static const uint32_t TOTAL_BIT_US = 2400;
static const uint32_t HIGH_BIT_US = 600;
static const uint32_t LOW_BIT_US = 1500;
// arbitration and retransmission
static const uint32_t MIN_SIGNAL_FREE_TIME = (TOTAL_BIT_US * 7);
static const size_t MAX_ATTEMPTS = 5;

static const gpio::Flags INPUT_MODE_FLAGS = gpio::FLAG_INPUT | gpio::FLAG_PULLUP;
static const gpio::Flags OUTPUT_MODE_FLAGS = gpio::FLAG_OUTPUT | gpio::FLAG_PULLUP;

std::string bytes_to_string(std::vector<uint8_t> bytes) {
  std::string result;
  char part_buffer[3];
  for (auto it = bytes.begin(); it != bytes.end(); it++) {
    uint8_t byte_value = *it;
    sprintf(part_buffer, "%02X", byte_value);
    result += part_buffer;

    if (it != (bytes.end() - 1)) {
      result += ":";
    }
  }
  return result;
}

void HDMICEC::setup() {
  isr_pin_ = pin_->to_isr();
  recv_frame_buffer_.reserve(16); // max 16 bytes per CEC frame
  pin_->attach_interrupt(HDMICEC::gpio_intr_, this, gpio::INTERRUPT_ANY_EDGE);
  switch_to_listen_mode_();
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  LOG_PIN("  pin: ", pin_);
  ESP_LOGCONFIG(TAG, "  address: %x", address_);
  ESP_LOGCONFIG(TAG, "  promiscuous mode: %s", (promiscuous_mode_ ? "yes" : "no"));
}

void HDMICEC::loop() {
  while(!recv_queue_.empty()) {
    auto frame = recv_queue_.front();
    recv_queue_.pop();

    uint8_t header = frame[0];
    uint8_t src_addr = ((header & 0xF0) >> 4);
    uint8_t dest_addr = (header & 0x0F);

    if (!promiscuous_mode_ && (dest_addr != 0x0F) && (dest_addr != address_)) {
      // ignore frames not meant for us
      continue;
    }

    if (frame.size() == 1) {
      // don't process pings. they're already dealt with by the acknowledgement mechanism
      ESP_LOGD(TAG, "ping received: 0x%01X -> 0x%01X", src_addr, dest_addr);
      continue;
    }

    auto frame_str = bytes_to_string(frame);
    ESP_LOGD(TAG, "frame received: %s", frame_str.c_str());

    uint8_t opcode = frame[1];
    std::vector<uint8_t> data(frame.begin() + 1, frame.end());

    for (auto trigger : message_triggers_) {
      bool can_trigger = (
        (!trigger->source_.has_value()      || (trigger->source_ == src_addr)) &&
        (!trigger->destination_.has_value() || (trigger->destination_ == dest_addr)) &&
        (!trigger->opcode_.has_value()      || (trigger->opcode_ == opcode)) &&
        (!trigger->data_.has_value() ||
          (data.size() == trigger->data_->size() && std::equal(trigger->data_->begin(), trigger->data_->end(), data.begin()))
        )
      );
      if (can_trigger) {
        trigger->trigger(src_addr, dest_addr, data);
      }
    }
  }
}

bool HDMICEC::send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data_bytes) {
  bool is_broadcast = (destination == 0xF);

  // prepare the bytes to send
  uint8_t header = (((source & 0x0F) << 4) | (destination & 0x0F));
  std::vector<uint8_t> frame = { header };
  frame.insert(frame.end(), data_bytes.begin(), data_bytes.end());

  std::string bytes_to_send = bytes_to_string(frame);
  ESP_LOGD(TAG, "sending frame: %s", bytes_to_send.c_str());

  {
    LockGuard send_lock(send_mutex_);

    for (size_t i = 0; i < MAX_ATTEMPTS; i++) {
      ESP_LOGV(TAG, "HDMICEC::send(): waiting for the bus to be free...");
      while((micros() - last_falling_edge_us_) < MIN_SIGNAL_FREE_TIME) {
        delay_microseconds_safe(TOTAL_BIT_US); // wait for one bit period
      }
      ESP_LOGV(TAG, "HDMICEC::send(): bus available, sending frame...");

      bool success = send_frame_(frame, is_broadcast);
      if (success) {
        ESP_LOGD(TAG, "HDMICEC::send(): frame sent and acknowledged");
        return true;
      } else {
        if (is_broadcast) {
          ESP_LOGW(TAG, "HDMICEC::send(): negative ack received. retrying...");
        } else {
          ESP_LOGW(TAG, "HDMICEC::send(): no ack received. retrying...");
        }
      }

      delay_microseconds_safe(TOTAL_BIT_US);
    }
  }

  ESP_LOGE(TAG, "HDMICEC::send(): send failed after five attempts");
  return false;
}

bool IRAM_ATTR HDMICEC::send_frame_(const std::vector<uint8_t> &frame, bool is_broadcast) {
  InterruptLock interrupt_lock;

  switch_to_send_mode_();

  send_start_bit_();
  
  // for each byte of the frame:
  bool success = true;
  for (auto it = frame.begin(); it != frame.end(); ++it) {
    uint8_t current_byte = *it;

    // 1. send the current byte
    for (int8_t i = 7; i >= 0; i--) {
      bool bit_value = ((current_byte >> i) & 0b1);
      send_bit_(bit_value);
    }

    // 2. send EOM bit (logic 1 if this is the last byte of the frame)
    bool is_eom = (it == (frame.end() - 1));
    send_bit_(is_eom);

    // 3. send ack bit
    bool ack_success = send_and_read_ack_(is_broadcast);
    if (!ack_success) {
      // return early if something went wrong
      success = false;
      break;
    }
  }

  switch_to_listen_mode_();

  return success;
}

void IRAM_ATTR HDMICEC::send_start_bit_() {
  // 1. pull low for 3700 us
  pin_->digital_write(false);
  delayMicroseconds(3700);

  // 2. pull high for 800 us
  pin_->digital_write(true);
  delayMicroseconds(800);

  // total duration of start bit: 4500 us
}

void IRAM_ATTR HDMICEC::send_bit_(bool bit_value) {
  // total bit duration:
  // logic 1: pull low for 600 us, then pull high for 1800 us
  // logic 0: pull low for 1500 us, then pull high for 900 us

  const uint32_t low_duration_us = (bit_value ? HIGH_BIT_US : LOW_BIT_US);
  const uint32_t high_duration_us = (TOTAL_BIT_US - low_duration_us);

  pin_->digital_write(false);
  delayMicroseconds(low_duration_us);
  pin_->digital_write(true);
  delayMicroseconds(high_duration_us);
}

bool IRAM_ATTR HDMICEC::send_and_read_ack_(bool is_broadcast) {
  uint32_t start_us = micros();

  // send a Logical 1
  pin_->digital_write(false);
  delayMicroseconds(HIGH_BIT_US);
  pin_->digital_write(true);

  // switch to input mode...
  pin_->pin_mode(INPUT_MODE_FLAGS);

  // ...then wait up to the middle of the "Safe sample period" (CEC spec -> Signaling and Bit Timing -> Figure 5)
  static const uint32_t SAFE_SAMPLE_US = 1050;
  while((micros() - start_us) < SAFE_SAMPLE_US);
  bool value = pin_->digital_read();

  pin_->pin_mode(OUTPUT_MODE_FLAGS);
  pin_->digital_write(true);

  // sleep for the rest of the bit period
  while((micros() - start_us) < TOTAL_BIT_US);

  // broadcast messages: line pulled low by any follower => something went wrong. no need to flip the value.
  if (is_broadcast) {
    return value;
  }

  // normal messages: line pulled low by the target follower => message ACKed successfully. we need to flip the value to match that logic.
  return (!value);
}

void IRAM_ATTR HDMICEC::switch_to_listen_mode_() {
  pin_->pin_mode(INPUT_MODE_FLAGS);
}

void IRAM_ATTR HDMICEC::switch_to_send_mode_() {
  pin_->pin_mode(OUTPUT_MODE_FLAGS);
  pin_->digital_write(true);
}

void IRAM_ATTR HDMICEC::gpio_intr_(HDMICEC *self) {
  const uint32_t now = micros();
  const bool level = self->isr_pin_.digital_read();

  // on falling edge, store current time as the start of the low pulse
  if (level == false) {
    self->last_falling_edge_us_ = now;

    if (self->recv_ack_queued_) {
      self->recv_ack_queued_ = false;
      {
        InterruptLock interrupt_lock;
        self->isr_pin_.pin_mode(OUTPUT_MODE_FLAGS);
        self->isr_pin_.digital_write(false);
        delayMicroseconds(LOW_BIT_US);
        self->isr_pin_.digital_write(true);
        self->isr_pin_.pin_mode(INPUT_MODE_FLAGS);
      }
    }

    return;
  }
  // otherwise, it's a rising edge, so it's time to process the pulse length

  auto pulse_duration = (micros() - self->last_falling_edge_us_);

  if (pulse_duration > START_BIT_MIN_US) {
    // start bit detected. reset everything and start receiving
    self->receiver_state_ = ReceiverState::ReceivingByte;
    reset_state_variables_(self);
    self->recv_ack_queued_ = false;
    return;
  }

  bool value = (pulse_duration >= HIGH_BIT_MIN_US && pulse_duration <= HIGH_BIT_MAX_US);
  
  switch (self->receiver_state_) {
    case ReceiverState::ReceivingByte: {
      // write bit to the current byte
      self->recv_byte_buffer_ = (self->recv_byte_buffer_ << 1) | (value & 0b1);

      self->recv_bit_counter_++;
      if (self->recv_bit_counter_ >= 8) { 
        // if we reached eight bits, push the current byte to the frame buffer
        self->recv_frame_buffer_.push_back(self->recv_byte_buffer_);

        self->recv_bit_counter_ = 0;
        self->recv_byte_buffer_ = 0;

        self->receiver_state_ = ReceiverState::WaitingForEOM;
      } else {
        self->receiver_state_ = ReceiverState::ReceivingByte;
      }
      break;
    }

    case ReceiverState::WaitingForEOM: {
      // check if we need to acknowledge this byte on the next bit
      uint8_t destination_address = (self->recv_frame_buffer_[0] & 0x0F);
      if (destination_address != 0xF && destination_address == self->address_) {
        self->recv_ack_queued_ = true;
      }

      bool isEOM = (value == 1);
      if (isEOM) {
        // pass frame to app
        self->recv_queue_.push(self->recv_frame_buffer_);
        reset_state_variables_(self);
      }

      self->receiver_state_ = (
        isEOM
        ? ReceiverState::WaitingForEOMAck
        : ReceiverState::WaitingForAck
      );
      break;
    }

    case ReceiverState::WaitingForAck: {
      self->receiver_state_ = ReceiverState::ReceivingByte;
      break;
    }

    case ReceiverState::WaitingForEOMAck: {
      self->receiver_state_ = ReceiverState::Idle;
      break;
    }

    default: {
      break;
    }
  }
}

void IRAM_ATTR HDMICEC::reset_state_variables_(HDMICEC *self) {
  self->recv_bit_counter_ = 0;
  self->recv_byte_buffer_ = 0x0;
  self->recv_frame_buffer_.clear();
  self->recv_frame_buffer_.reserve(16);
}

}
}
