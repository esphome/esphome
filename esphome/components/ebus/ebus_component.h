#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"

#include "ebus.h"

#include <driver/uart.h>
#include <soc/uart_reg.h>

namespace esphome {
namespace ebus {

static const char *const TAG = "ebus";

class EbusReceiver {
 public:
  EbusReceiver() {}
  virtual void process_received(Telegram) = 0;
};

class EbusSender {
 public:
  EbusSender() {}
  virtual void set_primary_address(uint8_t) = 0;
  virtual optional<SendCommand> prepare_command() = 0;
};

class EbusComponent : public PollingComponent {
 public:
  EbusComponent() {}

  void dump_config() override;
  void setup() override;

  void set_primary_address(uint8_t);
  void set_max_tries(uint8_t);
  void set_max_lock_counter(uint8_t);
  void set_uart_num(uint8_t);
  void set_uart_tx_pin(uint8_t);
  void set_uart_rx_pin(uint8_t);
  void set_history_queue_size(uint8_t);
  void set_command_queue_size(uint8_t);

  void add_sender(EbusSender *);
  void add_receiver(EbusReceiver *);

  void update();

 protected:
  uint8_t primary_address_;
  uint8_t max_tries_;
  uint8_t max_lock_counter_;
  uint8_t history_queue_size_;
  uint8_t command_queue_size_;
  uint8_t uart_num_;
  uint8_t uart_tx_pin_;
  uint8_t uart_rx_pin_;

  QueueHandle_t history_queue_;
  QueueHandle_t command_queue_;

  std::list<EbusSender *> senders_;
  std::list<EbusReceiver *> receivers_;

  Ebus *ebus;

  void setup_queues();
  void setup_ebus();
  void setup_uart();
  void setup_tasks();

  static void process_received_bytes(void *);
  static void process_received_messages(void *);
  void handle_message(Telegram &);
};

}  // namespace ebus
}  // namespace esphome
