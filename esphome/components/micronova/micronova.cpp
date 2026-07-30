#include "micronova.h"
#include "esphome/core/log.h"

namespace esphome::micronova {

static const char *const TAG = "micronova";
static constexpr uint8_t STOVE_REPLY_SIZE = 2;
static constexpr uint32_t STOVE_REPLY_TIMEOUT = 200;  // ms
static constexpr uint8_t WRITE_BIT = 1 << 7;          // 0x80

bool MicroNovaCommand::is_write() const { return this->memory_location & WRITE_BIT; }

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
  this->enable_rx_pin_->setup();
  this->enable_rx_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->enable_rx_pin_->digital_write(false);
}

void MicroNova::dump_config() {
  ESP_LOGCONFIG(TAG, "MicroNova:");
  LOG_PIN("  Enable RX Pin: ", this->enable_rx_pin_);
}

#ifdef MICRONOVA_LISTENER_COUNT
void MicroNova::register_micronova_listener(MicroNovaListener *listener) {
  this->listeners_.push_back(listener);
  // Request initial value
  this->queue_read_request(listener->get_memory_location(), listener->get_memory_address());
}

void MicroNova::request_update_listeners_() {
  ESP_LOGD(TAG, "Requesting update from all listeners");
  for (auto *listener : this->listeners_) {
    this->queue_read_request(listener->get_memory_location(), listener->get_memory_address());
  }
}
#endif

void MicroNova::loop() {
  // Check if we're processing a command and waiting for reply
  if (this->reply_pending_) {
    // Check if all reply bytes have arrived
    if (this->available() >= STOVE_REPLY_SIZE) {
#ifdef MICRONOVA_LISTENER_COUNT
      int stove_reply_value = this->read_stove_reply_();
      if (this->current_command_.is_write()) {
        if (stove_reply_value == -1) {
          ESP_LOGW(TAG, "Write to [0x%02X:0x%02X] may have failed (checksum mismatch in reply)",
                   this->current_command_.memory_location & ~WRITE_BIT, this->current_command_.memory_address);
        }
      } else {
        // For READ commands, notify all listeners registered for this address
        uint8_t loc = this->current_command_.memory_location;
        uint8_t addr = this->current_command_.memory_address;
        for (auto *listener : this->listeners_) {
          if (listener->get_memory_location() == loc && listener->get_memory_address() == addr) {
            listener->process_value_from_stove(stove_reply_value);
          }
        }
      }
#else
      this->read_stove_reply_();
#endif
      this->reply_pending_ = false;
    } else if (millis() - this->transmission_time_ > STOVE_REPLY_TIMEOUT) {
      // Timeout - no reply received (buffer cleared before next command)
      ESP_LOGW(TAG, "Timeout waiting for reply from [0x%02X:0x%02X], available: %d",
               this->current_command_.memory_location, this->current_command_.memory_address, this->available());
      this->reply_pending_ = false;
    }
    return;
  }

  // No reply pending - process next command (writes have priority over reads)
#ifdef USE_MICRONOVA_WRITER
  if (!this->write_queue_.empty()) {
    this->current_command_ = this->write_queue_.front();
    this->write_queue_.pop();
    this->send_current_command_();
    return;
  }
#endif
#ifdef MICRONOVA_LISTENER_COUNT
  if (!this->read_queue_.empty()) {
    this->current_command_ = this->read_queue_.front();
    this->read_queue_.pop();
    this->send_current_command_();
  }
#endif
}

#ifdef MICRONOVA_LISTENER_COUNT
void MicroNova::queue_read_request(uint8_t location, uint8_t address) {
  // Check if this read is already queued
  for (const auto &queued : this->read_queue_) {
    if (queued.memory_location == location && queued.memory_address == address) {
      ESP_LOGV(TAG, "Read [%02X,%02X] already queued, skipping", location, address);
      return;
    }
  }

  MicroNovaCommand cmd;
  cmd.memory_location = location;
  cmd.memory_address = address;
  cmd.data = 0;

  if (!this->read_queue_.push(cmd)) {
    ESP_LOGW(TAG, "Read queue full, dropping read [%02X,%02X]", location, address);
    return;
  }
  ESP_LOGV(TAG, "Queued read [%02X,%02X] (queue size: %u)", location, address, this->read_queue_.size());
}
#endif

void MicroNova::send_current_command_() {
  uint8_t trash_rx;

  // Clear rx buffer - stove hiccups may cause late replies in the rx
  while (this->available()) {
    this->read_byte(&trash_rx);
    ESP_LOGW(TAG, "Reading excess byte 0x%02X", trash_rx);
  }

  uint8_t write_data[4] = {this->current_command_.memory_location, this->current_command_.memory_address, 0, 0};
  size_t write_len;

  if (this->current_command_.is_write()) {
    write_len = 4;
    write_data[2] = this->current_command_.data;
    // calculate checksum
    write_data[3] = write_data[0] + write_data[1] + write_data[2];
    ESP_LOGV(TAG, "Sending write request [%02X,%02X,%02X,%02X]", write_data[0], write_data[1], write_data[2],
             write_data[3]);
  } else {
    write_len = 2;
    ESP_LOGV(TAG, "Sending read request [%02X,%02X]", write_data[0], write_data[1]);
  }

  this->enable_rx_pin_->digital_write(true);
  this->write_array(write_data, write_len);
  this->flush();
  this->enable_rx_pin_->digital_write(false);

  this->transmission_time_ = millis();
  this->reply_pending_ = true;
}

int MicroNova::read_stove_reply_() {
  uint8_t reply_data[2] = {0, 0};

  this->read_array(reply_data, 2);

  ESP_LOGV(TAG, "Reply from stove [%02X,%02X]", reply_data[0], reply_data[1]);

  uint8_t checksum = this->current_command_.memory_location + this->current_command_.memory_address + reply_data[1];
  if (reply_data[0] != checksum) {
    ESP_LOGE(TAG, "Checksum mismatch! From [0x%02X:0x%02X] received [0x%02X,0x%02X]. Expected 0x%02X, got 0x%02X",
             this->current_command_.memory_location, this->current_command_.memory_address, reply_data[0],
             reply_data[1], checksum, reply_data[0]);
    return -1;
  }
  return ((int) reply_data[1]);
}

#ifdef USE_MICRONOVA_WRITER
bool MicroNova::queue_write_command(uint8_t location, uint8_t address, uint8_t data) {
  MicroNovaCommand cmd;
  cmd.memory_location = location | WRITE_BIT;
  cmd.memory_address = address;
  cmd.data = data;

  // Check if a write to the same address is already queued - update data in-place
  for (auto &queued : this->write_queue_) {
    if (queued.memory_location == cmd.memory_location && queued.memory_address == cmd.memory_address) {
      if (queued.data != cmd.data) {
        ESP_LOGD(TAG, "Updating queued write [%02X,%02X] data 0x%02X -> 0x%02X", location, address, queued.data, data);
        queued.data = cmd.data;
      } else {
        ESP_LOGV(TAG, "Write [%02X,%02X] with data 0x%02X already queued, skipping", location, address, data);
      }
      return true;
    }
  }

  if (!this->write_queue_.push(cmd)) {
    ESP_LOGW(TAG, "Write queue full, dropping command");
    return false;
  }
  ESP_LOGD(TAG, "Queued write [%02X,%02X] (queue size: %u)", location, address, this->write_queue_.size());
#ifdef MICRONOVA_LISTENER_COUNT
  // Automatically queue sensor updates after write commands
  this->request_update_listeners_();
#endif
  return true;
}
#endif

}  // namespace esphome::micronova
