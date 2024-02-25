#ifndef USE_ESP8266

#include "ebus_component.h"

#include "esphome/core/helpers.h"

namespace esphome {
namespace ebus {

void EbusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EbusComponent");
  if (this->primary_address_ == SYN) {
    ESP_LOGCONFIG(TAG, "  primary_addres: N/A");
  } else {
    ESP_LOGCONFIG(TAG, "  primary_addres: 0x%02x", this->primary_address_);
  }
  ESP_LOGCONFIG(TAG, "  max_tries: %d", this->max_tries_);
  ESP_LOGCONFIG(TAG, "  max_lock_counter: %d", this->max_lock_counter_);
  ESP_LOGCONFIG(TAG, "  history_queue_size: %d", this->history_queue_size_);
  ESP_LOGCONFIG(TAG, "  command_queue_size: %d", this->command_queue_size_);
  ESP_LOGCONFIG(TAG, "  poll_interval (ms): %d", this->update_interval_);
  ESP_LOGCONFIG(TAG, "  uart:");
  ESP_LOGCONFIG(TAG, "    num: %d", this->uart_num_);
  ESP_LOGCONFIG(TAG, "    tx_pin: %d", this->uart_tx_pin_);
  ESP_LOGCONFIG(TAG, "    rx_pin: %d", this->uart_rx_pin_);
}

void EbusComponent::setup() {
  this->setup_queues_();
  this->setup_ebus_();
  this->setup_uart_();
  this->setup_tasks_();
}

void EbusComponent::set_primary_address(uint8_t primary_address) { this->primary_address_ = primary_address; }
void EbusComponent::set_max_tries(uint8_t max_tries) { this->max_tries_ = max_tries; }
void EbusComponent::set_max_lock_counter(uint8_t max_lock_counter) { this->max_lock_counter_ = max_lock_counter; }
void EbusComponent::set_uart_num(uint8_t uart_num) { this->uart_num_ = uart_num; }
void EbusComponent::set_uart_tx_pin(uint8_t uart_tx_pin) { this->uart_tx_pin_ = uart_tx_pin; }
void EbusComponent::set_uart_rx_pin(uint8_t uart_rx_pin) { this->uart_rx_pin_ = uart_rx_pin; }
void EbusComponent::set_history_queue_size(uint8_t history_queue_size) {
  this->history_queue_size_ = history_queue_size;
}
void EbusComponent::set_command_queue_size(uint8_t command_queue_size) {
  this->command_queue_size_ = command_queue_size;
}

void EbusComponent::setup_queues_() {
  this->history_queue_ = xQueueCreate(this->history_queue_size_, sizeof(Telegram));
  this->command_queue_ = xQueueCreate(this->command_queue_size_, sizeof(Telegram));
}

void EbusComponent::setup_ebus_() {
  this->ebus_ = make_unique<Ebus>();
  this->ebus_->set_primary_address(this->primary_address_);
  this->ebus_->set_max_tries(this->max_tries_);
  this->ebus_->set_max_lock_counter(this->max_lock_counter_);

  this->ebus_->set_uart_send_function(
      [&](const char *buffer, int16_t length) { return uart_write_bytes(this->uart_num_, buffer, length); });

  this->ebus_->set_queue_received_telegram_function([&](Telegram &telegram) {
    BaseType_t x_higher_priority_task_woken;
    x_higher_priority_task_woken = pdFALSE;
    xQueueSendToBackFromISR(this->history_queue_, &telegram, &x_higher_priority_task_woken);
    if (x_higher_priority_task_woken) {
      portYIELD_FROM_ISR();
    }
  });

  this->ebus_->add_send_response_handler([&](Telegram &telegram) {
    std::vector<uint8_t> reply = {};
    for (auto const &item : this->items_) {
      reply = item->reply(telegram);
      if (reply.size() != 0) {
        break;
      }
    }
    return reply;
  });

  this->ebus_->set_dequeue_command_function([&](void *const command) {
    BaseType_t x_higher_priority_task_woken = pdFALSE;
    if (xQueueReceiveFromISR(this->command_queue_, command, &x_higher_priority_task_woken)) {
      if (x_higher_priority_task_woken) {
        portYIELD_FROM_ISR();
      }
      return true;
    }
    return false;
  });
}

void EbusComponent::setup_uart_() {
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);

  uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 2,
      .source_clk = UART_SCLK_REF_TICK,
  };

  ESP_ERROR_CHECK(uart_param_config(this->uart_num_, &uart_config));

  ESP_ERROR_CHECK(
      uart_set_pin(this->uart_num_, this->uart_tx_pin_, this->uart_rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  ESP_ERROR_CHECK(uart_driver_install(this->uart_num_, 256, 0, 0, nullptr, 0));

  portEXIT_CRITICAL(&mux);
}

void EbusComponent::setup_tasks_() {
  xTaskCreate(&process_received_bytes, "ebus_process_received_bytes", 2048, (void *) this, 10, nullptr);
  xTaskCreate(&process_received_messages, "ebus_process_received_messages", 2560, (void *) this, 5, nullptr);
}

void EbusComponent::process_received_bytes(void *pv_parameter) {
  EbusComponent *instance = static_cast<EbusComponent *>(pv_parameter);

  while (true) {
    uint8_t received_byte;
    int len = uart_read_bytes(instance->uart_num_, &received_byte, 1, 20 / portTICK_PERIOD_MS);
    if (len) {
      instance->ebus_->process_received_char(received_byte);
      taskYIELD();
    }
  }
}

void EbusComponent::process_received_messages(void *pv_parameter) {
  EbusComponent *instance = static_cast<EbusComponent *>(pv_parameter);

  Telegram telegram;
  while (true) {
    if (xQueueReceive(instance->history_queue_, &telegram, pdMS_TO_TICKS(1000))) {
      instance->handle_message_(telegram);
      // TODO: this comment is kept as reference on how to debug stack overflows. Could be generalized.
      // ESP_LOGD(TAG, "Task: %s, Stack Highwater Mark: %d", pcTaskGetTaskName(NULL),
      // uxTaskGetStackHighWaterMark(NULL));
      taskYIELD();
    }
  }
}

void EbusComponent::handle_message_(Telegram &telegram) {
  if (telegram.get_state() != TelegramState::endCompleted) {
    ESP_LOGD(TAG, "Message received with invalid state: %s, QQ:%02X, ZZ:%02X, Command:%02X%02X",
             telegram.get_state_string(), telegram.get_qq(), telegram.get_zz(), telegram.get_pb(), telegram.get_sb());
    return;
  }

  for (auto const &item : this->items_) {
    item->process_received(telegram);
  }
}

void EbusComponent::update() {
  if (this->primary_address_ == SYN) {
    return;
  }
  for (auto const &item : this->items_) {
    optional<SendCommand> command = item->prepare_command();
    if (command.has_value()) {
      xQueueSendToBack(this->command_queue_, &command.value(), portMAX_DELAY);
    }
  }
}

void EbusItem::dump_config() {
  ESP_LOGCONFIG(TAG, "EbusSensor");
  ESP_LOGCONFIG(TAG, "  message:");
  ESP_LOGCONFIG(TAG, "    send_poll: %s", this->send_poll_ ? "true" : "false");
  if (this->address_ == SYN) {
    ESP_LOGCONFIG(TAG, "    address: N/A");
  } else {
    ESP_LOGCONFIG(TAG, "    address: 0x%02x", this->address_);
  }
  ESP_LOGCONFIG(TAG, "    command: 0x%04x", this->command_);
};

optional<SendCommand> EbusItem::prepare_command() {
  optional<SendCommand> command;

  if (this->send_poll_) {
    command = SendCommand(  //
        this->parent_->get_primary_address(), this->address_, this->command_, this->payload_);
  }
  return command;
}

uint32_t EbusItem::get_response_bytes(Telegram &telegram, uint8_t start, uint8_t length) {
  uint32_t result = 0;
  for (uint8_t i = 0; i < 4 && i < length; i++) {
    result = result | (telegram.get_response_byte(start + i) << (i * 8));
  }
  return result;
}

bool EbusItem::is_mine(Telegram &telegram) {
  if (this->address_ != SYN && this->address_ != telegram.get_zz()) {
    return false;
  }
  if (telegram.get_command() != this->command_) {
    return false;
  }
  for (int i = 0; i < this->payload_.size(); i++) {
    if (this->payload_[i] != telegram.get_request_byte(i)) {
      return false;
    }
  }
  return true;
}

}  // namespace ebus
}  // namespace esphome

#endif  // USE_ESP8266
