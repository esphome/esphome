#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef HAVE_UART
  #include "esphome/components/uart/uart_component.h"
#endif
#include "hdmi_cec.h"

namespace esphome {
namespace hdmi_cec {

// CEC protocol constants as stated in standard:
static constexpr uint8_t MAX_FRAME_LENGTH_BYTES = 16;  // max frame (message) length in bytes
static constexpr uint32_t START_BIT_MIN_US = 3500;  // minimum duration of 'low' startbit
static constexpr uint32_t START_BIT_NOM_US = 3700;  // nominal duration of 'low' startbit
static constexpr uint32_t START_BIT_HIGH_US = 800;
static constexpr uint32_t HIGH_BIT_MIN_US = 400;
static constexpr uint32_t HIGH_BIT_MAX_US = 800;
static constexpr uint32_t TOTAL_BIT_US = 2400;
static constexpr uint32_t HIGH_BIT_US = 600;
static constexpr uint32_t LOW_BIT_US = 1500;
static constexpr uint32_t SIGNAL_FREE_TIME_AFTER_RECEIVE = (TOTAL_BIT_US * 5);
static constexpr uint32_t SIGNAL_FREE_TIME_AFTER_XMIT_FAIL = (TOTAL_BIT_US * 3);
static constexpr uint32_t SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS = (TOTAL_BIT_US * 7);
static constexpr uint8_t MAX_ATTEMPTS = 5;

// constants used for this implementation:
static constexpr char *const TAG = "hdmi_cec";
static constexpr gpio::Flags PIN_MODE_FLAGS = gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP;

Message::Message(uint8_t initiator_addr, uint8_t target_addr, const std::vector<uint8_t> &payload)
  : std::vector<uint8_t>(1u + payload.size(), (uint8_t)(0)) {
  auto inx = this->begin();
  *inx++ = ((initiator_addr & 0xf) << 4) | (target_addr & 0xf);
  this->insert(inx, payload.cbegin(), payload.cend());
}

std::string Message::to_string() const {
  std::string result;
  char part_buffer[3];
  for (auto it = this->cbegin(); it != this->cend(); it++) {
    uint8_t byte_value = *it;
    sprintf(part_buffer, "%02X", byte_value);
    result += part_buffer;

    if (it != (this->end() - 1)) {
      result += ":";
    }
  }
  return result;
}

void HDMICEC::setup() {
  xmit_.setup(pin_);
  recv_.setup(pin_, address_);

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
  // The uart output is connected to the GPIO CEC pin with a diode: catode to uart pin.
  // This makes sure the uart can only pull-down the CEC line.
  // (It seems that specifying 'open-drain' mode for the uart pin has no effect)
  // A small-signal diode type is not critical, a schottky type is preferred:
  // such as schottky small-signal low-leakage types: 1N5711, BAT85, BAT46, BAT42, BAS70
  // Otherwise small-signal plain (non-schottky) types:
}

void HDMICEC::set_pin(InternalGPIOPin *pin) {
  pin_ = pin;
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  ESP_LOGCONFIG(TAG, "  address: 0x%x", address_);
  ESP_LOGCONFIG(TAG, "  physical address: 0x%04x", physical_address_);
  xmit_.dump_config();
  recv_.dump_config();
}

void HDMICEC::loop() {
  if (recv_.has_received_message()) {
    // handle one inbound message
    // handle 1 message per loop(), to avoid taking too much time
    Message mesg = recv_.take_received_message();
    handle_received_message(mesg);
  }
  if (!xmit_.is_idle()) {
    xmit_.transmit_message();
  }
#if 0
  static int cnt = 0;
  if (xmit_.is_idle()) {
    cnt = 0;
  } else if (cnt++ < 20) {
    ESP_LOGD(TAG, "T=%d RxState=%d, bitCnt=%d, ByteBuf=0x%02x, ByteCnt=%d",
             micros(), static_cast<int>(receiver_state_),
             recv_bit_counter_, recv_byte_buffer_, recv_frame_buffer_.size());
  }
#endif
}

void HDMICEC::handle_received_message(const Message &frame) {
  uint8_t header = frame[0];
  uint8_t src_addr = ((header & 0xF0) >> 4);
  uint8_t dest_addr = (header & 0x0F);

  if (frame.size() == 1) {
    // don't process pings. they're already dealt with by the acknowledgement mechanism
    ESP_LOGD(TAG, "ping received: 0x%01X -> 0x%01X", src_addr, dest_addr);
    return;
  }

  auto frame_str = frame.to_string();
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

bool HDMICEC::send(uint8_t destination, const std::vector<uint8_t> &data_bytes) {
  return send(address_, destination, data_bytes);
}

bool HDMICEC::send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data_bytes) {
  if (recv_.get_monitor_mode()) {
    // in 'monitor mode' no presence on the CEC bus is shown, so we don't send
    return false;
  }

  Message frame(source, destination, data_bytes);
  ESP_LOGD(TAG, "Queing frame to send: %s", frame.to_string().c_str());
  {
    xmit_.queue_for_send(std::move(frame));
  }
  return true;
}

void CECTransmit::setup(InternalGPIOPin *pin) {
  pin_ = pin;
  pin_->pin_mode(PIN_MODE_FLAGS);
  pin_->digital_write(true);  // make the 'open_drain' output high-impedance
  pin_->setup();

  #ifdef HAVE_UART
  if (uart_) {
    uart_->set_baud_rate(2083);
    uart_->set_data_bits(8);
    uart_->set_stop_bits(1);
    uart_->set_parity(uart::UART_CONFIG_PARITY_NONE);
    uart_->load_settings(false);
    ESP_LOGD(TAG, "Set uart configuration for CEC");
  }
  #else
    uart_ = nullptr;  // probably superfluous, just to be safe and clear
  #endif
}

void CECTransmit::dump_config() {
  LOG_PIN("  pin: ", pin_);
  ESP_LOGCONFIG(TAG, "  has UART: %s", (uart_ ? "yes" : "no"));
}

// void CECTransmit::queue_for_send(const Message &&frame) {
//   LockGuard send_lock(send_mutex_);  // prevent simultaneous modifications to the queue
//   send_queue_.push(std::move(frame));
// }

void CECTransmit::transmit_message() {
  if (transmit_state_ != TransmitState::IDLE && send_queue_.empty()) {
    // With a 'busy' transmit, the transmitted frame is always on the queue front
    // Error state: this shall never occur: SW bug or HW line failure?
    ESP_LOGE(TAG, "HDMICEC::transmit_message(): frame error status, force clear!");
    transmit_state_ = TransmitState::IDLE;
    return;
  }

  if (receiver_is_busy_ && (confirm_received_us_ + 15 * TOTAL_BIT_US < micros())) {
    // protocol error on the bus between receiver and transmitter, this should never occur!
    // The Receiver has stopped giving byte_eom_ack confirmations, which should occur after
    // every received byte, which is 10x bit-period. So, >=15 bit periods is an error.
    // (Normally, the sequence of bits ends with an 'eom' confirmation, which changes the state out of BUSY.)
    // Force now leaving the BUSY state to avoid a deadlock by waiting indefinitely.
    receiver_is_busy_ = false;
    if (transmit_state_ == TransmitState::BUSY) {
      transmit_state_ = TransmitState::EOM_CONFIRMED;
    }
  }

  if (transmit_state_ == TransmitState::BUSY) {
    // Transmit is busy, probably on the uart.
    // Need to wait until this message transmit ends, from confirmation by receiver.
    return;
  }

  if (transmit_state_ == TransmitState::EOM_CONFIRMED) {
    const Message &frame = send_queue_.front();
    uint8_t n_acks_expected = frame.is_broadcast() ? 0 : frame.size();  // for broadcast, acknowledge is bad
    bool sent_ok = (n_bytes_received_ == frame.size()) && (n_acks_received_ == n_acks_expected);
    // Create log message for debugging
    if (!sent_ok) {
      // last transmit had a byte count error, or ended without appropriate Acknowledge from recipient
      if (transmit_attempts_ >= MAX_ATTEMPTS) {
        if (n_bytes_received_ != frame.size()) {
          ESP_LOGD(TAG, "frame was sent incorrectly after %d attempts, saw %d bytes but expected %d bytes",
                   transmit_attempts_, n_bytes_received_, frame.size());
        } else {
          ESP_LOGD(TAG, "frame was NOT acknowledged after %d attempts, drop frame", transmit_attempts_);
        }
      }
    } else {
      // last transmit just ended successfully
      ESP_LOGD(TAG, "frame was sent successfully in %d attempt%s",
               transmit_attempts_, (transmit_attempts_ > 1 ? "s" : ""));
    }

    // terminate working on the current frame?
    if (sent_ok || transmit_attempts_ >= MAX_ATTEMPTS) {
      allow_xmit_message_us_ = confirm_received_us_ + SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS;
      transmit_attempts_ = 0;
      LockGuard send_lock(send_mutex_);
      send_queue_.pop();
    } else {
      allow_xmit_message_us_ = confirm_received_us_ + SIGNAL_FREE_TIME_AFTER_XMIT_FAIL;
    }
    // reset confirmation from receiver
    n_bytes_received_ = 0;
    n_acks_received_ = 0;
  }

  transmit_state_ = TransmitState::IDLE;
  if (send_queue_.empty() ||
      receiver_is_busy_ ||
      (micros() < allow_xmit_message_us_)) {
    // maybe it is too early for a transmit, to satisfy the CEC standard bus idle time
    return;
  }

  // Launch the transmit of the frame that is on the front of the queue
  const Message &frame = send_queue_.front();
  transmit_state_ = TransmitState::BUSY;
  transmit_attempts_++;
  if (transmit_attempts_ <= 1) {
    ESP_LOGD(TAG, "Send message from queue: %s", frame.to_string().c_str());
  } else {
    ESP_LOGD(TAG, "Send message from queue: attempt %d", transmit_attempts_);
  }
  // the 'start_bit' and the first 4 bits of the 'header block' are always sent by software on the GPIO
  // pin to detect a bus collision and allow early termination of the frame transmit
  if (!send_start_bit() || !transmit_my_address(frame.initiator_addr())) {
    // sending these first bits caused a bus-collision with another initiator.
    // further transmission is stopped immediatly, as the other initiator might not see the collision,
    allow_xmit_message_us_ = micros() + SIGNAL_FREE_TIME_AFTER_XMIT_FAIL;
    transmit_state_ = TransmitState::IDLE;
    return;
  }
  confirm_received_us_ = micros();  // initialize timing guard
  if (uart_) {
    transmit_message_on_uart(frame);
  } else {
    transmit_message_on_gpio(frame);
  }
  // don't wait here on the transmission result (Acknowledge from destination)
  // because (at least) the uart proceeds in the background
  return;
}

bool CECTransmit::transmit_my_address(const uint8_t initiator_addr) {
  // My (initiator) address is in the 4 MSB bits of the header byte (CEC transfers MSB first)
  bool ok = true;
  for (int i = 3; i >= 0; i--) {
    bool bit_value = ((initiator_addr >> i) & 0x1);
    if (bit_value) {
      ok &= send_high_and_test();
    } else {
      send_bit(false);
    }
  }
  // Check for bus collisions, which would make the received bus address different from the sent address
  if (!ok) {
    ESP_LOGD(TAG, "Bus collision while sending my initiator addr 0x%1x", initiator_addr);
  }
  return ok;
}

void CECTransmit::transmit_message_on_gpio(const Message &frame) {
  // for each byte of the frame:
  for (auto it = frame.begin(); it != frame.end(); ++it) {
    uint8_t current_byte = *it;

    // 1. send the current byte
    bool partial_first_byte = (it == frame.begin());
    for (int8_t i = (partial_first_byte ? 3 : 7); i >= 0; i--) {
      // send MSB (Most Significant Bit) first
      bool bit_value = ((current_byte >> i) & 0b1);
      send_bit(bit_value);
    }

    // 2. send EOM (End Of Message) bit (logic 1 if this is the last byte of the frame)
    bool is_eom = (it == (frame.end() - 1));
    send_bit(is_eom);

    // 3. send ACK (Acknowledge) bit
    send_bit(true);  // always send a 1, check elsewhere if our receiver sees a 1 or 0
  }
}

bool CECTransmit::send_start_bit() {
  bool value = pin_->digital_read();
  if (value) {
    // The CEC line was not yet occupied (pulled low) by someone else
    // 1. pull low for the start-bit duration
    pin_->digital_write(false);
    delay_microseconds_safe(START_BIT_NOM_US);

    // 2. let the line go high again, but test if no one else keeps it low
    pin_->digital_write(true);
    delay_microseconds_safe(START_BIT_HIGH_US / 2);
    value &= pin_->digital_read();
    delay_microseconds_safe(START_BIT_HIGH_US / 2);
    value &= pin_->digital_read();
  }

  if (!value) {
    ESP_LOGD(TAG, "Send frame: bus collision during start-bit!");
  }
  return value;
}

void IRAM_ATTR CECTransmit::send_bit(bool bit_value) {
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

bool IRAM_ATTR CECTransmit::send_high_and_test() {
  uint32_t start_us = micros();

  // send a Logical 1
  pin_->digital_write(false);
  delay_microseconds_safe(HIGH_BIT_US);
  pin_->digital_write(true);

  // ...then wait up to the middle of the "Safe sample period" (CEC spec -> Signaling and Bit Timing -> Figure 5)
  static constexpr uint32_t SAFE_SAMPLE_US = 1050;
  delay_microseconds_safe(SAFE_SAMPLE_US - (micros() - start_us));
  bool value = pin_->digital_read();

  // sleep for the rest of the bit period
  delay_microseconds_safe(TOTAL_BIT_US - (micros() - start_us));

  // If a 'high' value was read, the 'low' pulse was short, not lengthened by another driver.
  // Such short pulse represents a 'high' bit.
  return value;
}

void CECTransmit::transmit_message_on_uart(const Message &frame) {
  std::vector<uint8_t> uart_data;
  uart_data.reserve(5 * frame.size());  // the UART is used with 5x oversampling (5 uart bytes per cec byte)
  for (int i = 0; i < frame.size(); i++) {
    convert_byte_to_uart(uart_data, frame[i], i == 0, i == (frame.size() - 1));
  }
  #ifdef HAVE_UART
  uart_->write_array(uart_data);  // if not 'HAVE_UART', the include file is missing and this cannot be compiled
  #endif
}

void CECTransmit::convert_byte_to_uart(std::vector<uint8_t> &uart_data, uint8_t byte, bool is_header, bool is_eom) {
  // 5 uart-bits create the nominal 2.4ms cec bit period, with our baudrate of 2083 bits/sec.
  // 10 uart-bits are made with an (always-0) uart start bit, then 8 data bit, and last an (always 1) uart stop bit.
  // transmitting a '0' data bit gets translated to a uart 3xlow, 2xhigh on the cec line
  // transmitting a '1' data bit gets translated to a uart 1xlow, 4xhigh on the cec line
  // Note that a CEC 'header/data block' byte is sent MSB (Most Significant Bit) first,
  // whereas the bytes as given to the UART are sent out with LSB first.
  //  cec 2-bit data    10-bit pattern in transmission order    stripped start/stop, swap lsb-first, uart data byte
  //        00                    0001100011                           10001100 = 8c
  //        01                    0001101111                           11101100 = ec
  //        10                    0111100011                           10001111 = 8f
  //        11                    0111101111                           11101111 = ef
  static const std::array<uint8_t, 4> cec2bit_to_uartbyte = {0x8c, 0xec, 0x8f, 0xef};
  uint16_t cec_block = ((uint16_t)byte) << 2; // expand byte to 10 bits
  cec_block |= (is_eom ? 0x2 : 0);  // insert eom (end-of-message) bit
  cec_block |= 0x1;  // insert ack bit, is always written as 1
  // send 10-bit block MSB-first, but skip initial 4 bits of header byte as those have already been sent
  const uint8_t nbytes = is_header ? 3 : 5;
  for (int i = nbytes - 1; i >= 0; i--) {
    uint8_t twobits = (cec_block >> (2 * i)) & 0x3;  // get most-signifiant bits first
    uart_data.push_back(cec2bit_to_uartbyte[twobits]);
  }
}

bool IRAM_ATTR CECTransmit::send_ack_with_uart() {
  // transmit a '0' with 3 'low' uart bit periods, one of which is the uart start-bit.
  // So, the uart byte to send has its 2 most-significant bits 0.
  #ifdef HAVE_UART
  if (transmit_state_ == TransmitState::IDLE) {
    const uint8_t ack_byte = 0x3f;
    uart_->write_byte(ack_byte);
    return true;
  }
  #endif
  // This method is called by the receiver. When receiving a message, this transmitter
  // is expected to be idle. The only exception to that would be the rather abnormal case
  // where we transmit a message to ourselves (address_ == target_address).
  return false;
}

void IRAM_ATTR CECTransmit::got_start_of_activity() {
  // bus is occupied, inhibit send
  receiver_is_busy_ = true;
}

void IRAM_ATTR CECTransmit::got_byte_eom_ack(bool eom, bool ack) {
  confirm_received_us_ = micros();
  if (eom) {
    // some bus transfer ended. When the bus is free for a little while, we are allowed to transmit.
    allow_xmit_message_us_ = confirm_received_us_ + SIGNAL_FREE_TIME_AFTER_RECEIVE;
    receiver_is_busy_ = false;
  }
  if (transmit_state_ == TransmitState::BUSY) {
    // this received message was sent by myself, handle this confirmation to check acknowledgement
    n_bytes_received_++;
    // 'ack value == 0' means that the addressed device on the bus confirms receipt by pulling 'ack' low.
    // (But for broadcast messages, 'ack == 0' indicates that some receiver denies the message.)
    n_acks_received_ += (ack ? 0 : 1);  // 0 bit value means 'acknowledge' for an addressed message
    if (eom) {
      transmit_state_ = TransmitState::EOM_CONFIRMED;
    }
  }
}

void CECReceive::setup(InternalGPIOPin *pin, uint8_t address) {
  address_ = address;
  pin->attach_interrupt(CECReceive::gpio_isr_s, this, gpio::INTERRUPT_ANY_EDGE);
  isr_pin_ = pin->to_isr();
  reset_state_variables();
}

void CECReceive::dump_config() {
  ESP_LOGCONFIG(TAG, "  monitor mode: %s", (monitor_mode_ ? "yes" : "no"));
  ESP_LOGCONFIG(TAG, "  promiscuous mode: %s", (promiscuous_mode_ ? "yes" : "no"));
}

Message CECReceive::take_received_message() {
  InterruptLock lock;  // prevent simultaneous queue modification from the gpio_isr
  Message msg = recv_queue_.front();
  recv_queue_.pop();
  return msg;
}

void IRAM_ATTR CECReceive::gpio_isr_s(CECReceive *self) {
  self->gpio_isr();
}

void IRAM_ATTR CECReceive::gpio_isr() {
  const uint32_t now = micros();
  const bool level = isr_pin_.digital_read();

  // on falling edge, store current time as the start of the low pulse
  if (level == false) {
    last_falling_edge_us_ = now;

    if (receiver_state_ == ReceiverState::IDLE) {
      // inform transmitter on new bus activity 
      xmit_.got_start_of_activity();
    }

    if (recv_ack_queued_ && !monitor_mode_) {
      // create Acknowledge bit on the CEC bus by stretching the 'low' period
      recv_ack_queued_ = false;
      if (!(xmit_.has_uart() && xmit_.send_ack_with_uart())) {
        // put the ack bit here on the gpio pin.
        // Unfortunately, this keeps this isr function busy for a rather long time
        isr_pin_.digital_write(false);
        delay_microseconds_safe(LOW_BIT_US);
        isr_pin_.digital_write(true);
      }
    }
    return;
  }
  // otherwise, it's a rising edge, so it's time to process the pulse length 

  const uint32_t pulse_duration = (now - last_falling_edge_us_);
  if (pulse_duration < (HIGH_BIT_MIN_US / 5)) {
    // spurious edge, forget about this
    return;
  }
  
  if (pulse_duration > START_BIT_MIN_US) {
    // start bit detected. reset everything and start receiving
    reset_state_variables();  // abort any previously gathered (unfinished) state
    receiver_state_ = ReceiverState::RECEIVING_BYTE;
    return;
  }

  bool value = (pulse_duration <= HIGH_BIT_MAX_US);  // short low pulse represents '1'
  
  switch (receiver_state_) {
    case ReceiverState::RECEIVING_BYTE: {
      // write bit to the current byte
      recv_byte_buffer_ = (recv_byte_buffer_ << 1) | (value & 0x1);
      recv_bit_counter_++;
      if (recv_bit_counter_ >= 8) { 
        // if we reached eight bits, push the current byte to the frame buffer
        recv_frame_buffer_.push_back((uint8_t)(recv_byte_buffer_));
        recv_bit_counter_ = 0;
        recv_byte_buffer_ = 0;
        receiver_state_ = ReceiverState::WAITING_FOR_EOM;  // expect 9th bit of block: EOM
      }
      break;
    }

    case ReceiverState::WAITING_FOR_EOM: {
      // check if we need to acknowledge this byte on the next 'ack' bit (10th bit of block)
      if (!recv_frame_buffer_.is_broadcast() && recv_frame_buffer_.destination_addr() == address_) {
        recv_ack_queued_ = true;
      }
      bool isEOM = (value == 1);
      receiver_state_ =
        isEOM
        ? ReceiverState::WAITING_FOR_EOM_ACK
        : ReceiverState::WAITING_FOR_ACK;
      break;
    }

    case ReceiverState::WAITING_FOR_ACK: {
      xmit_.got_byte_eom_ack(false, value);  // pass time of received byte, eom and ack, to transmitter
      receiver_state_ = ReceiverState::RECEIVING_BYTE;  // no EOM, so expect next byte for this frame
      break;
    }

    case ReceiverState::WAITING_FOR_EOM_ACK: {
      xmit_.got_byte_eom_ack(true, value);  // pass time of received byte, eom and ack, to transmitter
      if (promiscuous_mode_ ||
          recv_frame_buffer_.is_broadcast() ||
          (recv_frame_buffer_.destination_addr() == address_)) {
        // we are interested in this message, push to application
        recv_queue_.push(recv_frame_buffer_);  // TODO: safe inside isr?
      }
      reset_state_variables();
      break;
    }

    default: {
      break;
    }
  }
}


void IRAM_ATTR CECReceive::reset_state_variables() {
  receiver_state_ = ReceiverState::IDLE;
  num_acks_ = 0;
  recv_bit_counter_ = 0;
  recv_byte_buffer_ = 0;
  recv_ack_queued_ = false;
  recv_frame_buffer_.clear();
  recv_frame_buffer_.reserve(MAX_FRAME_LENGTH_BYTES);
}

}
}
