#pragma once
#include "esphome/core/component.h"

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
  Frame(uint8_t *data) : pbuf_(data) {}
  Frame(const Frame &frame) : pbuf_(frame.data()) {}

  // Frame buffer
  uint8_t *data() const { return this->pbuf_; }
  // Frame size
  uint8_t size() const { return this->length_() + OFFSET_LENGTH; }
  uint8_t app_type() const { return this->pbuf_[OFFSET_APPTYPE]; }

  template<typename T> typename std::enable_if<std::is_base_of<Frame, T>::value, T>::type as() const {
    return T(*this);
  }
  String to_string() const;

 protected:
  uint8_t *pbuf_;
  uint8_t length_() const { return this->pbuf_[OFFSET_LENGTH]; }
};

class BaseFrame : public Frame {
 public:
  BaseFrame() = delete;
  BaseFrame(uint8_t *data) : Frame(data) {}
  BaseFrame(const Frame &frame) : Frame(frame) {}

  // Check for valid
  bool is_valid() const;
  // Prepare for sending to device
  void finalize();
  uint8_t get_type() const { return this->pbuf_[9]; }
  void set_type(uint8_t value) { this->pbuf_[9] = value; }
  bool has_response_type(uint8_t type) const { return this->resp_type_() == type; }
  bool has_type(uint8_t type) const { return this->get_type() == type; }

 protected:
  static const uint8_t PROGMEM CRC_TABLE[256];
  void set_bytemask_(uint8_t idx, uint8_t mask, bool state);
  uint8_t resp_type_() const { return this->pbuf_[OFFSET_BODY]; }
  bool has_valid_crc_() const;
  bool has_valid_cs_() const;
  void update_crc_();
  void update_cs_();
};

template<typename T = Frame, size_t buf_size = 36> class StaticFrame : public T {
 public:
  // Default constructor
  StaticFrame() : T(this->buf_) {}
  // Copy constructor
  StaticFrame(const Frame &src) : T(this->buf_) {
    if (src.length_() < sizeof(this->buf_)) {
      memcpy(this->buf_, src.data(), src.length_() + OFFSET_LENGTH);
    }
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
class NotifyFrame : public midea_dongle::StaticFrame<BaseFrame, 32> {
 public:
  NotifyFrame() : StaticFrame(FPSTR(NotifyFrame::INIT)) {}
  void set_signal_strength(uint8_t value) { this->pbuf_[12] = value; }
  uint8_t get_signal_strength() const { return this->pbuf_[12]; }
  void set_connected(bool state) { this->pbuf_[18] = state ? 0 : 1; }
  bool is_connected() const { return !this->pbuf_[18]; }

 private:
  static const uint8_t PROGMEM INIT[];
};

}  // namespace midea_dongle
}  // namespace esphome
