#pragma once

#include "esphome/core/defines.h"

#ifdef USE_I80
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"
#include "esphome/components/byte_bus/byte_bus.h"
#include <map>
#include <utility>
#include <vector>

namespace esphome {
namespace i80 {

static constexpr const char *TAG = "i80";

class I80Client;

class I80Delegate {
 public:
  I80Delegate() = default;
  // enable CS if configured.
  virtual void begin_transaction() {}

  // end the transaction
  virtual void end_transaction() {}

  virtual void write_cmd_data(int cmd, const uint8_t *ptr, size_t length) {}
  virtual void write_array(const uint8_t *data, size_t length){};

  virtual ~I80Delegate() = default;
};

class I80DelegateDummy : public I80Delegate {
  void begin_transaction() override { esph_log_e(TAG, "I80 bus not initialised - did you call bus_setup()?"); }
};

static I80Delegate *const NULL_DELEGATE = new I80DelegateDummy();  // NOLINT

class I80Bus {
 public:
  I80Bus() = default;

  virtual I80Delegate *get_delegate(GPIOPin *cs_pin, unsigned int data_rate) = 0;
};

class I80Client;

class I80Component : public Component {
 public:
  I80Component(InternalGPIOPin *wr_pin, InternalGPIOPin *dc_pin, std::vector<uint8_t> data_pins)
      : wr_pin_(wr_pin), dc_pin_(dc_pin), data_pins_(std::move(data_pins)) {}

  void setup() override;

  void set_rd_pin(InternalGPIOPin *rd_pin) { this->rd_pin_ = rd_pin; }
  void dump_config() override;
  I80Delegate *register_device(I80Client *device, GPIOPin *cs_pin, unsigned int data_rate);
  void unregister_device(I80Client *device);
  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  I80Bus *bus_{};
  InternalGPIOPin *wr_pin_{};
  InternalGPIOPin *rd_pin_{};
  InternalGPIOPin *dc_pin_{};
  std::vector<uint8_t> data_pins_{};
  std::map<I80Client *, I80Delegate *> devices_{};
};

class I80Client : public byte_bus::ByteBus {
 public:
  void bus_setup() override { this->delegate_ = this->parent_->register_device(this, this->cs_, this->data_rate_); }
  void bus_teardown() override {
    this->parent_->unregister_device(this);
    this->delegate_ = NULL_DELEGATE;
  }

  void write_cmd_data(int cmd, const uint8_t *data, size_t length) override {
    this->delegate_->write_cmd_data(cmd, data, length);
  }

  void write_array(const uint8_t *data, size_t length) override { this->delegate_->write_array(data, length); }
  void end_transaction() override { this->delegate_->end_transaction(); }
  void begin_transaction() override { this->delegate_->begin_transaction(); }

  void set_parent(I80Component *parent) { this->parent_ = parent; }

  void set_cs_pin(GPIOPin *cs) { this->cs_ = cs; }
  void dump_config() override;
  void set_data_rate(int data_rate) { this->data_rate_ = data_rate; }

 protected:
  I80Delegate *delegate_{NULL_DELEGATE};
  I80Component *parent_{};
  GPIOPin *cs_{byte_bus::NULL_PIN};
  uint32_t data_rate_{1000000};
};

}  // namespace i80
}  // namespace esphome
#endif
