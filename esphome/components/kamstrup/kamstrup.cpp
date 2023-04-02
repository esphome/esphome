#include "kamstrup.h"
#include "esphome/core/log.h"

#include <algorithm>

/*
 * As the Kamstrup meter communicates in 1200 baud with long data packages,
 * we cannot simply wait for the bytes to arrive, instead we read and process
 * the bytes as they becomes available.
 */

namespace esphome {
namespace kamstrup {

static const char TAG[] = "kamstrup";

void Kamstrup::dump_config() {
  ESP_LOGCONFIG(TAG, "Kamstrup device");
  ESP_LOGCONFIG(TAG, "  Receive timeout: %.1fs", this->receive_timeout_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Bundle requests: %d", this->bundle_requests_);
  LOG_UPDATE_INTERVAL(this);

  for (const auto &kv : this->sensors_) {
    LOG_SENSOR("  ", "Sensor", kv.second);
    ESP_LOGCONFIG(TAG, "    Register: %d (0x%04x)", kv.first, kv.first);
  }
}

void Kamstrup::setup() {
  this->queue_.reserve(this->sensors_.size());
  this->working_regs_.reserve(this->sensors_.size());
  ESP_LOGD(TAG, "setup: done");
}

void Kamstrup::update() {
  // Refill the queue
  this->queue_.clear();
  for (auto it = this->sensors_.crbegin(); it != this->sensors_.crend(); ++it) {
    this->queue_.push_back(it->first);
  }
  if (this->state_ == IDLE)
    this->state_ = START;
}

void Kamstrup::loop() {
  // Inactive...
  if (this->state_ == IDLE) {
    return;
  }
  this->handle_serial_();
}

// handle_serial_ - if data is available, process it while keeping track if
// we have seen the 0x40 start character and unescaping special characters
// on the way. When a valid line is detected, it decodes the registers and
// values and updates their registered sensors.
void Kamstrup::handle_serial_() {
  uint8_t r;
  if (this->state_ == 3 || this->state_ == 4 || this->state_ == 5) {
    // handle rx timeout
    if (millis() - this->starttime_ > this->receive_timeout_) {
      ESP_LOGW(TAG, "Timed out");
      this->state_ = RETRY;
      return;
    }
    if (!available() || !read_byte(&r)) {
      return;
    }
  }

  switch (this->state_) {
    case START:  // Start a new cycle
      if (this->queue_.empty()) {
        // We're done!
        ESP_LOGV(TAG, "queue empty");
        this->state_ = IDLE;
        break;
      }
      // transfer `bundle_requests` from queue to working_regs.
      this->working_regs_.clear();
      for (int i = 0; i < this->bundle_requests_ && !this->queue_.empty(); i++) {
        uint16_t reg = this->queue_.back();
        this->queue_.pop_back();
        this->working_regs_.push_back(reg);
      }
      ESP_LOGV(TAG, "got %d working_regs, queue left %d", this->working_regs_.size(), this->queue_.size());
      this->retry_ = 3;
      this->state_ = RETRY;
      break;
    case RETRY:  // (Re)send the get register cmd.
      if (--this->retry_ == 0) {
        ESP_LOGD(TAG, "giving up on these registers: %s", format_hex_pretty(this->working_regs_).c_str());
        this->state_ = START;
        break;
      }
      ESP_LOGV(TAG, "starting work on %s", format_hex_pretty(this->working_regs_).c_str());
      // Purge rX buffer
      while (available()) {
        read_byte(&r);
      }
      this->send_command_({this->working_regs_});
      this->state_ = WAIT_BEGIN;
      this->starttime_ = millis();
      this->bufsize_ = 0;
      break;
    case WAIT_BEGIN:  // Wait for start marker '@'
      if (r == 0x40) {
        // We got our marker
        this->state_ = DATA;
      }
      break;
    case DATA:  // Collect data
      if (r == 0x0d) {
        // We found our EOL
        this->state_ = LINE;
        break;
      }
      if (r == 0x1b) {
        // Break detected
        this->state_ = ESCAPED_DATA;
        break;
      }
      if (this->bufsize_ != 128) {
        this->buffer_[this->bufsize_++] = r;
      }
      break;
    case ESCAPED_DATA:  // Collect escaped data
      this->state_ = DATA;
      r ^= 0xff;
      if (r != 0x06 && r != 0x0d && r != 0x1b && r != 0x40 && r != 0x80) {
        ESP_LOGW(TAG, "Missing escape %X", r);
      }
      if (this->bufsize_ != 512) {
        this->buffer_[this->bufsize_++] = r;
      }
      break;
    case LINE:  // Got a line!
    {
      this->state_ = RETRY;
      // skip if message is not valid
      if (this->bufsize_ < 5 || this->buffer_[0] != 0x3f || this->buffer_[1] != 0x10) {
        ESP_LOGW(TAG, "Invalid message");
        break;
      }
      // check CRC
      if (this->crc_1021_(this->buffer_.begin(), this->bufsize_)) {
        ESP_LOGW(TAG, "CRC error");
        break;
      }
      const uint8_t *msgptr = &this->buffer_[2];
      const uint8_t *end = &this->buffer_[this->bufsize_ - 2];
      while (msgptr != end) {
        uint16_t reg;
        float val;
        int len = this->consume_register_(msgptr, end, &reg, &val);
        if (len == 0) {
          break;
        }
        this->sensors_[reg]->publish_state(val);
        auto pos = std::find(this->working_regs_.begin(), this->working_regs_.end(), reg);
        if (pos != this->working_regs_.end()) {
          this->working_regs_.erase(pos);
        }
        msgptr += len;
      }
      if (!this->working_regs_.empty()) {
        ESP_LOGD(TAG, "Not all registers reported back: %s", format_hex_pretty(this->working_regs_).c_str());
        break;
      }
      this->state_ = START;
    } break;
    default:
      ESP_LOGD(TAG, "invalid state %d", this->state_);
      this->state_ = IDLE;
      break;
  }
}

// send_command_ - format the given list of registers to the Kamstrup format,
// add prefix and crc and finally escape some special chanracters before
// sending it out.
void Kamstrup::send_command_(const std::vector<uint16_t> &regs) {
  // leave room for checksum bytes to message
  uint8_t newmsg[3 + regs.size() * sizeof(uint16_t) + 2];
  newmsg[0] = 0x3F;
  newmsg[1] = 0x10;
  newmsg[2] = regs.size();
  int size = 3;
  for (const uint16_t &reg : regs) {
    newmsg[size++] = reg >> 8;
    newmsg[size++] = reg & 0xff;
  }
  newmsg[size++] = 0x00;
  newmsg[size++] = 0x00;
  uint16_t crc = this->crc_1021_(newmsg, size);
  newmsg[size - 2] = crc >> 8;
  newmsg[size - 1] = crc & 0xff;

  // build final transmit message - escape various bytes
  std::vector<uint8_t> txmsg{0x80};  // prefix
  txmsg.reserve(size + 5);
  for (const uint8_t &ch : newmsg) {
    if (ch == 0x06 || ch == 0x0d || ch == 0x1b || ch == 0x40 || ch == 0x80) {
      txmsg.push_back(0x1b);
      txmsg.push_back(ch ^ 0xff);
    } else {
      txmsg.push_back(ch);
    }
  }
  txmsg.push_back(0x0d);  // EOL

  // send to serial interface
  write_array(txmsg);
}

// consume_register_ - extracts the register and its value.
// Returns length read or 0 if there was a problem
int Kamstrup::consume_register_(const uint8_t *msg, const uint8_t *end, uint16_t *register_id, float *value) {
  const size_t len = end - msg;
  if (len < 5 || len < 5 + msg[3]) {
    ESP_LOGD(TAG, "Not enough bytes left to decode");
    return 0;
  }

  *register_id = (msg[0] << 8) | msg[1];
  ESP_LOGV(TAG, "Found register %d, decoding", *register_id);

  // Decode the mantissa
  int32_t x = 0;
  for (int i = 0; i < msg[3]; i++) {
    x <<= 8;
    x |= msg[i + 5];
  }

  // Decode the exponent
  int i = msg[4] & 0x3f;
  if (msg[4] & 0x40) {
    i = -i;
  };
  float ifl = pow(10, i);
  if (msg[4] & 0x80) {
    ifl = -ifl;
  }

  // Return value and final length
  *value = x * ifl;
  return 5 + msg[3];
}

// crc_1021_ - calculate the crc1021 polynominal over the given bytes.
// Return the crc as an unsigned 16 bit int.
uint16_t Kamstrup::crc_1021_(const uint8_t msg[], size_t msgsize) const {
  uint32_t creg = 0x0000;
  for (size_t i = 0; i < msgsize; i++) {
    uint8_t mask = 0x80;
    while (mask > 0) {
      creg <<= 1;
      if (msg[i] & mask) {
        creg |= 1;
      }
      mask >>= 1;
      if (creg & 0x10000) {
        creg &= 0xffff;
        creg ^= 0x1021;
      }
    }
  }
  return creg;
}

}  // namespace kamstrup
}  // namespace esphome

// vim: set expandtab tabstop=2 shiftwidth=2:
