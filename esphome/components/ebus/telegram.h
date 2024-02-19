#pragma once

namespace esphome {
namespace ebus {

enum EbusState : int8_t {
  normal,
  arbitration,
};

enum TelegramType : int8_t {
  Unknown = -1,
  Broadcast = 0,
  PrimaryPrimary = 1,
  PrimarySecondary = 2,
};

#define TELEGRAM_STATE_TABLE \
  X(waitForSyn, 1) \
  X(waitForSend, 2) \
  X(waitForRequestData, 3) \
  X(waitForRequestAck, 4) \
  X(waitForResponseData, 5) \
  X(waitForResponseAck, 6) \
  X(waitForArbitration, 7) \
  X(waitForArbitration2nd, 8) \
  X(waitForCommandAck, 9) \
  X(unknown, 0) \
  X(endErrorUnexpectedSyn, -1) \
  X(endErrorRequestNackReceived, -2) \
  X(endErrorResponseNackReceived, -3) \
  X(endErrorResponseNoAck, -4) \
  X(endErrorRequestNoAck, -5) \
  X(endArbitration, -6) \
  X(endCompleted, -16) \
  X(endSendFailed, -17)

#define X(name, int) name = int,
enum TelegramState : int8_t { TELEGRAM_STATE_TABLE };
#undef X

const uint8_t SYN = 0xAA;
const uint8_t ESC = 0xA9;
const uint8_t ACK = 0x00;
const uint8_t NACK = 0xFF;

const uint8_t BROADCAST_ADDRESS = 0xFE;

/* Specification says:
   1. In primary and secondary telegram part, standardised commands must be limited to 10 used data bytes.
   2. In primary and secondary telegram part, the sum of mfr.-specific telegram used data bytes must not exceed 14.
   We use 16 to be on the safe side for now.
*/
const uint8_t MAX_DATA_LENGTH = 16;
const uint8_t OFFSET_QQ = 0;
const uint8_t OFFSET_ZZ = 1;
const uint8_t OFFSET_PB = 2;
const uint8_t OFFSET_SB = 3;
const uint8_t OFFSET_NN = 4;
const uint8_t OFFSET_DATA = 5;
const uint8_t REQUEST_BUFFER_SIZE = (OFFSET_DATA + MAX_DATA_LENGTH + 1);
const uint8_t RESPONSE_BUFFER_SIZE = (MAX_DATA_LENGTH + 2);
const uint8_t RESPONSE_OFFSET = 1;
const uint8_t INVALID_RESPONSE_BYTE = -1;

class TelegramBase {
 public:
  TelegramBase();

  uint8_t getQQ() { return this->request_buffer[OFFSET_QQ]; }
  uint8_t getZZ() { return this->request_buffer[OFFSET_ZZ]; }
  uint8_t getPB() { return this->request_buffer[OFFSET_PB]; }
  uint8_t getSB() { return this->request_buffer[OFFSET_SB]; }
  uint16_t getCommand() { return ((uint16_t) getPB()) << 8 | getSB(); }
  uint8_t getNN() {
    uint8_t nn = this->request_buffer[OFFSET_NN];
    if (nn >= MAX_DATA_LENGTH) {
      return 0;
    }
    return nn;
  }

  void set_state(TelegramState new_state);
  TelegramState get_state();
  const char *get_state_string();

  TelegramType get_type();
  int16_t get_request_byte(uint8_t pos);
  uint8_t get_request_crc();
  void push_req_data(uint8_t cr);
  bool is_ack_expected();
  bool is_response_expected();
  bool is_finished();

 protected:
  TelegramState state;
  uint8_t request_buffer[REQUEST_BUFFER_SIZE] = {
      ESC, ESC};  // initialize QQ and ZZ with ESC char to distinguish from valid primary 0
  uint8_t request_buffer_pos = 0;
  uint8_t request_rolling_crc = 0;
  bool wait_for_escaped_char_ = false;
  void push_buffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos);
};

class Telegram : public TelegramBase {
 public:
  Telegram();

  uint8_t getResponseNN() {
    uint8_t nn = response_buffer[0];
    if (nn >= MAX_DATA_LENGTH) {
      return 0;
    }
    return nn;
  }

  int16_t get_response_byte(uint8_t pos);
  uint8_t get_response_crc();

  void push_response_data(uint8_t cr);
  bool is_response_complete();
  bool is_response_valid();
  bool is_request_complete();
  bool is_request_valid();

 protected:
  uint8_t response_buffer[RESPONSE_BUFFER_SIZE] = {0};
  uint8_t response_buffer_pos = 0;
  uint8_t response_rolling_crc = 0;
};

class SendCommand : public TelegramBase {
 public:
  SendCommand();
  SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data);
  bool can_retry(int8_t max_tries);
  uint8_t get_crc();

 protected:
  uint8_t tries_count_ = 0;
};

}  // namespace ebus
}  // namespace esphome
