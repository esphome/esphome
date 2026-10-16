#include "one_wire_slave.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_err.h>
#include <esp_mac.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <hal/gpio_hal.h>
#include <soc/gpio_periph.h>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::one_wire_slave {

static const char *const TAG = "one_wire_slave";
static const gpio_hal_context_t GPIO_HAL = {.dev = GPIO_HAL_GET_HW(GPIO_PORT_0)};

static constexpr uint8_t ROM_FAMILY_DS18B20 = 0x28;
static constexpr uint8_t ROM_SEARCH = 0xF0;
static constexpr uint8_t ROM_READ = 0x33;
static constexpr uint8_t ROM_MATCH = 0x55;
static constexpr uint8_t ROM_SKIP = 0xCC;
static constexpr uint8_t CMD_CONVERT_T = 0x44;
static constexpr uint8_t CMD_WRITE_SCRATCHPAD = 0x4E;
static constexpr uint8_t CMD_READ_SCRATCHPAD = 0xBE;
static constexpr uint8_t CMD_COPY_SCRATCHPAD = 0x48;
static constexpr uint8_t CMD_READ_POWER_SUPPLY = 0xB4;

static constexpr uint32_t RESET_MIN_US = 400;
static constexpr uint32_t RESET_TIMEOUT_US = 2000;
static constexpr uint32_t SLOT_TIMEOUT_US = 5000;
static constexpr uint32_t SAMPLE_US = 15;
static constexpr uint32_t SEND_ASSERT_US = 2;
static constexpr uint32_t SLOT_US = 60;
static constexpr uint32_t TX_RELEASE_US = 55;

void OneWireSlaveComponent::setup() {
  if (this->pin_ == nullptr) {
    ESP_LOGE(TAG, "No data pin configured");
    this->mark_failed();
    return;
  }

  this->gpio_num_ = static_cast<gpio_num_t>(this->pin_->get_pin());
  this->isr_pin_ = this->pin_->to_isr();
  if (this->gpio_num_ < 0 || this->gpio_num_ >= GPIO_NUM_MAX) {
    ESP_LOGE(TAG, "Invalid data pin");
    this->mark_failed();
    return;
  }

  gpio_config_t gpio_settings{};
  gpio_settings.pin_bit_mask = 1ULL << this->gpio_num_;
  gpio_settings.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  gpio_settings.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_settings.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_settings.intr_type = GPIO_INTR_NEGEDGE;
  if (gpio_config(&gpio_settings) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure GPIO%u as open-drain", this->pin_->get_pin());
    this->mark_failed();
    return;
  }
  this->release_bus_();

  this->configure_rom_();
  this->configure_scratchpad_();

  esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  err = gpio_isr_handler_add(this->gpio_num_, &OneWireSlaveComponent::gpio_isr_, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to attach GPIO ISR: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->stopping_ = false;
  if (this->interrupt_mode_) {
    this->isr_state_ = IsrState::RX_ROM_COMMAND;
    return;
  }
  BaseType_t task_result = xTaskCreatePinnedToCore(
      &OneWireSlaveComponent::task_entry_, "one_wire_slave", 4096, this, 22, &this->task_handle_, 1);
  if (task_result != pdPASS || this->task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create 1-Wire slave timing task");
    gpio_isr_handler_remove(this->gpio_num_);
    this->mark_failed();
    return;
  }
}

void OneWireSlaveComponent::loop() {
  const uint32_t now = millis();
  if (now - this->last_stats_ms_ < 1000)
    return;
  this->last_stats_ms_ = now;
  ESP_LOGI(TAG, "stats scale=%u resets=%lu transactions=%lu rom=%lu function=%lu send0=%lu send1=%lu errors=%lu last=%u",
           this->timing_scale_, static_cast<unsigned long>(this->reset_count_),
           static_cast<unsigned long>(this->transaction_count_), static_cast<unsigned long>(this->rom_command_count_),
           static_cast<unsigned long>(this->function_command_count_), static_cast<unsigned long>(this->send_zero_count_),
           static_cast<unsigned long>(this->send_one_count_), static_cast<unsigned long>(this->slot_error_count_),
           this->last_error_status_);
}

void OneWireSlaveComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "1-Wire slave:");
  LOG_PIN("  Pin: ", this->pin_);
  char rom_buffer[17];
  ESP_LOGCONFIG(TAG, "  ROM: 0x%s", format_hex_to(rom_buffer, this->address_));
  ESP_LOGCONFIG(TAG, "  Family: DS18B20 (0x28)");
  ESP_LOGCONFIG(TAG, "  Temperature: %.3f °C", this->temperature_);
  ESP_LOGCONFIG(TAG, "  Power mode: external");
  ESP_LOGCONFIG(TAG, "  Backend: %s", this->interrupt_mode_ ? "GPIO ISR" : "timing task");
}

void OneWireSlaveComponent::on_shutdown() {
  this->stopping_ = true;
  if (this->task_handle_ != nullptr) {
    xTaskNotifyGive(this->task_handle_);
  }
  if (this->gpio_num_ != GPIO_NUM_NC) {
    gpio_isr_handler_remove(this->gpio_num_);
    this->release_bus_();
    gpio_set_direction(this->gpio_num_, GPIO_MODE_INPUT);
  }
}

void IRAM_ATTR OneWireSlaveComponent::gpio_isr_(void *arg) {
  auto *self = static_cast<OneWireSlaveComponent *>(arg);
  if (self->interrupt_mode_) {
    self->handle_isr_edge_();
    return;
  }
  if (self->task_handle_ == nullptr) {
    return;
  }
  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(self->task_handle_, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void IRAM_ATTR OneWireSlaveComponent::handle_isr_edge_() {
  if (this->stopping_ || this->isr_drive_active_)
    return;
  if (this->isr_ignore_edge_) {
    if (this->isr_pin_.digital_read()) {
      this->isr_ignore_edge_ = false;
      return;
    }
    this->isr_ignore_edge_ = false;
  }

  switch (this->isr_state_) {
    case IsrState::SEARCH_ID:
    case IsrState::SEARCH_COMPLEMENT:
    case IsrState::TX_BYTES:
    case IsrState::TX_POWER:
      this->isr_transmit_bit_();
      break;
    case IsrState::RX_ROM_COMMAND:
    case IsrState::SEARCH_BRANCH:
    case IsrState::RX_MATCH_ROM:
    case IsrState::RX_FUNCTION:
    case IsrState::RX_WRITE_SCRATCHPAD:
      esp_rom_delay_us(this->scale_(SAMPLE_US));
      this->isr_receive_bit_(this->isr_pin_.digital_read());
      break;
    case IsrState::IGNORE:
      break;
  }

  const uint32_t low_start = micros();
  while (!this->isr_pin_.digital_read() && micros() - low_start < this->scale_(RESET_TIMEOUT_US)) {
  }
  if (micros() - low_start >= this->scale_(RESET_MIN_US)) {
    this->isr_reset_();
  }
}

void IRAM_ATTR OneWireSlaveComponent::isr_reset_() {
  this->reset_count_++;
  this->transaction_count_++;
  this->isr_state_ = IsrState::RX_ROM_COMMAND;
  this->isr_bit_index_ = 0;
  this->isr_byte_index_ = 0;
  this->isr_rx_byte_ = 0;
  this->isr_tx_length_ = 0;
  this->isr_drive_active_ = false;
  this->isr_ignore_edge_ = false;
  this->isr_pin_.digital_write(true);
  esp_rom_delay_us(this->scale_(30));
  this->isr_ignore_edge_ = true;
  this->isr_drive_active_ = true;
  this->isr_pin_.digital_write(false);
  esp_rom_delay_us(this->scale_(120));
  this->isr_pin_.digital_write(true);
  gpio_hal_clear_intr_status_bit(&GPIO_HAL, this->gpio_num_);
  this->isr_drive_active_ = false;
}

void IRAM_ATTR OneWireSlaveComponent::isr_receive_bit_(bool bit) {
  if (this->isr_state_ == IsrState::SEARCH_BRANCH) {
    const bool expected = (this->rom_[this->isr_search_bit_ / 8] & (1U << (this->isr_search_bit_ % 8))) != 0;
    if (bit != expected) {
      this->isr_state_ = IsrState::IGNORE;
      return;
    }
    this->isr_search_bit_++;
    if (this->isr_search_bit_ >= 64) {
      this->isr_state_ = IsrState::RX_ROM_COMMAND;
    } else {
      this->isr_state_ = IsrState::SEARCH_ID;
    }
    return;
  }

  if (bit)
    this->isr_rx_byte_ |= static_cast<uint8_t>(1U << this->isr_bit_index_);
  this->isr_bit_index_++;
  if (this->isr_bit_index_ == 8) {
    const uint8_t value = this->isr_rx_byte_;
    this->isr_rx_byte_ = 0;
    this->isr_bit_index_ = 0;
    this->isr_complete_byte_(value);
  }
}

void IRAM_ATTR OneWireSlaveComponent::isr_complete_byte_(uint8_t value) {
  switch (this->isr_state_) {
    case IsrState::RX_ROM_COMMAND:
      this->rom_command_count_++;
      switch (value) {
        case ROM_SEARCH:
          this->isr_search_bit_ = 0;
          this->isr_state_ = IsrState::SEARCH_ID;
          break;
        case ROM_READ:
          for (uint8_t i = 0; i < 8; i++)
            this->isr_tx_buffer_[i] = this->rom_[i];
          this->isr_tx_length_ = 8;
          this->isr_byte_index_ = 0;
          this->isr_bit_index_ = 0;
          this->isr_after_tx_state_ = IsrState::RX_ROM_COMMAND;
          this->isr_state_ = IsrState::TX_BYTES;
          break;
        case ROM_MATCH:
          this->isr_byte_index_ = 0;
          this->isr_state_ = IsrState::RX_MATCH_ROM;
          break;
        case ROM_SKIP:
          this->isr_state_ = IsrState::RX_FUNCTION;
          break;
        default:
          this->isr_state_ = IsrState::IGNORE;
          break;
      }
      break;

    case IsrState::RX_MATCH_ROM:
      this->isr_rx_buffer_[this->isr_byte_index_++] = value;
      if (this->isr_byte_index_ == 8) {
        bool match = true;
        for (uint8_t i = 0; i < 8; i++)
          match &= this->isr_rx_buffer_[i] == this->rom_[i];
        this->isr_state_ = match ? IsrState::RX_FUNCTION : IsrState::IGNORE;
        this->isr_byte_index_ = 0;
      }
      break;

    case IsrState::RX_FUNCTION:
      this->function_command_count_++;
      switch (value) {
        case CMD_CONVERT_T:
          this->scratchpad_[0] = static_cast<uint8_t>(this->temperature_raw_ & 0xFF);
          this->scratchpad_[1] = static_cast<uint8_t>((this->temperature_raw_ >> 8) & 0xFF);
          this->scratchpad_[8] = crc8_(this->scratchpad_, 8);
          this->isr_state_ = IsrState::RX_ROM_COMMAND;
          break;
        case CMD_READ_SCRATCHPAD:
          for (uint8_t i = 0; i < 9; i++)
            this->isr_tx_buffer_[i] = this->scratchpad_[i];
          this->isr_tx_length_ = 9;
          this->isr_byte_index_ = 0;
          this->isr_bit_index_ = 0;
          this->isr_after_tx_state_ = IsrState::RX_ROM_COMMAND;
          this->isr_state_ = IsrState::TX_BYTES;
          break;
        case CMD_WRITE_SCRATCHPAD:
          this->isr_byte_index_ = 0;
          this->isr_state_ = IsrState::RX_WRITE_SCRATCHPAD;
          break;
        case CMD_READ_POWER_SUPPLY:
          this->isr_tx_buffer_[0] = 0xFF;
          this->isr_tx_length_ = 1;
          this->isr_byte_index_ = 0;
          this->isr_bit_index_ = 0;
          this->isr_after_tx_state_ = IsrState::RX_ROM_COMMAND;
          this->isr_state_ = IsrState::TX_POWER;
          break;
        default:
          this->isr_state_ = IsrState::RX_ROM_COMMAND;
          break;
      }
      break;

    case IsrState::RX_WRITE_SCRATCHPAD:
      this->isr_rx_buffer_[this->isr_byte_index_++] = value;
      if (this->isr_byte_index_ == 3) {
        this->scratchpad_[2] = this->isr_rx_buffer_[0];
        this->scratchpad_[3] = this->isr_rx_buffer_[1];
        this->scratchpad_[4] = this->isr_rx_buffer_[2];
        this->scratchpad_[8] = crc8_(this->scratchpad_, 8);
        this->isr_byte_index_ = 0;
        this->isr_state_ = IsrState::RX_FUNCTION;
      }
      break;

    default:
      break;
  }
}

void IRAM_ATTR OneWireSlaveComponent::isr_transmit_bit_() {
  bool bit = true;
  if (this->isr_state_ == IsrState::SEARCH_ID || this->isr_state_ == IsrState::SEARCH_COMPLEMENT) {
    const bool value = (this->rom_[this->isr_search_bit_ / 8] & (1U << (this->isr_search_bit_ % 8))) != 0;
    bit = this->isr_state_ == IsrState::SEARCH_ID ? value : !value;
  } else {
    bit = (this->isr_tx_buffer_[this->isr_byte_index_] & (1U << this->isr_bit_index_)) != 0;
  }

  if (bit)
    this->send_one_count_++;
  else
    this->send_zero_count_++;
  esp_rom_delay_us(this->scale_(SEND_ASSERT_US));
  if (!bit) {
    this->isr_ignore_edge_ = true;
    this->isr_drive_active_ = true;
    this->isr_pin_.digital_write(false);
  }
  esp_rom_delay_us(this->scale_(TX_RELEASE_US - SEND_ASSERT_US));
  this->isr_pin_.digital_write(true);
  gpio_hal_clear_intr_status_bit(&GPIO_HAL, this->gpio_num_);
  if (!bit)
    this->isr_drive_active_ = false;

  if (this->isr_state_ == IsrState::SEARCH_ID) {
    this->isr_state_ = IsrState::SEARCH_COMPLEMENT;
  } else if (this->isr_state_ == IsrState::SEARCH_COMPLEMENT) {
    this->isr_state_ = IsrState::SEARCH_BRANCH;
  } else {
    this->isr_bit_index_++;
    if (this->isr_bit_index_ == 8) {
      this->isr_bit_index_ = 0;
      this->isr_byte_index_++;
      if (this->isr_byte_index_ == this->isr_tx_length_) {
        this->isr_state_ = this->isr_after_tx_state_;
        this->isr_byte_index_ = 0;
      }
    }
  }
}

void OneWireSlaveComponent::task_entry_(void *arg) {
  auto *self = static_cast<OneWireSlaveComponent *>(arg);
  self->task_loop_();
  self->task_handle_ = nullptr;
  vTaskDelete(nullptr);
}

void OneWireSlaveComponent::task_loop_() {
  while (!this->stopping_) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (this->stopping_) {
      break;
    }
    if (!this->handle_reset_()) {
      continue;
    }
    this->reset_count_++;
    this->transaction_count_++;
    this->serve_transaction_();
    this->drain_notifications_();
  }
}

void OneWireSlaveComponent::drain_notifications_() {
  while (ulTaskNotifyTake(pdTRUE, 0) != 0) {
  }
}

void OneWireSlaveComponent::configure_rom_() {
  if (this->address_ == 0) {
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
      ESP_LOGE(TAG, "Unable to read ESP32 station MAC for generated ROM");
      this->mark_failed();
      return;
    }
    this->rom_[0] = ROM_FAMILY_DS18B20;
    std::memcpy(&this->rom_[1], mac, sizeof(mac));
    this->rom_[7] = crc8_(this->rom_, 7);
  } else {
    for (uint8_t i = 0; i < sizeof(this->rom_); i++) {
      this->rom_[i] = static_cast<uint8_t>((this->address_ >> (i * 8)) & 0xFF);
    }
  }

  if (this->rom_[0] != ROM_FAMILY_DS18B20 || crc8_(this->rom_, 7) != this->rom_[7]) {
    ESP_LOGE(TAG, "Invalid DS18B20 ROM address");
    this->mark_failed();
    return;
  }

  this->address_ = 0;
  for (uint8_t i = 0; i < sizeof(this->rom_); i++) {
    this->address_ |= static_cast<uint64_t>(this->rom_[i]) << (i * 8);
  }
}

void OneWireSlaveComponent::configure_scratchpad_() {
  this->temperature_raw_ = static_cast<int16_t>(std::lroundf(std::clamp(this->temperature_, -55.0f, 125.0f) * 16.0f));
  this->scratchpad_[0] = static_cast<uint8_t>(this->temperature_raw_ & 0xFF);
  this->scratchpad_[1] = static_cast<uint8_t>((this->temperature_raw_ >> 8) & 0xFF);
  this->scratchpad_[2] = 75;
  this->scratchpad_[3] = 70;
  this->scratchpad_[4] = 0x7F;
  this->scratchpad_[5] = 0xFF;
  this->scratchpad_[6] = 0x0C;
  this->scratchpad_[7] = 0x10;
  this->resolution_ = 12;
  this->update_scratchpad_crc_();
}

void OneWireSlaveComponent::update_scratchpad_crc_() {
  this->scratchpad_[8] = crc8_(this->scratchpad_, 8);
}

uint8_t IRAM_ATTR OneWireSlaveComponent::crc8_(const uint8_t *data, uint8_t length) {
  uint8_t crc = 0;
  while (length-- != 0) {
    uint8_t value = *data++;
    for (uint8_t bit = 0; bit < 8; bit++) {
      const uint8_t mix = (crc ^ value) & 1;
      crc >>= 1;
      if (mix != 0) {
        crc ^= 0x8C;
      }
      value >>= 1;
    }
  }
  return crc;
}

void OneWireSlaveComponent::release_bus_() { gpio_set_level(this->gpio_num_, 1); }

void OneWireSlaveComponent::drive_bus_low_() { gpio_set_level(this->gpio_num_, 0); }

bool OneWireSlaveComponent::wait_for_low_(uint32_t timeout_us) {
  const int64_t deadline = esp_timer_get_time() + timeout_us;
  while (gpio_get_level(this->gpio_num_) != 0) {
    if (esp_timer_get_time() >= deadline) {
      return false;
    }
  }
  return true;
}

bool OneWireSlaveComponent::wait_for_high_(uint32_t timeout_us) {
  const int64_t deadline = esp_timer_get_time() + timeout_us;
  while (gpio_get_level(this->gpio_num_) == 0) {
    if (esp_timer_get_time() >= deadline) {
      return false;
    }
  }
  return true;
}

bool OneWireSlaveComponent::handle_reset_() {
  if (!this->wait_for_low_(this->scale_(RESET_TIMEOUT_US))) {
    return false;
  }
  const int64_t low_start = esp_timer_get_time();
  if (!this->wait_for_high_(this->scale_(RESET_TIMEOUT_US))) {
    this->release_bus_();
    return false;
  }
  const int64_t low_duration = esp_timer_get_time() - low_start;
  if (low_duration < this->scale_(RESET_MIN_US)) {
    return false;
  }

  esp_rom_delay_us(this->scale_(30));
  this->drive_bus_low_();
  esp_rom_delay_us(this->scale_(120));
  this->release_bus_();
  return true;
}

OneWireSlaveComponent::SlotResult OneWireSlaveComponent::receive_bit_(bool *value) {
  if (!this->wait_for_low_(this->scale_(SLOT_TIMEOUT_US))) {
    this->slot_error_count_++;
    this->last_error_status_ = 1;
    return SlotResult::TIMEOUT;
  }
  const int64_t low_start = esp_timer_get_time();
  esp_rom_delay_us(this->scale_(SAMPLE_US));
  *value = gpio_get_level(this->gpio_num_) != 0;
  if (!this->wait_for_high_(this->scale_(SLOT_TIMEOUT_US))) {
    this->release_bus_();
    this->slot_error_count_++;
    this->last_error_status_ = 2;
    return SlotResult::RESET;
  }
  if (esp_timer_get_time() - low_start >= this->scale_(RESET_MIN_US)) {
    this->last_error_status_ = 3;
    return SlotResult::RESET;
  }
  return SlotResult::OK;
}

OneWireSlaveComponent::SlotResult OneWireSlaveComponent::send_bit_(bool value) {
  if (!this->wait_for_low_(this->scale_(SLOT_TIMEOUT_US))) {
    this->slot_error_count_++;
    this->last_error_status_ = 1;
    return SlotResult::TIMEOUT;
  }
  const int64_t slot_start = esp_timer_get_time();
  if (value) {
    this->send_one_count_++;
  } else {
    this->send_zero_count_++;
  }
  esp_rom_delay_us(this->scale_(SEND_ASSERT_US));
  if (!value) {
    this->drive_bus_low_();
  }
  while (esp_timer_get_time() - slot_start < this->scale_(SLOT_US)) {
  }
  this->release_bus_();
  return SlotResult::OK;
}

OneWireSlaveComponent::SlotResult OneWireSlaveComponent::receive_byte_(uint8_t *value) {
  uint8_t result = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    bool bit_value = false;
    const SlotResult status = this->receive_bit_(&bit_value);
    if (status != SlotResult::OK) {
      return status;
    }
    if (bit_value) {
      result |= static_cast<uint8_t>(1U << bit);
    }
  }
  *value = result;
  return SlotResult::OK;
}

OneWireSlaveComponent::SlotResult OneWireSlaveComponent::send_byte_(uint8_t value) {
  for (uint8_t bit = 0; bit < 8; bit++) {
    const SlotResult status = this->send_bit_((value & (1U << bit)) != 0);
    if (status != SlotResult::OK) {
      return status;
    }
  }
  return SlotResult::OK;
}

OneWireSlaveComponent::SlotResult OneWireSlaveComponent::receive_bytes_(uint8_t *data, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    const SlotResult status = this->receive_byte_(&data[i]);
    if (status != SlotResult::OK) {
      return status;
    }
  }
  return SlotResult::OK;
}

OneWireSlaveComponent::SlotResult OneWireSlaveComponent::send_bytes_(const uint8_t *data, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    const SlotResult status = this->send_byte_(data[i]);
    if (status != SlotResult::OK) {
      return status;
    }
  }
  return SlotResult::OK;
}

void OneWireSlaveComponent::serve_transaction_() {
  uint8_t rom_command = 0;
  if (this->receive_byte_(&rom_command) != SlotResult::OK) {
    return;
  }
  this->rom_command_count_++;

  switch (rom_command) {
    case ROM_SEARCH: {
      for (uint8_t bit = 0; bit < 64; bit++) {
        const bool rom_bit = (this->rom_[bit / 8] & (1U << (bit % 8))) != 0;
        if (this->send_bit_(rom_bit) != SlotResult::OK || this->send_bit_(!rom_bit) != SlotResult::OK) {
          return;
        }
        bool selected_bit = false;
        if (this->receive_bit_(&selected_bit) != SlotResult::OK || selected_bit != rom_bit) {
          return;
        }
      }
      break;
    }
    case ROM_READ:
      this->send_bytes_(this->rom_, sizeof(this->rom_));
      return;
    case ROM_MATCH: {
      uint8_t address[sizeof(this->rom_)] = {0};
      if (this->receive_bytes_(address, sizeof(address)) != SlotResult::OK ||
          std::memcmp(address, this->rom_, sizeof(this->rom_)) != 0) {
        return;
      }
      break;
    }
    case ROM_SKIP:
      break;
    default:
      return;
  }

  uint8_t command = 0;
  if (this->receive_byte_(&command) != SlotResult::OK) {
    return;
  }
  this->function_command_count_++;

  switch (command) {
    case CMD_CONVERT_T: {
      int16_t raw = static_cast<int16_t>(std::lroundf(std::clamp(this->temperature_, -55.0f, 125.0f) * 16.0f));
      const uint8_t shift = 12 - this->resolution_;
      raw = static_cast<int16_t>(static_cast<uint16_t>(raw) & static_cast<uint16_t>(~((1U << shift) - 1U)));
      this->scratchpad_[0] = static_cast<uint8_t>(raw & 0xFF);
      this->scratchpad_[1] = static_cast<uint8_t>((raw >> 8) & 0xFF);
      this->update_scratchpad_crc_();
      return;
    }
    case CMD_READ_SCRATCHPAD:
      this->send_bytes_(this->scratchpad_, sizeof(this->scratchpad_));
      return;
    case CMD_WRITE_SCRATCHPAD: {
      uint8_t values[3] = {0};
      if (this->receive_bytes_(values, sizeof(values)) != SlotResult::OK) {
        return;
      }
      this->scratchpad_[2] = values[0];
      this->scratchpad_[3] = values[1];
      this->scratchpad_[4] = values[2];
      switch (this->scratchpad_[4] & 0x60) {
        case 0x00:
          this->resolution_ = 9;
          break;
        case 0x20:
          this->resolution_ = 10;
          break;
        case 0x40:
          this->resolution_ = 11;
          break;
        default:
          this->resolution_ = 12;
          break;
      }
      this->update_scratchpad_crc_();
      return;
    }
    case CMD_COPY_SCRATCHPAD:
      return;
    case CMD_READ_POWER_SUPPLY:
      this->send_bit_(true);
      return;
    default:
      return;
  }
}

}  // namespace esphome::one_wire_slave

#endif  // USE_ESP32
