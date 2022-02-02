#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/output/float_output.h"

#define UPDATE_INTERVAL_MS  500
#define DMX_MAX_CHANNEL     512
#define DMX_MSG_SIZE        DMX_MAX_CHANNEL + 1
#define DMX_BREAK_LEN       92
#define DMX_MAB_LEN         12

namespace esphome {
namespace dmx512 {

class DMX512Output;

class DMX512 : public Component {
 public:
  DMX512() = default;
  void set_uart_parent(esphome::uart::UARTComponent *parent) { this->uart_ = parent; }

  void setup();

  void loop() override;

  void dump_config() override;
  
  virtual void sendBreak() = 0;

  void set_enable_pin(GPIOPin *pin_enable) { pin_enable_ = pin_enable; }
  
  void set_uart_tx_pin(InternalGPIOPin *tx_pin) { tx_pin_ = tx_pin; }
  
  void set_channel_used(uint16_t channel);

  void set_force_full_frames(bool force) { force_full_frames_ = force; }

  void set_periodic_update(bool update) { periodic_update_ = update; }

  void set_mab_len(int len) { mab_len_ = len; }

  void set_break_len(int len) { break_len_ = len; }

  void set_update_interval(int intvl) { update_interval_ = intvl; }

  virtual void set_uart_num(int num) = 0;

  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_channel(uint16_t channel, uint8_t value);

 protected:

  esphome::uart::UARTComponent *uart_{nullptr};
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_dmx512_transmission_{0};
  uint8_t device_values_[DMX_MSG_SIZE];
  int uart_idx_{0};
  InternalGPIOPin *tx_pin_{nullptr};
  int update_interval_{UPDATE_INTERVAL_MS};
  int mab_len_{DMX_MAB_LEN};
  int break_len_{DMX_BREAK_LEN};
  uint16_t max_chan_{0};
  bool update_{true};
  bool periodic_update_{true};
  bool force_full_frames_{false};
  unsigned long last_update_{0};
  GPIOPin *pin_enable_{nullptr};
};

class DMX512Output : public output::FloatOutput, public Component {
public:
  void set_universe(DMX512 *universe) { this->universe_ = universe; }
  void set_channel(uint16_t channel);
  void write_state(float state);

protected:
  uint16_t channel_{0};
  DMX512 *universe_{nullptr};
};

}  // namespace dmx512
}  // namespace esphome
