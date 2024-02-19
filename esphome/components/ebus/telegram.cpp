#include "ebus.h"

namespace esphome {
namespace ebus {

TelegramBase::TelegramBase() {
}

void TelegramBase::set_state(TelegramState new_state) {
  this->state = new_state;
}

TelegramState TelegramBase::get_state() {
  return this->state;
}

#define X(name, int) case int: return ""#name"";
const char * TelegramBase::get_state_string() {
  switch((int8_t) this->state) {
    TELEGRAM_STATE_TABLE
    default:
      return "[INVALID STATE]";
  }
}
#undef X


void TelegramBase::push_buffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
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
  if (this->getZZ() == ESC) {
    return TelegramType::Unknown;
  }
  if (this->getZZ() == BROADCAST_ADDRESS) {
    return TelegramType::Broadcast;
  }
  if (Elf::is_primary(this->getZZ())) {
    return TelegramType::PrimaryPrimary;
  }
  return TelegramType::PrimarySecondary;
}

int16_t TelegramBase::get_request_byte(uint8_t pos) {
  if (pos > this->getNN() || pos >= MAX_DATA_LENGTH) {
    return -1;
  }
  return this->request_buffer[OFFSET_DATA + pos];
}

uint8_t TelegramBase::get_request_crc() {
  return this->request_buffer[OFFSET_DATA + this->getNN()];
}

void TelegramBase::push_req_data(uint8_t cr) {
  this->push_buffer(cr, request_buffer, &request_buffer_pos, &request_rolling_crc, OFFSET_DATA + getNN());
}

bool TelegramBase::is_ack_expected() {
  return (this->get_type() != TelegramType::Broadcast);
}

bool TelegramBase::is_response_expected() {
  return (this->get_type() == TelegramType::PrimarySecondary);
}

bool TelegramBase::is_finished() {
  return this->state < TelegramState::unknown;
}


Telegram::Telegram() {
  this->state = TelegramState::waitForSyn;
}

int16_t Telegram::get_response_byte(uint8_t pos) {
  if (pos > this->getResponseNN() || pos >= MAX_DATA_LENGTH) {
    return INVALID_RESPONSE_BYTE;
  }
  return this->response_buffer[RESPONSE_OFFSET + pos];
}

uint8_t Telegram::get_response_crc() {
  return this->response_buffer[RESPONSE_OFFSET + this->getResponseNN()];
}

void Telegram::push_response_data(uint8_t cr) {
  this->push_buffer(cr, response_buffer, &response_buffer_pos, &response_rolling_crc, RESPONSE_OFFSET + getResponseNN());
}

bool Telegram::is_response_complete() {
  return (this->state > TelegramState::waitForSyn || this->state == TelegramState::endCompleted) &&
         (this->response_buffer_pos > RESPONSE_OFFSET) &&
         (this->response_buffer_pos == (RESPONSE_OFFSET + this->getResponseNN() + 1)) &&
         !this->wait_for_escaped_char_;
}

bool Telegram::is_response_valid() {
  return this->is_response_complete() && this->get_response_crc() == this->response_rolling_crc;
}

bool Telegram::is_request_complete() {
  return (this->state > TelegramState::waitForSyn || this->state == TelegramState::endCompleted) &&
         (this->request_buffer_pos > OFFSET_DATA) &&
         (this->request_buffer_pos == (OFFSET_DATA + this->getNN() + 1)) && !this->wait_for_escaped_char_;
}
bool Telegram::is_request_valid() {
  return this->is_request_complete() && this->get_request_crc() == this->request_rolling_crc;
}


SendCommand::SendCommand() {
  this->state = TelegramState::endCompleted;
}

SendCommand::SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data) {
  this->state = TelegramState::waitForSend;
  this->push_req_data(QQ);
  this->push_req_data(ZZ);
  this->push_req_data(PB);
  this->push_req_data(SB);
  this->push_req_data(NN);
  for (int i = 0; i < NN; i++) {
    this->push_req_data(data[i]);
  }
  this->push_req_data(this->request_rolling_crc);
}

bool SendCommand::can_retry(int8_t max_tries) {
  return this->tries_count_++ < max_tries;
}

uint8_t SendCommand::get_crc() {
  return this->request_rolling_crc;
}

}  // namespace ebus
}  // namespace esphome
