#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "core_logic.h"
#include "core_types.h"
#include "core_state.h"
#include "core_frame.h"
#include "core_messages.h"

#include <memory>
#include <cstdarg>

namespace esphome
{
  namespace sharp_ac
  {

    using climate::ClimateCall;
    using climate::ClimateFanMode;
    using climate::ClimateMode;
    using climate::ClimatePreset;
    using climate::ClimateSwingMode;
    using climate::ClimateTraits;

    class VaneSelectVertical;
    class VaneSelectHorizontal;
    class ConnectionStatusSensor;
    class ReconnectButton;
    class SharpAc; 

    class ESPHomeHardwareInterface : public SharpAcHardwareInterface {
    public:
      ESPHomeHardwareInterface(uart::UARTDevice* uart_device) : uart_device_(uart_device) {}

      size_t read_array(uint8_t *data, size_t len) override {
        return uart_device_->read_array(data, len) ? len : 0;
      }

      size_t available() override {
        return uart_device_->available();
      }

      void write_array(const uint8_t *data, size_t len) override {
        uart_device_->write_array(data, len);
      }

      uint8_t peek() override {
        return uart_device_->peek();
      }

      uint8_t read() override {
        return uart_device_->read();
      }

      unsigned long get_millis() override {
        return millis();
      }

      void log_debug(const char* tag, const char* format, ...) override {
        va_list args;
        va_start(args, format);
        esp_log_vprintf_(ESPHOME_LOG_LEVEL_DEBUG, tag, __LINE__, format, args);
        va_end(args);
      }

      void format_hex_pretty_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t len) override {
        esphome::format_hex_pretty_to(buffer, buffer_size, data, len, ' ');
      }

    private:
      uart::UARTDevice* uart_device_;
    };

    class ESPHomeStateCallback : public SharpAcStateCallback {
    public:
      ESPHomeStateCallback(SharpAc* sharp_ac) : sharp_ac_(sharp_ac) {}

      void on_state_update() override;
      void on_ion_state_update(bool state) override;
      void on_vane_horizontal_update(SwingHorizontal val) override;
      void on_vane_vertical_update(SwingVertical val) override;
      void on_connection_status_update(int status) override;

    private:
      SharpAc* sharp_ac_;
    };

    class SharpAc : public climate::Climate, public uart::UARTDevice, public Component
    {
    public:
      SharpAc();
      
      void control(const climate::ClimateCall &call) override;
      void dump_config() override;
      void loop() override;
      void setup() override;
      float get_setup_priority() const override { return setup_priority::HARDWARE; }
      esphome::climate::ClimateTraits traits() override;

      void setIon(bool state);
      void setVaneHorizontal(SwingHorizontal val);
      void setVaneVertical(SwingVertical val);

      void publishUpdate();

      void setIonSwitch(switch_::Switch *ionSwitch)
      {
        this->ionSwitch = ionSwitch;
      };
      void setVaneVerticalSelect(VaneSelectVertical *vane)
      {
        this->vaneVertical = vane;
      };
      void setVaneHorizontalSelect(VaneSelectHorizontal *vane)
      {
        this->vaneHorizontal = vane;
      };
      void setConnectionStatusSensor(text_sensor::TextSensor *sensor)
      {
        this->connectionStatusSensor = sensor;
      };

      void setReconnectButton(button::Button *button)
      {
        this->reconnectButton = button;
      };

      void updateConnectionStatus(int status);
      void triggerReconnect();

    private:
      std::unique_ptr<ESPHomeHardwareInterface> hardware_interface_;
      std::unique_ptr<ESPHomeStateCallback> state_callback_;
      std::unique_ptr<SharpAcCore> core_;

      switch_::Switch *ionSwitch{nullptr};
      VaneSelectVertical *vaneVertical{nullptr};
      VaneSelectHorizontal *vaneHorizontal{nullptr};
      text_sensor::TextSensor *connectionStatusSensor{nullptr};
      button::Button *reconnectButton{nullptr};
    };
  }
}
