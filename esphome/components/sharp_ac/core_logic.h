#pragma once

#include <cstdint>
#include <cstddef>

#include "core_types.h"
#include "core_state.h"
#include "core_frame.h"
#include "core_messages.h"

namespace esphome::sharp_ac {

    static const char *const TAG = "sharp_ac.climate";

    class SharpAcHardwareInterface {
    public:
      virtual ~SharpAcHardwareInterface() = default;
      virtual size_t read_array(uint8_t *data, size_t len) = 0;
      virtual size_t available() = 0;
      virtual void write_array(const uint8_t *data, size_t len) = 0;
      virtual uint8_t peek() = 0;
      virtual uint8_t read() = 0;
      virtual uint32_t get_millis() = 0;
      virtual void log_debug(const char* tag, const char* format, ...) = 0;
      virtual void format_hex_pretty_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t len) = 0;
    };

class SharpAcStateCallback {
 public:
  virtual ~SharpAcStateCallback() = default;
  virtual void on_state_update() = 0;
  virtual void on_ion_state_update(bool state) = 0;
  virtual void on_vane_horizontal_update(SwingHorizontal val) = 0;
  virtual void on_vane_vertical_update(SwingVertical val) = 0;
  virtual void on_connection_status_update(int status) = 0;
};

class SharpAcCore {
 public:
  SharpAcCore(SharpAcHardwareInterface *hardware, SharpAcStateCallback *callback);
  virtual ~SharpAcCore() = default;

  void loop();
  void setup();

      void set_ion(bool state);
      void set_vane_horizontal(SwingHorizontal val);
      void set_vane_vertical(SwingVertical val);

      void publish_update();
      const SharpState &get_state() const { return state_; }
      float get_current_temperature() const { return current_temperature_; }

      void control_mode(PowerMode mode, bool state);
      void control_fan(FanMode fan);
      void control_swing(SwingHorizontal h, SwingVertical v);
      void control_temperature(int temperature);
      void control_preset(Preset preset);
      void reset_connection();

    protected:
      void log_frame_(const char *prefix, const uint8_t *data, size_t len)
      {
        char buffer[18 * 3]{0};
        this->hardware_->format_hex_pretty_to(buffer, sizeof(buffer), data, len);
        this->hardware_->log_debug(TAG, "%s%s", prefix, buffer);
      }

      void write_frame_(SharpFrame &frame)
      {
        frame.set_checksum();
        frame.print();
        
        if (frame.get_size() == 1 && frame.get_data()[0] == 0x06) {
          this->hardware_->log_debug(TAG, "TX: ACK");
        } else {
          this->log_frame_("TX: ", frame.get_data(), frame.get_size());
          this->awaiting_response_ = true;
          this->last_request_time_ = this->hardware_->get_millis();
        }
        
        this->hardware_->write_array(frame.get_data(), frame.get_size());
      }

      void write_ack_();

    private:
      SharpAcHardwareInterface *hardware_;
      SharpAcStateCallback *callback_;

      void send_init_msg_(const uint8_t *arr, size_t size);
      int err_counter_ = 0;

    protected:
      SharpState state_;
      void init_(SharpFrame &frame);
      SharpFrame read_msg_();
      void process_update_(SharpFrame &frame);
      void start_init_();
      void check_timeout_();
      int status_ = 0;
      uint32_t connection_start_ = 0;
      uint32_t previous_millis_ = 0;
      uint32_t last_request_time_ = 0;
      bool awaiting_response_ = false;
      static constexpr uint32_t interval_ = 60000UL;
      static constexpr uint32_t response_timeout_ = 10000UL; // 10 seconds
      float current_temperature_ = 0.0f;
    };

}  // namespace esphome::sharp_ac
