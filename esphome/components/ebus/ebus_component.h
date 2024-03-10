#pragma once

#include <memory>
#include <vector>

#include "esphome/core/log.h"
#include "esphome/core/component.h"

#include "ebus.h"

#ifndef USE_ESP8266
#include <driver/uart.h>
#include <soc/uart_reg.h>
#endif

namespace esphome {
namespace ebus {

static const char *const TAG = "ebus";

class EbusComponent;

class EbusItem : public Component {
 public:
  void dump_config() override;

  void set_parent(EbusComponent *parent) { this->parent_ = parent; }
  void set_address(uint8_t address) { this->address_ = Elf::to_secondary(address); }
  void set_send_poll(bool send_poll) { this->send_poll_ = send_poll; }
  void set_command(uint16_t command) { this->command_ = command; }
  void set_payload(const std::vector<uint8_t> &payload) { this->payload_ = payload; }
  void set_response_read_position(uint8_t response_position) { this->response_position_ = response_position; }

  virtual void process_received(Telegram) {}
  virtual std::vector<uint8_t> reply(Telegram telegram) {
    std::vector<uint8_t> reply = {0xe3, 'E', 'S', 'P', 'H', 'M', 0x12, 0x34, 0x56, 0x78};
    return reply;
  };
  virtual optional<SendCommand> prepare_command();

  // TODO: refactor these
  uint32_t get_response_bytes(Telegram &telegram, uint8_t start, uint8_t length);
  bool is_mine(Telegram &telegram);

 protected:
  EbusComponent *parent_;
  uint8_t address_ = SYN;
  bool send_poll_;
  uint16_t command_;
  std::vector<uint8_t> payload_{};
  uint8_t response_position_;
};

class EbusComponent : public PollingComponent {
 public:
  EbusComponent() {}

  void dump_config() override;
  void setup() override;

  void set_primary_address(uint8_t /*primary_address*/);
  uint8_t get_primary_address() { return this->primary_address_; }
  void set_max_tries(uint8_t /*max_tries*/);
  void set_max_lock_counter(uint8_t /*max_lock_counter*/);
  void set_uart_num(uint8_t /*uart_num*/);
  void set_uart_tx_pin(uint8_t /*uart_tx_pin*/);
  void set_uart_rx_pin(uint8_t /*uart_rx_pin*/);
  void set_history_queue_size(uint8_t /*history_queue_size*/);
  void set_command_queue_size(uint8_t /*command_queue_size*/);

  void add_item(EbusItem *item) { this->items_.push_back(item); };

  void update() override;

 protected:
  uint8_t primary_address_;
  uint8_t max_tries_;
  uint8_t max_lock_counter_;
  uint8_t history_queue_size_;
  uint8_t command_queue_size_;
  uint8_t uart_num_;
  uint8_t uart_tx_pin_;
  uint8_t uart_rx_pin_;

#ifndef USE_ESP8266
  QueueHandle_t history_queue_;
  QueueHandle_t command_queue_;
#endif

  std::list<EbusItem *> items_;

  std::unique_ptr<Ebus> ebus_;

  void setup_queues_();
  void setup_ebus_();
  void setup_uart_();
  void setup_tasks_();

  static void process_received_bytes(void * /*pvParameter*/);
  static void process_received_messages(void * /*pvParameter*/);
  void handle_message_(Telegram & /*telegram*/);
};

}  // namespace ebus
}  // namespace esphome
