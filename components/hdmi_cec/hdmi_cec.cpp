#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef USE_UART
  #include "esphome/components/uart/uart_component.h"
#endif
#include "hdmi_cec.h"

namespace esphome {
namespace hdmi_cec {

static const char *const TAG = "hdmi_cec";
// receiver constants
static const uint32_t START_BIT_MIN_US = 3500;  // minimum duration of 'low' startbit
static const uint32_t START_BIT_NOM_US = 3700;  // nominal duration of 'low' startbit
static const uint32_t START_BIT_STRECHED_US = 4000;  // star bit got 'streched' by a bus collision with another initiator
static const uint32_t START_BIT_HIGH_US = 800;
static const uint32_t HIGH_BIT_MIN_US = 400;
static const uint32_t HIGH_BIT_MAX_US = 800;
// transmitter constants
static const uint32_t TOTAL_BIT_US = 2400;
static const uint32_t HIGH_BIT_US = 600;
static const uint32_t LOW_BIT_US = 1500;
// arbitration and retransmission
static const uint32_t MIN_SIGNAL_FREE_TIME = (TOTAL_BIT_US * 7);
static const uint32_t SIGNAL_FREE_TIME_AFTER_RECEIVE = (TOTAL_BIT_US * 5);
static const uint32_t SIGNAL_FREE_TIME_AFTER_XMIT_FAIL = (TOTAL_BIT_US * 3);
static const uint32_t SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS = (TOTAL_BIT_US * 7);
static const size_t MAX_ATTEMPTS = 5;

// static const gpio::Flags INPUT_MODE_FLAGS = gpio::FLAG_INPUT | gpio::FLAG_PULLUP;
// static const gpio::Flags OUTPUT_MODE_FLAGS = gpio::FLAG_OUTPUT;
static const gpio::Flags INPUT_MODE_FLAGS = gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP;
static const gpio::Flags OUTPUT_MODE_FLAGS = gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP;

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
  this->pin_->setup();
  this->pin_->digital_write(true);
  isr_pin_ = pin_->to_isr();
  recv_frame_buffer_.reserve(16); // max 16 bytes per CEC frame
  pin_->attach_interrupt(HDMICEC::gpio_intr_, this, gpio::INTERRUPT_ANY_EDGE);
  pin_->pin_mode(INPUT_MODE_FLAGS);
  // The UART can send with a CEC-standard bit pattern by using a 5x oversampling:
  // Each CEC-bit is created with 5 successive UART-bits.
  // The UART mode is 1 (always 0) start-bit, 8 data-bits, 1 (always 1) stop-bit: total 10 bit periods for 1 byte
  // Those 10 UART bits are used to create 2 CEC bits, each of which start low and end high.
  // So the UART baud rate is ((10 / 2) / (CEC bit period)) bits/sec,
  // which is 5 / 2400us = 2083 bits/sec or baudrate.
  // In this mode, every 2 CEC bits need to be translated into 8 UART data bits with the appropriate pattern:
  // A CEC-1 is translated into 1 low (480us) and 4 high uart bits (1920us)
  // A CEC-0 is translated into 3 low (1440us) and 2 high uart bits (960us)
  // These generated low and high periods fall well within the CEC standard presribed ranges
  #ifdef USE_UART
  if (uart_) {
    uart_->set_baud_rate(2083);
    uart_->set_data_bits(8);
    uart_->set_stop_bits(1);
    uart_->set_parity(uart::UART_CONFIG_PARITY_NONE);
    uart_->load_settings(false);
    // TODO: check/manage tx_pin mode settings??
    ESP_LOGD(TAG, "Set uart config");
  }
  #endif
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  LOG_PIN("  pin: ", pin_);
  ESP_LOGCONFIG(TAG, "  address: %x", address_);
  ESP_LOGCONFIG(TAG, "  promiscuous mode: %s", (promiscuous_mode_ ? "yes" : "no"));
  ESP_LOGCONFIG(TAG, "  monitor mode: %s", (monitor_mode_ ? "yes" : "no"));
  ESP_LOGCONFIG(TAG, "  has UART: %s", (uart_ ? "yes" : "no"));
}

void HDMICEC::loop() {
  if (!recv_queue_.empty()) {
    // handle one inbound message
    // handle 1 message per loop(), to avoid taking too much time
    handle_received_message(recv_queue_.front());
    recv_queue_.pop();
  }
  if (transmit_state_ != TransmitState::Idle && send_queue_.empty()) {
    // With a 'busy' transmit, the transmitted frame is always on the queue front
    // Error state: this shall never occur: SW bug or HW line failure?
    ESP_LOGE(TAG, "HDMICEC::send(): frame error status, force clear!");
    transmit_state_ = TransmitState::Idle;
  }
  if (!send_queue_.empty()) {
    transmit_message();
  }
  static int cnt = 0;
  if (transmit_state_ == TransmitState::Idle) {
    cnt = 0;
  } else if (cnt++ < 20) {
    const char *state_s = (transmit_state_ == TransmitState::Busy) ? "Busy" : "End";
    ESP_LOGD(TAG, "T=%d TxState=%d, RxState=%d, bitCnt=%d, ByteBuf=0x%02x, ByteCnt=%d",
             micros(), static_cast<int>(transmit_state_), static_cast<int>(receiver_state_),
             recv_bit_counter_, recv_byte_buffer_, recv_frame_buffer_.size());
  }
}

void HDMICEC::handle_received_message(const Message &frame) {
  uint8_t header = frame[0];
  uint8_t src_addr = ((header & 0xF0) >> 4);
  uint8_t dest_addr = (header & 0x0F);

  if (!promiscuous_mode_ && (dest_addr != 0x0F) && (dest_addr != address_)) {
    // ignore frames not meant for us
    return;
  }

  if (frame.size() == 1) {
    // don't process pings. they're already dealt with by the acknowledgement mechanism
    ESP_LOGD(TAG, "ping received: 0x%01X -> 0x%01X", src_addr, dest_addr);
    return;
  }

  auto frame_str = bytes_to_string(frame);
  ESP_LOGD(TAG, "frame received: %s", frame_str.c_str());

  std::vector<uint8_t> data(frame.begin() + 1, frame.end());

  // Process on_message triggers
  bool handled_by_trigger = false;
  uint8_t opcode = data[0];
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
      handled_by_trigger = true;
    }
  }

  // If nothing in on_message handled this message, we try to run the built-in handlers
  bool is_directly_addressed = (dest_addr != 0xF && dest_addr == address_);
  if (is_directly_addressed && !handled_by_trigger) {
    try_builtin_handler_(src_addr, dest_addr, data);
  }
}

void HDMICEC::transmit_message() {
  if (transmit_state_ == TransmitState::Busy) {
    // Transmit is busy, probably on the uart.
    // Need to wait until this message transmit ends;
    return;
  }

  if (transmit_state_ == TransmitState::EomAckGood) {
    // last transmit just ended successfully
    ESP_LOGD(TAG, "frame was sent successfully in %d attempt(s)", transmit_attempts_);
    transmit_attempts_ = 0;
    send_queue_.pop();
    // Maybe there is a new queue front to be handled below
  } else if (transmit_state_ == TransmitState::EomAckFail ) {
    // last transmit just ended without appropriate Acknowledge from recipient
    if (transmit_attempts_ >= MAX_ATTEMPTS) {
      ESP_LOGD(TAG, "frame was NOT acknowledged after %d attempts, drop frame", transmit_attempts_);
      transmit_attempts_ = 0;
      send_queue_.pop();       
    }
  }
  transmit_state_ = TransmitState::Idle;

  if (send_queue_.empty() ||
      (micros() < allow_xmit_message_us_)) {
    // maybe it is too early for a transmit, to satisfy the CEC standard bus idle time
    return;
  }

  // Launch the transmit of the frame that is on the front of the queue
  const Message &frame = send_queue_.front();
  transmit_state_ = TransmitState::Busy;
  transmit_attempts_++;
  transmit_acks_ok_ = true;  // expect successful acknowledges
  // the 'start_bit' and the first 4 bits of the 'header block' are always sent by software on the GPIO
  // pin to detect a bus collision and allow early termination of the frame transmit
  if (!send_start_bit_() || !transmit_my_address(frame[0])) {
    // sending these first bits caused a bus-collision with another initiator.
    // further transmission is stopped immediatly, as the other initiator might not see the collision,
    allow_xmit_message_us_ = micros() + SIGNAL_FREE_TIME_AFTER_XMIT_FAIL;  // TODO: timing of receiver?
    transmit_state_ = TransmitState::EomAckFail;
    return;
  }

  if (uart_) {
    transmit_message_on_uart(frame);
  } else {
    transmit_message_on_gpio(frame);
  }
  // don't wait here on the transmission result (Acknowledge from destination)
  // because (at least) the uart proceeds in the background
  return;
}

bool HDMICEC::transmit_my_address(const uint8_t &header) {
  // My (initiator) address is in the 4 MSB bits of the header byte (CEC transfers MSB first)
  for (int i = 7; i >= 4; i--) {
    bool bit_value = ((header >> i) & 0b1);
    send_bit_(bit_value);
  }
  // Check for bus collisions, which would make the received bus address different from the sent address
  uint8_t received_address = recv_byte_buffer_;
  bool collision = (received_address & 0xf) != ((header >> 4) & 0xf);
  if (collision) {
    ESP_LOGD(TAG, "Mismatch in sent initiator addr 0x%1x and bus seen addr 0x%1x: collision, bitcnt=%d",
             ((header >> 4) & 0xf), (received_address & 0xf), recv_bit_counter_);
  }
  return !collision;
}

uint8_t logical_address_to_device_type(uint8_t logical_address) {
  switch (logical_address) {
    // "TV"
    case 0x0:
      return 0x00; // "TV"

    // "Audio System"
    case 0x5:
      return 0x05; // "Audio System"

    // "Recording 1"
    case 0x1:
    // "Recording 2"
    case 0x2:
    // "Recording 3"
    case 0x9:
      return 0x01; // "Recording Device"

    // "Tuner 1"
    case 0x3:
    // "Tuner 2"
    case 0x6:
    // "Tuner 3"
    case 0x7:
    // "Tuner 4"
    case 0xA:
      return 0x03; // "Tuner"

    default:
      return 0x04; // "Playback Device"
  }
}

void HDMICEC::try_builtin_handler_(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data) {
  if (data.empty()) {
    return;
  }

  uint8_t opcode = data[0];
  switch (opcode) {
    // "Get CEC Version" request
    case 0x9F: {
      // reply with "CEC Version" (0x9E)
      send(address_, source, {0x9E, 0x04});
      break;
    }

    // "Give Device Power Status" request
    case 0x8F: {
      // reply with "Report Power Status" (0x90)
      send(address_, source, {0x90, 0x00}); // "On"
      break;
    }

    // "Give OSD Name" request
    case 0x46: {
      // reply with "Set OSD Name" (0x47)
      std::vector<uint8_t> data = { 0x47 };
      data.insert(data.end(), osd_name_bytes_.begin(), osd_name_bytes_.end());
      send(address_, source, data);
      break;
    }

    // "Give Physical Address" request
    case 0x83: {
      // reply with "Report Physical Address" (0x84)
      auto physical_address_bytes = decode_value(physical_address_);
      std::vector<uint8_t> data = { 0x84 };
      data.insert(data.end(), physical_address_bytes.begin(), physical_address_bytes.end());
      // Device Type
      data.push_back(logical_address_to_device_type(address_));
      // Broadcast Physical Address
      send(address_, 0xF, data);
      break;
    }

    // Ignore "Feature Abort" opcode responses
    case 0x00:
      // no-op
      break;

    // default case (no built-in handler + no on_message handler) => message not supported => send "Feature Abort"
    default:
      send(address_, source, {0x00, opcode, 0x00});
      break;
  }
}

bool HDMICEC::send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data_bytes) {
  if (monitor_mode_) return false;

  bool is_broadcast = (destination == 0xF);

  // prepare the bytes to send
  uint8_t header = (((source & 0x0F) << 4) | (destination & 0x0F));
  std::vector<uint8_t> frame = { header };
  frame.insert(frame.end(), data_bytes.begin(), data_bytes.end());

  std::string bytes_to_send = bytes_to_string(frame);
  ESP_LOGD(TAG, "Queing frame to send: %s", bytes_to_send.c_str());

  {
    LockGuard send_lock(send_mutex_);
    send_queue_.push(frame);
  }
  return true;
}

void IRAM_ATTR HDMICEC::transmit_message_on_gpio(const std::vector<uint8_t> &frame) {
  // InterruptLock interrupt_lock;
  // switch_to_send_mode_();
  // send_start_bit_();
  
  // for each byte of the frame:
  bool success = true;
  for (auto it = frame.begin(); it != frame.end(); ++it) {
    uint8_t current_byte = *it;

    // 1. send the current byte
    bool partial_first_byte = (it == frame.begin());
    for (int8_t i = (partial_first_byte ? 3 : 7); i >= 0; i--) {
      bool bit_value = ((current_byte >> i) & 0b1);
      send_bit_(bit_value);
    }

    // 2. send EOM bit (logic 1 if this is the last byte of the frame)
    bool is_eom = (it == (frame.end() - 1));
    send_bit_(is_eom);

    // 3. send ack bit
    send_bit_(true);  // always send a 1, check elsewhere if our receiver sees a 1 or 0
  }

  // switch_to_listen_mode_();

  // return success;
}

bool IRAM_ATTR HDMICEC::send_start_bit_() {
  // TODO: improve 
  // 1. pull low for 3700 us
  pin_->digital_write(false);
  delay_microseconds_safe(START_BIT_NOM_US);


  // 2. pull high for 800 us
  pin_->digital_write(true);
  delay_microseconds_safe(800);

  // total duration of start bit: 4500 us
  if (transmit_bus_collision_) {
    ESP_LOGD(TAG, "Send frame: bus collision during start-bit!");
  }
  return !transmit_bus_collision_;
}

void IRAM_ATTR HDMICEC::send_bit_(bool bit_value) {
  // total bit duration:
  // logic 1: pull low for 600 us, then pull high for 1800 us
  // logic 0: pull low for 1500 us, then pull high for 900 us

  const uint32_t low_duration_us = (bit_value ? HIGH_BIT_US : LOW_BIT_US);
  const uint32_t high_duration_us = (TOTAL_BIT_US - low_duration_us);

  pin_->digital_write(false);
  delay_microseconds_safe(low_duration_us);
  pin_->digital_write(true);
  delay_microseconds_safe(high_duration_us);
}

bool IRAM_ATTR HDMICEC::send_and_read_ack_(bool is_broadcast) {
  uint32_t start_us = micros();

  // send a Logical 1
  pin_->digital_write(false);
  delay_microseconds_safe(HIGH_BIT_US);
  pin_->digital_write(true);

  // switch to input mode...
  pin_->pin_mode(INPUT_MODE_FLAGS);

  // ...then wait up to the middle of the "Safe sample period" (CEC spec -> Signaling and Bit Timing -> Figure 5)
  static const uint32_t SAFE_SAMPLE_US = 1050;
  delay_microseconds_safe(SAFE_SAMPLE_US - (micros() - start_us));
  bool value = pin_->digital_read();

  pin_->pin_mode(OUTPUT_MODE_FLAGS);
  pin_->digital_write(true);

  // sleep for the rest of the bit period
  delay_microseconds_safe(TOTAL_BIT_US - (micros() - start_us));

  // broadcast messages: line pulled low by any follower => something went wrong. no need to flip the value.
  if (is_broadcast) {
    return value;
  }

  // normal messages: line pulled low by the target follower => message ACKed successfully. we need to flip the value to match that logic.
  return (!value);
}

void IRAM_ATTR HDMICEC::switch_to_listen_mode_() {
  // TODO: remove all mode switching
  pin_->pin_mode(INPUT_MODE_FLAGS);
}

void IRAM_ATTR HDMICEC::switch_to_send_mode_() {
  pin_->pin_mode(OUTPUT_MODE_FLAGS);
  pin_->digital_write(true);
}

void IRAM_ATTR HDMICEC::transmit_message_on_uart(const Message &frame) {
  pin_->digital_write(true);  // make sure gpio output is 'high' (and is pull-up), so uart can pull low
  std::vector<uint8_t> uart_data;
  for (int i = 0; i < frame.size(); i++) {
      transmit_byte_on_uart(frame[i], i == 0, i == (frame.size() - 1));
  }
}

void IRAM_ATTR HDMICEC::transmit_byte_on_uart(uint8_t byte, bool is_header, bool is_eom) {
  // 5 uart-bits create the nominal 2.4ms cec bit period
  // 10 uart-bits are made with an (always-0) uart start bit, then 8 data bit, and an (always 1) uart stop bit.
  // transmitting a '0' data bit gets translated to a uart 3xlow, 2xhigh on the cec line
  // transmitting a '1' data bit gets translated to a uart 1xlow, 4xhigh on the cec line
  // Note that a CEC 'header/data block' byte is sent MSB (Most Significant Bit) first,
  // whereas the bytes as given to the UART are sent out with LSB first.
  //   cec2bit data     10-bit pattern in transmission order      stripped start/stop, swap lsb-first, uart data byte
  //        00                    0001100011                           10001100 = 8c
  //        01                    0001101111                           11101100 = ec
  //        10                    0111100011                           10001111 = 8f
  //        11                    0111101111                           11101111 = ef
  #ifdef USE_UART
  static const std::array<uint8_t, 4> cec2bit_to_uartbyte = {0x8c, 0xec, 0x8f, 0xef};
  std::array<uint8_t, 5> uart_bytes;
  uint16_t cec_block = ((uint16_t)byte) << 2;
  cec_block |= (is_eom ? 0x2 : 0);  // add eom (end-of-message) bit
  cec_block |= 0x1;  // add ack bit, is always written as 1
  // send block MSB-first, but skip initial 4 bits of header byte as those have already been sent
  uint8_t nbytes = is_header ? 3 : 5;
  for (int i = 0; i < nbytes; i++, cec_block >>= 2) {
    uint8_t twobits = cec_block & 0x3;
    uart_bytes[nbytes - 1 - i] = cec2bit_to_uartbyte[twobits];
  }
  uart_->write_array(uart_bytes.data(), nbytes);
  // TODO: is this buffered in the uart, and can be called again quickly?
  #endif
}

void IRAM_ATTR HDMICEC::gpio_intr_(HDMICEC *self) {
  const uint32_t now = micros();
  const bool level = self->isr_pin_.digital_read();

  // on falling edge, store current time as the start of the low pulse
  if (level == false) {
    self->last_falling_edge_us_ = now;

    if (self->recv_ack_queued_ && !self->monitor_mode_) {
      self->recv_ack_queued_ = false;
      {
        // InterruptLock interrupt_lock; Needed when already inside an ISR?
        self->isr_pin_.pin_mode(OUTPUT_MODE_FLAGS);
        self->isr_pin_.digital_write(false);
        delay_microseconds_safe(LOW_BIT_US);
        self->isr_pin_.digital_write(true);
        self->isr_pin_.pin_mode(INPUT_MODE_FLAGS);
      }
    }

    return;
  }
  // otherwise, it's a rising edge, so it's time to process the pulse length 

  self->allow_xmit_message_us_ = now + SIGNAL_FREE_TIME_AFTER_RECEIVE;

  auto pulse_duration = (now - self->last_falling_edge_us_);

  if (pulse_duration >= START_BIT_STRECHED_US && self->transmit_state_ == TransmitState::Busy) {
    // the receiver gets the frame that the transmitter is currently sending
    // On a too long start bit, report a bus collision to support the transmitter
    // Our transmitter should stop sending, but we might continue receiving from the other initiator
    self->transmit_bus_collision_ = true;
  }
  
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
        self->recv_frame_buffer_.push_back((uint8_t)(self->recv_byte_buffer_));

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
      if (self->transmit_state_ == TransmitState::Busy) {
        self->transmit_receives_ack(false, value);
      }
      self->receiver_state_ = ReceiverState::ReceivingByte;
      break;
    }

    case ReceiverState::WaitingForEOMAck: {
      if (self->transmit_state_ == TransmitState::Busy) {
        self->transmit_receives_ack(true, value);
      }
      self->receiver_state_ = ReceiverState::Idle;
      break;
    }

    default: {
      break;
    }
  }
}

void IRAM_ATTR HDMICEC::transmit_receives_ack(bool is_eom, bool ack_value) {
  // currently a transmit is busy
  // TODO: don't need to check for broadcast in each ISR invocation if we just count the ack values
  bool is_broadcast = ((send_queue_.front())[0] & 0x0f) == 0x0f;
  bool ack_ok = ack_value == is_broadcast;
  transmit_acks_ok_ &= ack_ok;
  if (is_eom) {
    transmit_state_ = transmit_acks_ok_ ? TransmitState::EomAckGood : TransmitState::EomAckFail;
    allow_xmit_message_us_ = micros() +
      (transmit_acks_ok_ ? SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS : SIGNAL_FREE_TIME_AFTER_XMIT_FAIL);
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
