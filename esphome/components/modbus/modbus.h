#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace modbus {

class ModbusDevice;

class Modbus : public uart::UARTDevice, public Component {
 public:

  static const bool  RX_ENABLE =  false ;
  static const bool  TX_ENABLE =  true ;
  
  Modbus() = default;

  void setup() override  {
    if(this->ctrl_pin_) {
        this->ctrl_pin_->setup();
        this->ctrl_pin_->digital_write(RX_ENABLE);
    }    
  }

  void loop() override;

  void dump_config() override;

  void register_device(ModbusDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;

  void send(uint8_t address, uint8_t function, uint16_t start_address, uint16_t register_count,const uint16_t *payload = nullptr);
  
  /** RX,TX Control pin Ref: https://github.com/greays/esphome/blob/master/esphome/components/rs485/rs485.h */
  void set_ctrl_pin(uint8_t ctrl_pin ) {
     static GPIOPin PIN (ctrl_pin,OUTPUT) ;
     ctrl_pin_ =  &PIN ;  
  }
 protected:
  bool parse_modbus_byte_(uint8_t byte);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_modbus_byte_{0};
  std::vector<ModbusDevice *> devices_;
  GPIOPin *ctrl_pin_{nullptr};  
};

class ModbusDevice {
 public:
  void set_parent(Modbus *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_modbus_data(const std::vector<uint8_t> &data) = 0;
  // provide a default implementation to avoid breaking existing code
  virtual void on_modbus_error(uint8_t function_code,uint8_t exception_code) {}
  void send(uint8_t function_code , uint16_t start_address, uint16_t  num_values, const uint16_t *payload = nullptr) {
     this->parent_->send(this->address_,function_code,start_address,num_values,payload) ;
  }

 protected:
  friend Modbus;

  Modbus *parent_;
  uint8_t address_;
};

}  // namespace modbus
}  // namespace esphome
