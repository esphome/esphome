#include "micronova.h"
#include "esphome/core/log.h"

namespace esphome::micronova {

static const int STOVE_REPLY_DELAY = 60;
static const uint8_t WRITE_BIT = 1 << 7;  // 0x80

void MicroNovaBaseListener::dump_base_config() {
  ESP_LOGCONFIG(TAG,
                "  Memory Location: %02X\n"
                "  Memory Address: %02X",
                this->memory_location_, this->memory_address_);
}

void MicroNovaListener::dump_base_config() {
  MicroNovaBaseListener::dump_base_config();
  LOG_UPDATE_INTERVAL(this);
}

void MicroNovaListener::request_value_from_stove_() {
  this->micronova_->queue_read_request(this->memory_location_, this->memory_address_);
}

void MicroNova::setup() {
  if (this->enable_rx_pin_ != nullptr) {
    this->enable_rx_pin_->setup();
    this->enable_rx_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->enable_rx_pin_->digital_write(false);
  }
}

void MicroNova::dump_config() {
  ESP_LOGCONFIG(TAG, "MicroNova:");
  if (this->enable_rx_pin_ != nullptr) {
    LOG_PIN("  Enable RX Pin: ", this->enable_rx_pin_);
  }
}

void MicroNova::register_micronova_listener(MicroNovaListener *listener) {
  MicroNovaAddress addr = {listener->get_memory_location(), listener->get_memory_address()};
  this->listeners_[addr].push_back(listener);
  // Request initial value
  this->queue_read_request(addr.memory_location, addr.memory_address);
}

void MicroNova::request_update_listeners_() {
  ESP_LOGD(TAG, "Requesting update from all listeners");
  for (auto &entry : this->listeners_) {
    this->queue_read_request(entry.first.memory_location, entry.first.memory_address);
  }
}

void MicroNova::loop() {
  // Check if we're processing a command and waiting for reply
  if (this->current_command_.has_value()) {
    if (millis() - this->current_command_->transmission_time > STOVE_REPLY_DELAY) {
      int stove_reply_value = this->read_stove_reply_();
      // For READ commands, notify all listeners registered for this address
      if (this->current_command_->type == MicroNovaCommandType::READ) {
        MicroNovaAddress addr = {this->current_command_->memory_location, this->current_command_->memory_address};
        auto it = this->listeners_.find(addr);
        if (it != this->listeners_.end()) {
          ESP_LOGV(TAG, "Found %zu listeners for [%02X,%02X], dispatching value %d", it->second.size(),
                   addr.memory_location, addr.memory_address, stove_reply_value);
          for (auto *listener : it->second) {
            listener->process_value_from_stove(stove_reply_value);
          }
        } else {
          ESP_LOGW(TAG, "No listeners found for [%02X,%02X]", addr.memory_location, addr.memory_address);
        }
      }
      this->current_command_.reset();
    }
    return;
  }

  // No reply pending - process next command from queue
  if (!this->command_queue_.empty()) {
    this->current_command_ = this->command_queue_.front();
    this->command_queue_.pop_front();
    this->send_current_command_();
  }
}

void MicroNova::queue_read_request(uint8_t location, uint8_t address) {
  MicroNovaCommand cmd;
  cmd.type = MicroNovaCommandType::READ;
  cmd.memory_location = location;
  cmd.memory_address = address;
  cmd.data = 0;

  // Check if this read is already queued
  for (const auto &queued : this->command_queue_) {
    if (queued == cmd) {
      ESP_LOGV(TAG, "Read [%02X,%02X] already queued, ignoring", location, address);
      return;
    }
  }

  this->command_queue_.push_back(cmd);
  ESP_LOGV(TAG, "Queued read [%02X,%02X] at back (queue size: %zu)", location, address, this->command_queue_.size());
}

void MicroNova::send_current_command_() {
  if (!this->current_command_.has_value()) {
    return;
  }

  uint8_t trash_rx;

  // Clear rx buffer - stove hiccups may cause late replies in the rx
  while (this->available()) {
    this->read_byte(&trash_rx);
    ESP_LOGW(TAG, "Reading excess byte 0x%02X", trash_rx);
  }

  uint8_t write_data[4] = {this->current_command_->memory_location, this->current_command_->memory_address, 0, 0};
  size_t write_len;

  if (this->current_command_->type == MicroNovaCommandType::READ) {
    write_len = 2;
    ESP_LOGV(TAG, "Request from stove [%02X,%02X]", write_data[0], write_data[1]);
  } else {
    write_len = 4;
    write_data[2] = this->current_command_->data;
    // calculate checksum
    write_data[3] = ((uint16_t) write_data[0] + (uint16_t) write_data[1] + (uint16_t) write_data[2]) & 0xFF;
    ESP_LOGV(TAG, "Write 4 bytes [%02X,%02X,%02X,%02X]", write_data[0], write_data[1], write_data[2], write_data[3]);
  }

  this->enable_rx_pin_->digital_write(true);
  this->write_array(write_data, write_len);
  this->flush();
  this->enable_rx_pin_->digital_write(false);

  this->current_command_->transmission_time = millis();
}

int MicroNova::read_stove_reply_() {
  if (!this->current_command_.has_value()) {
    return -1;
  }

  uint8_t reply_data[2] = {0, 0};

  this->read_array(reply_data, 2);

  ESP_LOGV(TAG, "Reply from stove [%02X,%02X]", reply_data[0], reply_data[1]);

  uint8_t checksum = ((uint16_t) this->current_command_->memory_location +
                      (uint16_t) this->current_command_->memory_address + (uint16_t) reply_data[1]) &
                     0xFF;
  if (reply_data[0] != checksum) {
    ESP_LOGE(TAG, "Checksum mismatch! From [0x%02X:0x%02X] received [0x%02X,0x%02X]. Expected 0x%02X, got 0x%02X",
             this->current_command_->memory_location, this->current_command_->memory_address, reply_data[0],
             reply_data[1], checksum, reply_data[0]);
    return -1;
  }
  return ((int) reply_data[1]);
}

void MicroNova::queue_write_command(uint8_t location, uint8_t address, uint8_t data) {
  MicroNovaCommand cmd;
  cmd.type = MicroNovaCommandType::WRITE;
  cmd.memory_location = location | WRITE_BIT;
  cmd.memory_address = address;
  cmd.data = data;

  this->command_queue_.push_front(cmd);
  ESP_LOGD(TAG, "Queued write [%02X,%02X] at front (queue size: %zu)", location, address, this->command_queue_.size());
  // Automatically queue sensor updates after write commands
  this->request_update_listeners_();
}

}  // namespace esphome::micronova
