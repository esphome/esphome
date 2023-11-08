#include "hdmi_cec.h"

#include "esphome/core/log.h"

namespace esphome {
namespace hdmi_cec {

static const char *const TAG = "hdmi_cec";
static const uint32_t START_BIT_MIN_US = 3500;
static const uint32_t HIGH_BIT_MIN_US = 400;
static const uint32_t HIGH_BIT_MAX_US = 800;

void HDMICEC::setup() {
  this->recv_frame_buffer_.reserve(16); // max 16 bytes per CEC frame
  this->cec_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->cec_pin_->attach_interrupt(HDMICEC::falling_edge_interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
  this->cec_pin_->attach_interrupt(HDMICEC::rising_edge_interrupt, this, gpio::INTERRUPT_RISING_EDGE);
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  LOG_PIN("  Pin: ", this->cec_pin_);
}

void IRAM_ATTR HDMICEC::falling_edge_interrupt(HDMICEC *self) {
  self->last_falling_edge_us_ = micros();
}

void IRAM_ATTR HDMICEC::rising_edge_interrupt(HDMICEC *self) {
  if (self->last_falling_edge_us_ == 0) {
    return;
  }

  auto pulse_duration = micros() - self->last_falling_edge_us_;
  self->last_falling_edge_us_ = 0;

  ESP_LOGI(TAG, "pulse duration: %zu ms", pulse_duration);
  
  if (pulse_duration > START_BIT_MIN_US) {
    // start bit detected. reset everything and start receiving
    ESP_LOGD(TAG, "start bit received");
    self->decoder_state_ = DecoderState::ReceivingByte;
    reset_state_variables(self);
    return;
  }

  bool value = (pulse_duration >= HIGH_BIT_MIN_US && pulse_duration <= HIGH_BIT_MAX_US);
  ESP_LOGD(TAG, "got bit: %d", value);

  switch (self->decoder_state_) {
    case DecoderState::ReceivingByte: {
      // write bit to the current byte
      self->recv_byte_buffer_ << 1;
      self->recv_byte_buffer_ |= (value & 0b1);

      self->recv_bit_counter_++;
      if (self->recv_bit_counter_ >= 8) {
        // if we reached eight bits, push the current byte to the frame buffer
        self->recv_frame_buffer_.push_back(self->recv_byte_buffer_);

        self->recv_bit_counter_ = 0;
        self->recv_byte_buffer_ = 0;

        self->decoder_state_ = DecoderState::WaitingForEOM;
      } else {
        self->decoder_state_ = DecoderState::ReceivingByte;
      }
      break;
    }

    case DecoderState::WaitingForEOM: {
      bool isEOM = (value == 1);
      if (isEOM) {
        // TODO pass frame to app
        ESP_LOGD("frame complete. first byte is %02x", self->recv_frame_buffer_[0]);
      }

      self->decoder_state_ = (
        isEOM
        ? DecoderState::WaitingForEOMAck
        : DecoderState::WaitingForAck
      );
      break;
    }

    case DecoderState::WaitingForAck: {
      self->decoder_state_ = DecoderState::ReceivingByte;
      break;
    }

    case DecoderState::WaitingForEOMAck: {
      reset_state_variables(self);
      self->decoder_state_ = DecoderState::Idle;
      break;
    }

    default: {
      ESP_LOGD(TAG, "invalid state: %u", (uint8_t)self->decoder_state_);
      self->decoder_state_ = DecoderState::ReceivingByte;
      reset_state_variables(self);
      break;
    }
  }
}

void IRAM_ATTR HDMICEC::reset_state_variables(HDMICEC *self) {
  self->recv_bit_counter_ = 0;
  self->recv_byte_buffer_ = 0x0;
  self->recv_frame_buffer_.clear();
}

}
}
