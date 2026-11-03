#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/spi/spi.h"

#include <algorithm>
#include <memory>
#include <string>

namespace esphome::trf7962a {

// Reads are clocked on the opposite edge to writes; a second client on the bus provides that mode
class TRF7962AReadClient final : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                       spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_2MHZ> {};

class TRF7962A final : public PollingComponent,
                       public nfc::Nfcc,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_select_pin(GPIOPin *pin) { this->select_pin_ = pin; }
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }
  void set_supply_5v(bool supply_5v) { this->supply_5v_ = supply_5v; }
  void set_half_power(bool half_power) { this->half_power_ = half_power; }

  template<typename F> void add_on_tag_callback(F &&callback) { this->on_tag_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_tag_removed_callback(F &&callback) {
    this->on_tag_removed_callback_.add(std::forward<F>(callback));
  }

 protected:
  enum class State : uint8_t { IDLE, FIELD_SETTLE, WAIT_RESPONSE };

  void select_(bool selected);
  void direct_command_(uint8_t command);
  void write_register_(uint8_t reg, uint8_t value);
  void read_registers_(uint8_t reg, uint8_t *data, size_t len);
  uint8_t read_register_(uint8_t reg);
  uint8_t read_irq_status_();
  bool configure_();
  void set_field_(bool on);
  void send_inventory_();
  bool read_response_();
  void finish_scan_(bool response_ok);
  void remove_tag_();

  TRF7962AReadClient read_client_;
  GPIOPin *select_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};
  std::unique_ptr<nfc::NfcTag> current_tag_;
  LazyCallbackManager<void(const std::string &, const nfc::NfcTag &)> on_tag_callback_;
  LazyCallbackManager<void(const std::string &, const nfc::NfcTag &)> on_tag_removed_callback_;
  uint32_t state_start_{0};
  State state_{State::IDLE};
  uint8_t rx_[10]{};
  uint8_t rx_len_{0};
  uint8_t miss_count_{0};
  bool supply_5v_{true};
  bool half_power_{false};
};

}  // namespace esphome::trf7962a
