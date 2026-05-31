#include "core_logic.h"

namespace esphome::sharp_ac {

SharpAcCore::SharpAcCore(SharpAcHardwareInterface *hardware, SharpAcStateCallback *callback)
    : hardware_(hardware), callback_(callback) {
  this->hardware_->log_debug(TAG, "SharpAcCore initialized successfully");
}

void SharpAcCore::write_ack_() {
  SharpACKFrame frame;
  this->write_frame_(frame);
}

void SharpAcCore::setup() {}

void SharpAcCore::start_init_() {
  if (this->awaiting_response_) {
    return;
  }

  if (this->connection_start_ == 0) {
    this->hardware_->log_debug(TAG, "Initializing connection...");
    this->connection_start_ = this->hardware_->get_millis();
  }

  SharpFrame frame(INIT_MSG, sizeof(INIT_MSG), true);
  this->write_frame_(frame);
}

void SharpAcCore::send_init_msg_(const uint8_t *arr, size_t size) {
  SharpFrame frame(arr, size, true);
  this->write_frame_(frame);
  this->status_++;

  if (this->callback_ != nullptr) {
    this->callback_->on_connection_status_update(this->status_);
  }

  if (this->status_ < 8) {
    this->hardware_->log_debug(TAG, "Connecting (%d/8)...", this->status_);
  } else {
    this->hardware_->log_debug(TAG, "Connected");
  }
}

void SharpAcCore::init_(SharpFrame &frame) {
  if (frame.get_size() == 0) {
    return;
  }

  switch (frame.get_data()[0]) {
    case 0x06:
      if (this->status_ == 2 || this->status_ == 7) {
        this->status_++;
        if (this->callback_ != nullptr) {
          this->callback_->on_connection_status_update(this->status_);
        }
        if (this->status_ < 8) {
          this->hardware_->log_debug(TAG, "Connecting (%d/8)...", this->status_);
        } else {
          this->hardware_->log_debug(TAG, "Connected");
        }
      }
      break;
    case 0x02:
      if (this->status_ == 0) {
        this->send_init_msg_(INIT_MSG2, sizeof(INIT_MSG2));
      } else if (this->status_ == 1) {
        this->send_init_msg_(SUBSCRIBE_MSG, sizeof(SUBSCRIBE_MSG));
      }
      break;
    case 0x03:
      if (this->status_ == 3) {
        this->send_init_msg_(SUBSCRIBE_MSG2, sizeof(SUBSCRIBE_MSG2));
      } else if (this->status_ == 4) {
        this->send_init_msg_(GET_STATE, sizeof(GET_STATE));
      }
      break;
    case 0xDC:
      if (this->status_ == 5) {
        this->send_init_msg_(GET_STATUS, sizeof(GET_STATUS));
      } else if (this->status_ == 6) {
        this->send_init_msg_(CONNECTED_MSG, sizeof(CONNECTED_MSG));
      }
      this->process_update_(frame);
      break;
    default:
      break;
  }
}

SharpFrame SharpAcCore::read_msg_() {
  uint8_t msg[18];
  uint8_t msg_id = this->hardware_->peek();

  if (msg_id == 0x06 || msg_id == 0x00) {
    uint8_t single_byte = this->hardware_->read();
    SharpFrame frame(single_byte);

    this->hardware_->log_debug(TAG, "RX: ACK");
    this->awaiting_response_ = false;

    return frame;
  }

  int size = 8;

  if (this->hardware_->available() < 8) {
    if (this->err_counter_ < 5) {
      this->err_counter_++;
      return SharpFrame(msg, 0);
    }

    this->err_counter_ = 0;
    uint8_t single_byte = this->hardware_->read();
    SharpFrame frame(single_byte);

    char buffer[4]{0};
    this->hardware_->format_hex_pretty_to(buffer, sizeof(buffer), &single_byte, 1);
    this->hardware_->log_debug(TAG, "RX: %s (error recovery)", buffer);

    return frame;
  }

  this->err_counter_ = 0;
  this->hardware_->read_array(msg, 8);

  if (msg_id == 0x02) {
    size += msg[6];
  } else if (msg_id == 0x03 && msg[1] == 0xFE && msg[2] == 0x00) {
    size = 17;
  } else if (msg_id == 0xDC) {
    if (msg[1] == 0x0B) {
      size = 14;
    } else if (msg[1] == 0x0F) {
      size = 18;
    }
  }

  if (size > 8) {
    this->hardware_->read_array(msg + 8, size - 8);
  }

  SharpFrame frame(msg, size);
  this->log_frame_("RX: ", frame.get_data(), frame.get_size());
  this->awaiting_response_ = false;

  return frame;
}

void SharpAcCore::process_update_(SharpFrame &frame) {
  if (frame.get_size() == 0 || frame.get_size() == 1) {
    return;
  }

  if (frame.get_size() == 18) {
    SharpStatusFrame status(frame.get_data());
    this->current_temperature_ = status.get_temperature();
    this->hardware_->log_debug(TAG, "Current temp: %.1f C", this->current_temperature_);
    this->publish_update();
  } else if (frame.get_size() >= 14) {
    SharpModeFrame status(frame.get_data());
    this->state_.fan = status.get_fan_mode();
    this->state_.mode = status.get_power_mode();
    this->state_.state = status.get_state();
    this->state_.swingH = status.get_swing_horizontal();
    this->state_.swingV = status.get_swing_vertical();
    this->state_.preset = status.get_preset();
    this->state_.ion = status.get_ion();

    if (this->state_.state) {
      if (this->state_.mode == PowerMode::COOL || this->state_.mode == PowerMode::HEAT) {
        this->state_.temperature = status.get_temperature();
      }
    }

    if (this->current_temperature_ > 0.0f) {
      this->publish_update();
    } else {
      this->hardware_->log_debug(TAG, "Waiting for temperature reading...");
    }
  }
}

void SharpAcCore::publish_update() {
  if (this->callback_ != nullptr) {
    this->callback_->on_state_update();
    this->callback_->on_ion_state_update(this->state_.ion);
    this->callback_->on_vane_horizontal_update(this->state_.swingH);
    this->callback_->on_vane_vertical_update(this->state_.swingV);
  }
}

void SharpAcCore::set_ion(bool state) {
  this->state_.ion = state;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::set_vane_horizontal(SwingHorizontal state) {
  this->state_.swingH = state;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::set_vane_vertical(SwingVertical state) {
  this->state_.swingV = state;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::control_mode(PowerMode mode, bool state) {
  this->state_.state = state;
  if (state) {
    this->state_.mode = mode;
  }
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::control_fan(FanMode fan) {
  this->state_.fan = fan;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::control_swing(SwingHorizontal h, SwingVertical v) {
  this->state_.swingH = h;
  this->state_.swingV = v;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::control_temperature(int temperature) {
  this->state_.temperature = temperature;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::control_preset(Preset preset) {
  this->state_.preset = preset;
  SharpCommandFrame frame = this->state_.to_frame();
  this->write_frame_(frame);
}

void SharpAcCore::reset_connection() {
  this->status_ = 0;
  this->awaiting_response_ = false;
  this->connection_start_ = 0;

  if (this->callback_ != nullptr) {
    this->callback_->on_connection_status_update(0);
  }
}

void SharpAcCore::check_timeout_() {
  if (!this->awaiting_response_) {
    return;
  }

  uint32_t current_millis = this->hardware_->get_millis();
  if (current_millis - this->last_request_time_ >= RESPONSE_TIMEOUT) {
    this->hardware_->log_debug(TAG, "Timeout - no response for 10s, reconnecting...");
    this->reset_connection();
  }
}

void SharpAcCore::loop() {
  uint32_t current_millis = this->hardware_->get_millis();

  this->check_timeout_();

  if (this->hardware_->available() > 0) {
    SharpFrame frame = this->read_msg_();
    frame.print();

    if (this->status_ < 8) {
      this->init_(frame);
    } else {
      this->process_update_(frame);
      if (frame.get_size() > 1) {
        this->write_ack_();
      }
    }
  }

  if (this->status_ != 8) {
    this->start_init_();
  } else if (current_millis - this->previous_millis_ >= INTERVAL) {
    this->previous_millis_ = current_millis;
    SharpFrame frame(GET_STATUS, sizeof(GET_STATUS), true);
    this->write_frame_(frame);
  }
}

}  // namespace esphome::sharp_ac
