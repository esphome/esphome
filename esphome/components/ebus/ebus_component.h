#pragma once

#include <memory>

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

  void set_primary_address(uint8_t /*primary_address*/);
  void set_max_tries(uint8_t /*max_tries*/);
  void set_max_lock_counter(uint8_t /*max_lock_counter*/);
  void set_uart_num(uint8_t /*uart_num*/);
  void set_uart_tx_pin(uint8_t /*uart_tx_pin*/);
  void set_uart_rx_pin(uint8_t /*uart_rx_pin*/);
  void set_history_queue_size(uint8_t /*history_queue_size*/);
  void set_command_queue_size(uint8_t /*command_queue_size*/);

  void add_sender(EbusSender * /*sender*/);
  void add_receiver(EbusReceiver * /*receiver*/);

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

  QueueHandle_t history_queue_;
  QueueHandle_t command_queue_;

  std::list<EbusSender *> senders_;
  std::list<EbusReceiver *> receivers_;

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
