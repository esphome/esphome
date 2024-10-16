#include "ebus.h"

namespace esphome {
namespace ebus {

TelegramBase::TelegramBase() {}

void TelegramBase::set_state(TelegramState new_state) { this->state_ = new_state; }

TelegramState TelegramBase::get_state() { return this->state_; }

#define X(name, int) \
  case int: \
    return "" #name "";
const char *TelegramBase::get_state_string() {
  switch ((int8_t) this->state_) {
    TELEGRAM_STATE_TABLE
    default:
      return "[INVALID STATE]";
  }
}
#undef X

void TelegramBase::push_buffer_(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
  if (*pos < max_pos) {
    *crc = Elf::crc8_calc(cr, *crc);
  }
  if (this->wait_for_escaped_char_) {
    buffer[(*pos)] = (cr == 0x0 ? ESC : SYN);
    this->wait_for_escaped_char_ = false;
  } else {
    buffer[(*pos)++] = cr;
    this->wait_for_escaped_char_ = (cr == ESC);
  }
}

TelegramType TelegramBase::get_type() {
  if (this->get_zz() == ESC) {
    return TelegramType::UNKNOWN;
  }
  if (this->get_zz() == BROADCAST_ADDRESS) {
    return TelegramType::BROADCAST;
  }
  if (Elf::is_primary(this->get_zz())) {
    return TelegramType::PRIMARY_PRIMARY;
  }
  return TelegramType::PRIMARY_SECONDARY;
}

int16_t TelegramBase::get_request_byte(uint8_t pos) {
  if (pos > this->get_nn() || pos >= MAX_DATA_LENGTH) {
    return -1;
  }
  return this->request_buffer_[OFFSET_DATA + pos];
}

uint8_t TelegramBase::get_request_crc() { return this->request_buffer_[OFFSET_DATA + this->get_nn()]; }

void TelegramBase::push_req_data(uint8_t cr) {
  this->push_buffer_(cr, request_buffer_, &request_buffer_pos_, &request_rolling_crc_, OFFSET_DATA + get_nn());
}

bool TelegramBase::is_ack_expected() { return (this->get_type() != TelegramType::BROADCAST); }

bool TelegramBase::is_response_expected() { return (this->get_type() == TelegramType::PRIMARY_SECONDARY); }

bool TelegramBase::is_finished() { return this->state_ < TelegramState::unknown; }

Telegram::Telegram() { this->state_ = TelegramState::waitForSyn; }

int16_t Telegram::get_response_byte(uint8_t pos) {
  if (pos > this->get_response_nn() || pos >= MAX_DATA_LENGTH) {
    return INVALID_RESPONSE_BYTE;
  }
  return this->response_buffer_[RESPONSE_OFFSET + pos];
}

uint8_t Telegram::get_response_crc() { return this->response_buffer_[RESPONSE_OFFSET + this->get_response_nn()]; }

void Telegram::push_response_data(uint8_t cr) {
  this->push_buffer_(cr, response_buffer_, &response_buffer_pos_, &response_rolling_crc_,
                     RESPONSE_OFFSET + get_response_nn());
}

bool Telegram::is_response_complete() {
  return (this->state_ > TelegramState::waitForSyn || this->state_ == TelegramState::endCompleted) &&
         (this->response_buffer_pos_ > RESPONSE_OFFSET) &&
         (this->response_buffer_pos_ == (RESPONSE_OFFSET + this->get_response_nn() + 1)) &&
         !this->wait_for_escaped_char_;
}

bool Telegram::is_response_valid() {
  return this->is_response_complete() && this->get_response_crc() == this->response_rolling_crc_;
}

bool Telegram::is_request_complete() {
  return (this->state_ > TelegramState::waitForSyn || this->state_ == TelegramState::endCompleted) &&
         (this->request_buffer_pos_ > OFFSET_DATA) &&
         (this->request_buffer_pos_ == (OFFSET_DATA + this->get_nn() + 1)) && !this->wait_for_escaped_char_;
}
bool Telegram::is_request_valid() {
  return this->is_request_complete() && this->get_request_crc() == this->request_rolling_crc_;
}

SendCommand::SendCommand() { this->state_ = TelegramState::endCompleted; }

SendCommand::SendCommand(uint8_t qq, uint8_t zz, uint16_t command, std::vector<uint8_t> &data) {
  this->state_ = TelegramState::waitForSend;
  this->push_req_data(qq);
  this->push_req_data(zz);
  this->push_req_data(command >> 8);
  this->push_req_data(command & 0xFF);
  this->push_req_data(data.size());
  for (int i = 0; i < data.size(); i++) {
    this->push_req_data(data.at(i));
  }
  this->push_req_data(this->request_rolling_crc_);
}
bool SendCommand::can_retry(int8_t max_tries) { return this->tries_count_++ < max_tries; }

uint8_t SendCommand::get_crc() { return this->request_rolling_crc_; }

}  // namespace ebus
}  // namespace esphome
