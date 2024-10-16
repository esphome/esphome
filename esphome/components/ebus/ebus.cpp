#include "ebus.h"

#include <utility>

namespace esphome {
namespace ebus {

uint8_t Ebus::uart_send_char_(uint8_t cr, bool esc, bool run_crc, uint8_t crc_init) {
  char buffer[2];
  uint8_t crc = 0;
  uint8_t len = 1;
  if (esc && cr == ESC) {
    buffer[0] = ESC;
    buffer[1] = 0x00;
    len = 2;
  } else if (esc && cr == SYN) {
    buffer[0] = ESC;
    buffer[1] = 0x01;
    len = 2;
  } else {
    buffer[0] = cr;
  }
  uart_send_(buffer, len);
  if (!run_crc) {
    return 0;
  }
  crc = Elf::crc8_calc(buffer[0], crc_init);
  if (len == 1) {
    return crc;
  }
  return Elf::crc8_calc(buffer[1], crc);
}

void Ebus::uart_send_char_(uint8_t cr, bool esc) { this->uart_send_char_(cr, esc, false, 0); }

void Ebus::uart_send_remaining_request_part_(SendCommand &command) {
  this->uart_send_char_(command.get_zz());
  this->uart_send_char_(command.get_pb());
  this->uart_send_char_(command.get_sb());
  this->uart_send_char_(command.get_nn());
  for (int i = 0; i < command.get_nn(); i++) {
    this->uart_send_char_((uint8_t) command.get_request_byte(i));
  }
  this->uart_send_char_(command.get_crc());
}

void Ebus::process_received_char(uint8_t received_byte) {
  // keep track of number of character between last 2 SYN chars
  // this is needed in case of arbitration
  if (received_byte == SYN) {
    this->state_ = this->char_count_since_last_syn_ == 1 ? EbusState::ARBITRATION : EbusState::NORMAL;
    this->char_count_since_last_syn_ = 0;

    if (this->lock_counter_ > 0 && this->state_ == EbusState::NORMAL) {
      this->lock_counter_--;
    }

  } else {
    this->char_count_since_last_syn_++;
  }

  if (this->receiving_telegram_.is_finished()) {
    if (this->queue_received_telegram_) {
      this->queue_received_telegram_(this->receiving_telegram_);
    }
    this->receiving_telegram_ = Telegram();
  }

  if (this->active_command_.is_finished() && this->dequeue_command_) {
    SendCommand dequeued;
    if (this->dequeue_command_(&dequeued)) {
      this->active_command_ = dequeued;
    }
  }

  switch (this->receiving_telegram_.get_state()) {
    case TelegramState::waitForSyn:
      if (received_byte == SYN) {
        this->receiving_telegram_.set_state(TelegramState::waitForArbitration);
      }
      break;
    case TelegramState::waitForArbitration:
      if (received_byte != SYN) {
        this->receiving_telegram_.push_req_data(received_byte);
        this->receiving_telegram_.set_state(TelegramState::waitForRequestData);
      }
      break;
    case TelegramState::waitForRequestData:
      if (received_byte == SYN) {
        if (this->receiving_telegram_.get_zz() == ESC) {
          this->receiving_telegram_.set_state(TelegramState::endArbitration);
        } else {
          this->receiving_telegram_.set_state(TelegramState::endErrorUnexpectedSyn);
        }
      } else {
        this->receiving_telegram_.push_req_data(received_byte);
        if (this->receiving_telegram_.is_request_complete()) {
          this->receiving_telegram_.set_state(this->receiving_telegram_.is_ack_expected()
                                                  ? TelegramState::waitForRequestAck
                                                  : TelegramState::endCompleted);
        }
      }
      break;
    case TelegramState::waitForRequestAck:
      switch (received_byte) {
        case ACK:
          this->receiving_telegram_.set_state(this->receiving_telegram_.is_response_expected()
                                                  ? TelegramState::waitForResponseData
                                                  : TelegramState::endCompleted);
          break;
        case NACK:
          this->receiving_telegram_.set_state(TelegramState::endErrorRequestNackReceived);
          break;
        default:
          this->receiving_telegram_.set_state(TelegramState::endErrorRequestNoAck);
      }
      break;
    case TelegramState::waitForResponseData:
      if (received_byte == SYN) {
        this->receiving_telegram_.set_state(TelegramState::endErrorUnexpectedSyn);
      } else {
        this->receiving_telegram_.push_response_data(received_byte);
        if (this->receiving_telegram_.is_response_complete()) {
          this->receiving_telegram_.set_state(TelegramState::waitForResponseAck);
        }
      }
      break;
    case TelegramState::waitForResponseAck:
      switch (received_byte) {
        case ACK:
          this->receiving_telegram_.set_state(TelegramState::endCompleted);
          break;
        case NACK:
          this->receiving_telegram_.set_state(TelegramState::endErrorResponseNackReceived);
          break;
        default:
          this->receiving_telegram_.set_state(TelegramState::endErrorResponseNoAck);
      }
      break;
    default:
      break;
  }

  switch (this->active_command_.get_state()) {
    case TelegramState::waitForSend:
      if (received_byte == SYN && state_ == EbusState::NORMAL && this->lock_counter_ == 0) {
        this->active_command_.set_state(TelegramState::waitForArbitration);
        this->uart_send_char_(this->active_command_.get_qq());
      }
      break;
    case TelegramState::waitForArbitration:
      if (received_byte == this->active_command_.get_qq()) {
        // we won arbitration
        this->uart_send_remaining_request_part_(this->active_command_);
        if (this->active_command_.is_ack_expected()) {
          this->active_command_.set_state(TelegramState::waitForCommandAck);
        } else {
          this->active_command_.set_state(TelegramState::endCompleted);
          this->lock_counter_ = this->max_lock_counter_;
        }
      } else if (Elf::get_priority_class(received_byte) == Elf::get_priority_class(this->active_command_.get_qq())) {
        // eligible for round 2
        this->active_command_.set_state(TelegramState::waitForArbitration2nd);
      } else {
        // lost arbitration, try again later if retries left
        this->active_command_.set_state(this->active_command_.can_retry(this->max_tries_)
                                            ? TelegramState::waitForSend
                                            : TelegramState::endSendFailed);
      }
      break;
    case TelegramState::waitForArbitration2nd:
      if (received_byte == SYN) {
        this->uart_send_char_(this->active_command_.get_qq());
      } else if (received_byte == this->active_command_.get_qq()) {
        // won round 2
        this->uart_send_remaining_request_part_(this->active_command_);
        if (this->active_command_.is_ack_expected()) {
          this->active_command_.set_state(TelegramState::waitForCommandAck);
        } else {
          this->active_command_.set_state(TelegramState::endCompleted);
          this->lock_counter_ = this->max_lock_counter_;
        }
      } else {
        // try again later if retries left
        this->active_command_.set_state(this->active_command_.can_retry(this->max_tries_)
                                            ? TelegramState::waitForSend
                                            : TelegramState::endSendFailed);
      }
      break;
    case TelegramState::waitForCommandAck:
      if (received_byte == ACK) {
        this->active_command_.set_state(TelegramState::endCompleted);
        this->lock_counter_ = this->max_lock_counter_;
      } else if (received_byte == SYN) {  // timeout waiting for ACK signaled by AUTO-SYN
        this->active_command_.set_state(this->active_command_.can_retry(this->max_tries_)
                                            ? TelegramState::waitForSend
                                            : TelegramState::endSendFailed);
      }
      break;
    default:
      break;
  }

  // responses to our commands are stored in receiving_telegram_
  // when response is completed send ACK or NACK when we were the primary
  if (this->receiving_telegram_.get_state() == TelegramState::waitForResponseAck &&
      this->receiving_telegram_.get_qq() == this->primary_address_) {
    if (this->receiving_telegram_.is_response_valid()) {
      this->uart_send_char_(ACK);
      this->uart_send_char_(SYN, false);
    } else {
      this->uart_send_char_(NACK);
    }
  }

  // Handle our responses
  this->handle_response_(this->receiving_telegram_);
}

void Ebus::add_send_response_handler(std::function<std::vector<uint8_t>(Telegram &)> send_response_handler) {
  send_response_handlers_.push_back(send_response_handler);
}

void Ebus::handle_response_(Telegram &telegram) {
  if (telegram.get_state() != TelegramState::waitForRequestAck ||
      telegram.get_zz() != Elf::to_secondary(this->primary_address_)) {
    return;
  }
  if (!telegram.is_request_valid()) {
    uart_send_char_(NACK);
    return;
  }

  // response buffer
  std::vector<uint8_t> reply;

  // find response
  for (auto const &handler : send_response_handlers_) {
    reply = handler(telegram);
    if (reply.size() != 0) {
      break;
    }
  }

  // we found no reponse to send
  if (reply.size() == 0 || reply.size() > RESPONSE_BUFFER_SIZE) {
    uart_send_char_(NACK);
    return;
  }

  uart_send_char_(ACK);
  uint8_t crc = Elf::crc8_calc(reply.size(), 0);
  uart_send_char_(reply.size());
  for (int i = 0; i < reply.size(); i++) {
    crc = uart_send_char_(reply[i], true, true, crc);
  }
  uart_send_char_(crc);
}

uint8_t Elf::crc8_calc(uint8_t data, uint8_t crc_init) {
  uint8_t crc;
  uint8_t polynom;

  crc = crc_init;
  for (int i = 0; i < 8; i++) {
    if (crc & 0x80) {
      polynom = 0x9B;
    } else {
      polynom = 0;
    }
    crc = ((crc & ~0x80) << 1);
    if (data & 0x80) {
      crc = (crc | 1);
    }
    crc = (crc ^ polynom);
    data = (data << 1);
  }
  return (crc);
}

uint8_t Elf::crc8_array(uint8_t data[], uint8_t length) {
  uint8_t uc_crc = 0;
  for (int i = 0; i < length; i++) {
    uc_crc = crc8_calc(data[i], uc_crc);
  }
  return (uc_crc);
}

bool Elf::is_primary(uint8_t address) {
  return is_primary_nibble(get_priority_class(address)) &&  //
         is_primary_nibble(get_sub_address(address));
}

int Elf::is_primary_nibble(uint8_t nibble) {
  switch (nibble) {
    case 0b0000:
    case 0b0001:
    case 0b0011:
    case 0b0111:
    case 0b1111:
      return true;
    default:
      return false;
  }
}

uint8_t Elf::get_priority_class(uint8_t address) { return (address & 0x0F); }

uint8_t Elf::get_sub_address(uint8_t address) { return (address >> 4); }

uint8_t Elf::to_secondary(uint8_t address) {
  if (is_primary(address)) {
    return (address + 5) % 0xFF;
  }
  return address;
}

}  // namespace ebus
}  // namespace esphome
