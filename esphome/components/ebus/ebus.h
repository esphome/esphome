#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <vector>

#include "telegram.h"

namespace esphome {
namespace ebus {

class Elf {
 public:
  static uint8_t crc8_calc(uint8_t data, uint8_t crc_init);
  static uint8_t crc8_array(uint8_t data[], uint8_t length);
  static bool is_primary(uint8_t address);
  static int is_primary_nibble(uint8_t nibble);
  static uint8_t get_priority_class(uint8_t address);
  static uint8_t get_sub_address(uint8_t address);
  static uint8_t to_secondary(uint8_t address);
};

class Ebus {
 public:
  void set_primary_address(uint8_t primary_address) { this->primary_address_ = primary_address; }
  void set_max_tries(uint8_t max_tries) { this->max_tries_ = max_tries; }
  void set_max_lock_counter(uint8_t max_lock_counter) { this->max_lock_counter_ = max_lock_counter; }
  void set_uart_send_function(std::function<void(const char *, int16_t)> uart_send) {
    this->uart_send_ = std::move(uart_send);
  }
  void set_queue_received_telegram_function(std::function<void(Telegram &telegram)> queue_received_telegram) {
    this->queue_received_telegram_ = std::move(queue_received_telegram);
  }
  void set_dequeue_command_function(const std::function<bool(void *const)> &dequeue_command) {
    this->dequeue_command_ = dequeue_command;
  }

  void process_received_char(uint8_t received_byte);
  void add_send_response_handler(std::function<std::vector<uint8_t>(Telegram &)> send_response_handler);

 protected:
  uint8_t primary_address_;
  uint8_t max_tries_;
  uint8_t max_lock_counter_;
  uint8_t lock_counter_ = 0;
  uint8_t char_count_since_last_syn_ = 0;
  EbusState state_ = EbusState::ARBITRATION;
  Telegram receiving_telegram_;
  SendCommand active_command_;
  std::list<std::function<std::vector<uint8_t>(Telegram &)>> send_response_handlers_;

  std::function<void(const char *, int16_t)> uart_send_;
  std::function<void(Telegram &)> queue_received_telegram_;
  std::function<bool(void *const &)> dequeue_command_;
  uint8_t uart_send_char_(uint8_t cr, bool esc, bool run_crc, uint8_t crc_init);
  void uart_send_char_(uint8_t cr, bool esc = true);
  void uart_send_remaining_request_part_(SendCommand &command);
  void handle_response_(Telegram &telegram);
};

}  // namespace ebus
}  // namespace esphome
