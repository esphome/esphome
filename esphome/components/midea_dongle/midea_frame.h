#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <IPAddress.h>

namespace esphome {
namespace midea_dongle {

static const uint8_t OFFSET_START = 0;
static const uint8_t OFFSET_LENGTH = 1;
static const uint8_t OFFSET_APPTYPE = 2;
static const uint8_t OFFSET_BODY = 10;
static const uint8_t SYNC_BYTE = 0xAA;

class Frame {
 public:
  Frame() = delete;
  Frame(uint8_t *data) : pb_(data) {}
  Frame(const Frame &frame) : pb_(frame.data()) {}

  bool has_valid_crc() const { return this->pb_[this->len_() - 1] == this->calc_crc_(); }
  bool has_valid_cs() const { return this->pb_[this->len_()] == this->calc_cs_(); }
  void update_crc() { this->pb_[this->len_() - 1] = this->calc_crc_(); }
  void update_cs() { this->pb_[this->len_()] = this->calc_cs_(); }
  void update_all() {
    this->update_crc();
    this->update_cs();
  }

  uint8_t *data() const { return this->pb_; }
  uint8_t size() const { return this->len_() + OFFSET_LENGTH; }
  void set_type(uint8_t value) { this->pb_[9] = value; }
  uint8_t get_type() const { return this->pb_[9]; }
  bool has_type(uint8_t value) const { return this->pb_[9] == value; }
  bool has_id(uint8_t value) const { return this->pb_[10] == value; }
  bool has_app(uint8_t value) const { return this->pb_[OFFSET_APPTYPE] == value; }
  uint8_t app_type() const { return this->pb_[OFFSET_APPTYPE]; }
  std::string to_string() const { return hexencode(*this); }

  template<typename T> typename std::enable_if<std::is_base_of<Frame, T>::value, T>::type as() const {
    return T(*this);
  }

 protected:
  uint8_t *pb_;
  uint8_t len_() const { return this->pb_[OFFSET_LENGTH]; }
  uint8_t resp_type_() const { return this->pb_[OFFSET_BODY]; }
  uint8_t calc_crc_() const;
  uint8_t calc_cs_() const;
  uint8_t get_val_(uint8_t idx, uint8_t shift = 0, uint8_t mask = 255) const { return (this->pb_[idx] >> shift) & mask; }
  void set_val_(uint8_t idx, uint8_t shift, uint8_t mask, uint8_t value) {
    this->pb_[idx] &= ~(mask << shift);
    this->pb_[idx] |= (value << shift);
  }
  void set_bytemask_(uint8_t idx, uint8_t mask, bool state) { this->set_val_(idx, 0, mask, state ? mask : 0); }
};

template<typename T = Frame, size_t buf_size = 36> class StaticFrame : public T {
 public:
  // Default constructor
  StaticFrame() : T(this->buf_) {}
  // Copy constructor
  StaticFrame(const Frame &src) : T(this->buf_) {
    if (src.size() <= sizeof(this->buf_))
      memcpy(this->buf_, src.data(), src.size());
  }
  // Constructor for RAM data
  StaticFrame(const uint8_t *src) : T(this->buf_) {
    const uint8_t len = src[OFFSET_LENGTH];
    if (len < sizeof(this->buf_)) {
      memcpy(this->buf_, src, len + OFFSET_LENGTH);
    }
  }
  // Constructor for PROGMEM data
  StaticFrame(const __FlashStringHelper *pgm) : T(this->buf_) {
    const uint8_t *src = reinterpret_cast<decltype(src)>(pgm);
    const uint8_t len = pgm_read_byte(src + OFFSET_LENGTH);
    if (len < sizeof(this->buf_)) {
      memcpy_P(this->buf_, src, len + OFFSET_LENGTH);
    }
  }

 protected:
  uint8_t buf_[buf_size];
};

// Device network notification frame
class NotifyFrame : public StaticFrame<Frame, 32> {
 public:
  NotifyFrame() : StaticFrame(FPSTR(NotifyFrame::INIT)) {}
  void set_connected(bool state) { this->pb_[18] = state ? 0 : 1; }
  void set_signal_strength(uint8_t value) { this->pb_[12] = value; }
  void set_ip(const IPAddress &ip);

 private:
  static const uint8_t PROGMEM INIT[];
};

// Frame reader from Stream object
template <size_t buf_size> class FrameReceiver : public StaticFrame<Frame, buf_size> {
 public:
  // Read frame from Stream. Return True if frame is received.
  bool read(Stream *stream) {
    while (stream->available() > 0) {
      const uint8_t data = stream->read();
      switch (this->index_) {
      case 0:
        if (data != SYNC_BYTE)
          continue;
        break;
      case OFFSET_LENGTH:
        if (data <= OFFSET_BODY || data >= sizeof(this->buf_)) {
          this->reset_();
          continue;
        }
        this->remain_ = data;
      }
      this->buf_[this->index_++] = data;
      if (--this->remain_)
        continue;
      this->reset_();
      return true;
    }
    return false;
  }
 protected:
  uint8_t index_{0};
  uint8_t remain_{0};
  void reset_() {
    this->index_ = 0;
    this->remain_ = 0;
  }
};

}  // namespace midea_dongle
}  // namespace esphome
