#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <array>
#include <atomic>

#ifdef HAVE_UART
#include "esphome/components/uart/uart_component.h"
#endif
#include "hdmi_cec.h"

#ifdef USE_CEC_DECODER
#include "cec_decoder.h"
#endif

namespace esphome {
namespace hdmi_cec {

// CEC protocol constants as stated in standard:
static constexpr uint8_t MAX_FRAME_LENGTH_BYTES = 16;  // max frame (message) length in bytes
static constexpr uint32_t START_BIT_MIN_US = 3500;     // minimum duration of 'low' startbit
static constexpr uint32_t START_BIT_NOM_US = 3700;     // nominal duration of 'low' startbit
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
static const char *const TAG = "hdmi_cec";
static constexpr gpio::Flags PIN_MODE_FLAGS =
    gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP;

Frame::Frame(uint8_t initiator_addr, uint8_t target_addr, const std::vector<uint8_t> &payload) {
  this->at(0) = ((initiator_addr & 0xf) << 4) | (target_addr & 0xf);  // header byte
  size_ = std::min((int) (1 + payload.size()), MAX_FRAME_LENGTH);
  std::memcpy(this->data() + 1, payload.data(), size_ - 1);  // payload bytes, not exceeding capacity
}

std::string Frame::to_string() const {
  std::string result;
  char part_buffer[3];
  for (auto it = this->cbegin(); it != this->cend(); it++) {
    uint8_t byte_value = *it;
    snprintf(part_buffer, sizeof(part_buffer), "%02X", byte_value);
    result += part_buffer;

    if (it != (this->end() - 1)) {
      result += ":";
    }
  }
#ifdef USE_CEC_DECODER
  Decoder decoder(*this);
  result += " => " + decoder.decode();
#endif
  return result;
}

void HDMICEC::setup() {
  xmit_.setup(pin_);
  recv_.setup(pin_, address_);
}

void HDMICEC::set_pin(InternalGPIOPin *pin) { pin_ = pin; }

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  ESP_LOGCONFIG(TAG, "  address: 0x%x", address_);
  ESP_LOGCONFIG(TAG, "  physical address: 0x%04x", physical_address_);
  xmit_.dump_config();
  recv_.dump_config();
}

void HDMICEC::loop() {
  if (const Frame *msg = recv_.frames_queue_.front()) {
    // handle 1 inbound message per loop(), to avoid taking too much time
    handle_received_message_(msg);
    recv_.frames_queue_.push_front();
  }
  if (!xmit_.is_idle()) {
    xmit_.transmit_message();
  }
  // Need more frequent service to handle pending workload?
  if (!recv_.frames_queue_.is_empty() || !xmit_.is_idle()) {
    fast_loop_.start();
  } else {
    fast_loop_.stop();
  }
}

std::string HDMICEC::get_state() const { return recv_.get_state() + "; " + xmit_.get_state(); }

void HDMICEC::handle_received_message_(const Frame *frame) {
  const uint8_t src_addr = frame->initiator_addr();
  const uint8_t dest_addr = frame->destination_addr();
  const uint8_t opcode = frame->opcode();

  if (frame->size() == 1) {
    // don't process pings. they're already dealt with by the acknowledgement mechanism
    ESP_LOGV(TAG, "Received ping: 0x%01X -> 0x%01X", src_addr, dest_addr);
    return;
  }

  auto frame_str = frame->to_string();
  ESP_LOGD(TAG, "Received: %s", frame_str.c_str());

  std::vector<uint8_t> data(frame->begin() + 1, frame->end());

  // Process on_message triggers
  bool handled_by_trigger = false;
  for (auto *trigger : message_triggers_) {
    bool can_trigger =
        ((!trigger->source_.has_value() || (trigger->source_ == src_addr)) &&
         (!trigger->destination_.has_value() || (trigger->destination_ == dest_addr)) &&
         (!trigger->opcode_.has_value() || (trigger->opcode_ == opcode)) &&
         (!trigger->data_.has_value() || (data.size() >= trigger->data_->size() &&
                                          std::equal(trigger->data_->begin(), trigger->data_->end(), data.begin()))));
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

// Derive CEC device type from 'logical address'
uint8_t HDMICEC::get_device_type() const {
  switch (address_) {
    // "TV"
    case 0x0:
      return 0x00;  // "TV"
    // "Audio System"
    case 0x5:
      return 0x05;  // "Audio System"
    // "Recording 1"
    case 0x1:
    // "Recording 2"
    case 0x2:
    // "Recording 3"
    case 0x9:
      return 0x01;  // "Recording Device"
    // "Tuner 1"
    case 0x3:
    // "Tuner 2"
    case 0x6:
    // "Tuner 3"
    case 0x7:
    // "Tuner 4"
    case 0xA:
      return 0x03;  // "Tuner"
    default:
      return 0x04;  // "Playback Device"
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
      // reply with "CEC Version" (0x9E), "1.4" (0x5)
      send(address_, source, {0x9E, 0x05});
      break;
    }

    // "Give Device Power Status" request
    case 0x8F: {
      // reply with "Report Power Status" (0x90)
      send(address_, source, {0x90, 0x00});  // "On"
      break;
    }

    // "Give OSD Name" request
    case 0x46: {
      // reply with "Set OSD Name" (0x47)
      std::vector<uint8_t> data = {0x47};
      data.insert(data.end(), osd_name_bytes_.begin(), osd_name_bytes_.end());
      send(address_, source, data);
      break;
    }

    // "Give Physical Address" request
    case 0x83: {
      // reply with "Report Physical Address" (0x84)
      auto physical_address_bytes = decode_value(physical_address_);
      std::vector<uint8_t> data = {0x84};
      data.insert(data.end(), physical_address_bytes.begin(), physical_address_bytes.end());
      // Device Type
      data.push_back(get_device_type());
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

  Frame frame(source, destination, data_bytes);
  ESP_LOGV(TAG, "Queing frame to send: %s", frame.to_string().c_str());
  xmit_.queue_for_send(std::move(frame));
  return true;
}

inline void IRAM_ATTR CECTransmit::set_pin_input_high() { pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); }

inline void IRAM_ATTR CECTransmit::set_pin_output_low() {
  pin_->pin_mode(gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);
  pin_->digital_write(false);
}

void CECTransmit::setup(InternalGPIOPin *pin) {
  // This CEC software component optionally uses a UART to transmit (write) on the cec bus.
  // When used, the uart output pin is connected to the GPIO CEC pin through a diode: cathode to uart pin.
  // This makes sure the uart can only pull-down the CEC line, not pull-up.
  // (It seems that specifying 'open-drain' mode for the uart pin has no effect!)
  pin_ = pin;
  set_pin_input_high();
  pin_->setup();
  set_pin_input_high();

#ifdef HAVE_UART
  if (uart_) {
    uart_->set_baud_rate(2083);
    uart_->set_data_bits(8);
    uart_->set_stop_bits(1);
    uart_->set_parity(uart::UART_CONFIG_PARITY_NONE);
#if defined(USE_ESP8266) || defined(USE_ESP32)
    uart_->load_settings(false);
#endif  // USE_ESP8266 || USE_ESP32
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

void CECTransmit::queue_for_send(Frame &&frame) {
  LockGuard send_lock(send_mutex_);  // prevent simultaneous modifications to the queue
  send_queue_.push(std::move(frame));
}

std::string CECTransmit::get_state() const {
  char line[64];
  const static std::array<const char *, 3> NAMES = {"IDLE", "BUSY", "EOM_CONFIRMED"};
  const TransmitState s = transmit_state_;  // take atomic value
  snprintf(line, sizeof(line), "Tx State=%s", NAMES[static_cast<int>(s)]);
  return std::string(line);
}

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
    // the Frame which is on the queue.front is confirmed to be fully sent out
    const Frame &frame = send_queue_.front();
    uint8_t n_acks_expected = frame.is_broadcast() ? 0 : frame.size();  // for broadcast, acknowledge is bad
    bool sent_ok = (n_bytes_received_ == frame.size()) && (n_acks_received_ == n_acks_expected);
    // Create log message for debugging
    if (!sent_ok) {
      // last transmit had a byte count error, or ended without appropriate Acknowledge from recipient
      if (n_bytes_received_ != frame.size()) {
        ESP_LOGD(TAG, "Send frame incorrect on attempt %d, saw %d bytes but expected %d bytes", transmit_attempts_,
                 static_cast<int>(n_bytes_received_), frame.size());
      } else {
        ESP_LOGD(TAG, "Send frame NOT acknowledged on attempt %d", transmit_attempts_);
      }
    } else {
      // last transmit just ended successfully
      ESP_LOGD(TAG, "Send frame success in %d attempt%s", transmit_attempts_, (transmit_attempts_ > 1 ? "s" : ""));
    }

    // terminate working on the current frame?
    if (sent_ok) {
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
  if (transmit_attempts_ >= MAX_ATTEMPTS) {
    ESP_LOGD(TAG, "frame was NOT sent correctly after %d attempts, drop frame", transmit_attempts_);
    allow_xmit_message_us_ = confirm_received_us_ + SIGNAL_FREE_TIME_AFTER_XMIT_SUCCESS;
    transmit_attempts_ = 0;
    LockGuard send_lock(send_mutex_);
    send_queue_.pop();
  }

  if (send_queue_.empty() || receiver_is_busy_ || (micros() < allow_xmit_message_us_)) {
    // maybe it is too early for a transmit, to satisfy the CEC standard bus idle time
    return;
  }

  // Launch the transmit of the frame that is on the front of the queue
  const Frame &frame = send_queue_.front();
  transmit_state_ = TransmitState::BUSY;
  transmit_attempts_++;
  if (transmit_attempts_ <= 1) {
    ESP_LOGD(TAG, "Sending: %s", frame.to_string().c_str());
  }
  // the 'start_bit' and the first 4 bits of the 'header block' are always sent by software on the GPIO
  // pin to detect a bus collision and allow early termination of the frame transmit
  if (!send_start_bit_() || !transmit_my_address_(frame.initiator_addr())) {
    // sending these first bits caused a bus-collision with another initiator.
    // further transmission is stopped immediatly, as the other initiator might not see the collision,
    allow_xmit_message_us_ = micros() + SIGNAL_FREE_TIME_AFTER_XMIT_FAIL;
    transmit_state_ = TransmitState::IDLE;
    return;
  }
  confirm_received_us_ = micros();  // initialize timing guard
  if (uart_) {
    transmit_message_on_uart_(frame);
  } else {
    transmit_message_on_gpio_(frame);
  }
  // don't wait here on the transmision result (Acknowledge from destination)
  // because (at least) the uart proceeds in the background
}

bool CECTransmit::transmit_my_address_(const uint8_t initiator_addr) {
  // My (initiator) address is in the 4 MSB bits of the header byte (CEC transfers MSB first)
  bool ok = true;
  for (int i = 3; i >= 0; i--) {
    bool bit_value = ((initiator_addr >> i) & 0x1);
    if (bit_value) {
      ok &= send_high_and_test_();
    } else {
      send_bit_(false);
    }
  }
  // Check for bus collisions, which would make the received bus address different from the sent address
  if (!ok) {
    ESP_LOGD(TAG, "Bus collision on sending my initiator addr 0x%1x on attempt %d", initiator_addr, transmit_attempts_);
  }
  return ok;
}

void CECTransmit::transmit_message_on_gpio_(const Frame &frame) {
  // for each byte of the frame:
  for (auto it = frame.begin(); it != frame.end(); ++it) {
    uint8_t current_byte = *it;

    // 1. send the current byte
    bool partial_first_byte = (it == frame.begin());
    for (int8_t i = (partial_first_byte ? 3 : 7); i >= 0; i--) {
      // send MSB (Most Significant Bit) first
      bool bit_value = ((current_byte >> i) & 0b1);
      send_bit_(bit_value);
    }

    // 2. send EOM (End Of Frame) bit (logic 1 if this is the last byte of the frame)
    bool is_eom = (it == (frame.end() - 1));
    send_bit_(is_eom);

    // 3. send ACK (Acknowledge) bit
    send_bit_(true);  // always send a 1, check elsewhere if our receiver sees a 1 or 0
  }
}

bool CECTransmit::send_start_bit_() {
  set_pin_input_high();
  bool value = pin_->digital_read();
  if (!value) {
    ESP_LOGD(TAG, "Bus occupied before start-bit on attempt %d", transmit_attempts_);
    return false;
  }
  // The CEC line was not yet occupied (pulled low) by someone else
  // 1. pull low for the start-bit duration
  set_pin_output_low();
  delay_microseconds_safe(START_BIT_NOM_US);

  // 2. let the line go high again, but test if no one else keeps it low
  set_pin_input_high();
  delay_microseconds_safe(START_BIT_HIGH_US / 2);
  value &= pin_->digital_read();
  delay_microseconds_safe(START_BIT_HIGH_US / 2);
  value &= pin_->digital_read();

  if (!value) {
    ESP_LOGD(TAG, "Bus collision on sending start-bit on attempt %d", transmit_attempts_);
  }
  return value;
}

void IRAM_ATTR CECTransmit::send_bit_(bool bit_value) {
  // total bit duration:
  // logic 1: pull low for 600 us, then pull high for 1800 us
  // logic 0: pull low for 1500 us, then pull high for 900 us

  const uint32_t low_duration_us = (bit_value ? HIGH_BIT_US : LOW_BIT_US);
  const uint32_t high_duration_us = (TOTAL_BIT_US - low_duration_us);

  set_pin_output_low();
  delay_microseconds_safe(low_duration_us);
  set_pin_input_high();
  delay_microseconds_safe(high_duration_us);
}

bool IRAM_ATTR CECTransmit::send_high_and_test_() {
  uint32_t start_us = micros();

  // send a Logical 1
  set_pin_output_low();
  delay_microseconds_safe(HIGH_BIT_US);
  set_pin_input_high();

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

void CECTransmit::transmit_message_on_uart_(const Frame &frame) {
  std::vector<uint8_t> uart_data;
  uart_data.reserve(5 * frame.size());  // the UART is used with 5x oversampling (5 uart bytes per cec byte)
  for (unsigned int i = 0; i < frame.size(); i++) {
    convert_byte_to_uart_(uart_data, frame[i], i == 0, i == (frame.size() - 1));
  }
#ifdef HAVE_UART
  uart_->write_array(uart_data);  // if not 'HAVE_UART', the include file is missing and this cannot be compiled
#endif
}

void CECTransmit::convert_byte_to_uart_(std::vector<uint8_t> &uart_data, uint8_t byte, bool is_header, bool is_eom) {
  // 5 uart-bits create the nominal 2.4ms cec bit period, with our baudrate of 2083 bits/sec.
  // 10 uart-bits are made with an (always 0) uart start bit, then 8 data bit, and an (always 1) uart stop bit.
  // transmitting a '0' data bit gets translated to a uart 3xlow, 2xhigh on the cec line
  // transmitting a '1' data bit gets translated to a uart 1xlow, 4xhigh on the cec line
  // Note that a CEC 'header/data block' byte is sent MSB (Most Significant Bit) first,
  // whereas the bytes as given to the UART are sent out with LSB first.
  //  cec 2-bit data    10-bit pattern in transmission order    stripped start/stop, swap lsb-first, uart data byte
  //        00                    0001100011                           10001100 = 8c
  //        01                    0001101111                           11101100 = ec
  //        10                    0111100011                           10001111 = 8f
  //        11                    0111101111                           11101111 = ef
  static const std::array<uint8_t, 4> CEC2BIT_TO_UARTBYTE = {0x8c, 0xec, 0x8f, 0xef};
  uint16_t cec_block = ((uint16_t) byte) << 2;  // expand byte to 10 bits
  cec_block |= (is_eom ? 0x2 : 0);              // insert eom (end-of-message) bit
  cec_block |= 0x1;                             // insert ack bit, is always written as 1
  // send 10-bit block MSB-first, but skip initial 4 bits of header byte as those have already been sent
  const uint8_t nbytes = is_header ? 3 : 5;
  for (int i = nbytes - 1; i >= 0; i--) {
    uint8_t twobits = (cec_block >> (2 * i)) & 0x3;  // get most-signifiant bits first
    uart_data.push_back(CEC2BIT_TO_UARTBYTE[twobits]);
  }
}

void IRAM_ATTR CECTransmit::send_ack() {
  // This method is called by the receiver (gpio_isr). When receiving a message, this transmitter
  // is expected to be idle. The only exception to that would be the rather abnormal case
  // where we transmit a message to ourselves (address_ == target_address).
#ifdef HAVE_UART
  if (transmit_state_ == TransmitState::IDLE) {
    // transmit a '0' with 3 'low' uart bit periods, one of which is the uart start-bit.
    // So, the uart byte to send has its 2 least-significant bits 0.
    const uint8_t ack_byte = 0xfc;
    uart_->write_byte(ack_byte);  // issue on the uart, don't wait on completion
    return;
  }
#endif
  // send an ack bit by direct gpio pin manipulation
  // This has the serious disadvantage that the receiver isr is kept
  // busy for a serious amount of time. (Thus blocking other system interrupts.)
  set_pin_output_low();
  delay_microseconds_safe(LOW_BIT_US);
  set_pin_input_high();
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
    ATOMIC_INCR(n_bytes_received_);
    // 'ack value == 0' means that the addressed device on the bus confirms receipt by pulling 'ack' low.
    // (But for broadcast messages, 'ack == 0' indicates that some receiver denies the message.)
    if (!ack) {
      // 0 bit value means 'acknowledge' for an addressed message
      ATOMIC_INCR(n_acks_received_);
    }
    if (eom) {
      transmit_state_ = TransmitState::EOM_CONFIRMED;
    }
  }
}

void CECReceive::setup(InternalGPIOPin *pin, uint8_t address) {
  address_ = address;
  pin->attach_interrupt(CECReceive::gpio_isr_s, this, gpio::INTERRUPT_ANY_EDGE);
  isr_pin_ = pin->to_isr();
  frames_queue_.reset();
  reset_state_variables_();
}

void CECReceive::dump_config() {
  ESP_LOGCONFIG(TAG, "  monitor mode: %s", (monitor_mode_ ? "yes" : "no"));
  ESP_LOGCONFIG(TAG, "  promiscuous mode: %s", (promiscuous_mode_ ? "yes" : "no"));
}

std::string CECReceive::get_state() const {
  char line[128];
  const static std::array<const char *, 5> NAMES = {"IDLE", "RECEIVING_BYTE", "WAITING_FOR_EOM", "WAITING_FOR_ACK",
                                                    "WAITING_FOR_EOM_ACK"};
  const int rcv_state = static_cast<int>(receiver_state_);
  const int rcv_cnt = static_cast<int>(recv_bit_counter_);
  snprintf(line, sizeof(line), "Rx State=%s, bytecnt=%d + bitcnt=%d", NAMES[rcv_state], recv_frame_buffer_->size(),
           rcv_cnt);
  return std::string(line);
}

void IRAM_ATTR CECReceive::gpio_isr_s(CECReceive *self) { self->gpio_isr_(); }

void IRAM_ATTR CECReceive::gpio_isr_() {
  const uint32_t now = micros();
  const bool level = isr_pin_.digital_read();

  if (level == prev_pin_level_) {
    // spurious interrupt, probably triggered by a mode-change of the cec gpio
    return;
  }
  prev_pin_level_ = level;

  // on falling edge, store current time as the start of the low pulse
  if (!level) {
    if (now - last_falling_edge_us_ > START_BIT_NOM_US + TOTAL_BIT_US) {
      // there was a very long period of silence on the bus: reset state
      reset_state_variables_();
    }

    last_falling_edge_us_ = now;

    if (receiver_state_ == ReceiverState::IDLE) {
      // inform transmitter on new bus activity
      xmit_.got_start_of_activity();
    }

    if (recv_ack_queued_ && !monitor_mode_) {
      // create Acknowledge bit on the CEC bus by stretching the 'low' period
      // Use the Transmitter to immediatly send this ack bit
      xmit_.send_ack();
      recv_ack_queued_ = false;
    }
    return;
  }
  // otherwise, it's a rising edge, so it's time to process the pulse length

  const uint32_t pulse_duration = (now - last_falling_edge_us_);
  if (pulse_duration > START_BIT_MIN_US) {
    // start bit detected. reset everything and start receiving
    reset_state_variables_();  // abort any previously gathered (unfinished) state
    receiver_state_ = ReceiverState::RECEIVING_BYTE;
    recv_frame_buffer_ = frames_queue_.back();
    return;
  } else if (pulse_duration < (HIGH_BIT_MIN_US / 4)) {
    // short glitch on the line: ignore
    return;
  }

  bool value = (pulse_duration <= HIGH_BIT_MAX_US);  // short low pulse represents '1'

  switch (receiver_state_) {
    case ReceiverState::RECEIVING_BYTE: {
      // write bit to the current byte
      recv_byte_buffer_ = (recv_byte_buffer_ << 1) | (value & 0x1);
      ATOMIC_INCR(recv_bit_counter_);
      if (recv_bit_counter_ >= 8) {
        // if we reached eight bits, push the current byte to the frame buffer
        if (recv_frame_buffer_) {
          recv_frame_buffer_->push_back((uint8_t) (recv_byte_buffer_));
        }
        recv_bit_counter_ = 0;
        recv_byte_buffer_ = 0;
        receiver_state_ = ReceiverState::WAITING_FOR_EOM;  // expect 9th bit of block: EOM
      }
      break;
    }

    case ReceiverState::WAITING_FOR_EOM: {
      // check if we need to acknowledge this byte on the next 'ack' bit (10th bit of block)
      if (recv_frame_buffer_ && !recv_frame_buffer_->is_broadcast() &&
          recv_frame_buffer_->destination_addr() == address_ && recv_frame_buffer_->initiator_addr() != address_) {
        // Note: Do NOT acknowledge a message initiated by myself,
        // to support dynamic 'Logical Address' allocation by sending 'polling' messages
        recv_ack_queued_ = true;
      }
      bool is_eom = (value == 1);
      receiver_state_ = is_eom ? ReceiverState::WAITING_FOR_EOM_ACK : ReceiverState::WAITING_FOR_ACK;
      break;
    }

    case ReceiverState::WAITING_FOR_ACK: {
      xmit_.got_byte_eom_ack(false, value);             // pass time of received byte, eom and ack, to transmitter
      receiver_state_ = ReceiverState::RECEIVING_BYTE;  // no EOM, so expect next byte for this frame
      break;
    }

    case ReceiverState::WAITING_FOR_EOM_ACK: {
      xmit_.got_byte_eom_ack(true, value);  // pass time of received byte, eom and ack, to transmitter
      // Check if the application could be interested in this message
      if (recv_frame_buffer_ &&
          (promiscuous_mode_ ||
           (recv_frame_buffer_->is_broadcast() && (recv_frame_buffer_->initiator_addr() != address_)) ||
           (recv_frame_buffer_->destination_addr() == address_))) {
        frames_queue_.push_back();  // commit the received frame for later pick-up by 'front()'
      }
      reset_state_variables_();
      break;
    }

    default: {
      break;
    }
  }
}

void IRAM_ATTR CECReceive::reset_state_variables_() {
  receiver_state_ = ReceiverState::IDLE;
  num_acks_ = 0;
  recv_bit_counter_ = 0;
  recv_byte_buffer_ = 0;
  recv_ack_queued_ = false;
  recv_frame_buffer_ = nullptr;
}

}  // namespace hdmi_cec
}  // namespace esphome
