#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/spi/spi.h"

#include <string>
#include <utility>

namespace esphome::trf7962a {

// Reads are clocked on the opposite edge to writes; a second client on the bus provides that mode
class TRF7962AReadClient final : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                       spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_2MHZ> {};

class TRF7962A final : public PollingComponent,
                       public nfc::Nfcc,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  // base_chip_status holds the supply and RF power bits, computed in Python
  TRF7962A(GPIOPin *select_pin, InternalGPIOPin *irq_pin, uint8_t base_chip_status)
      : select_pin_(select_pin), irq_pin_(irq_pin), base_chip_status_(base_chip_status) {}

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  template<typename F> void add_on_tag_callback(F &&callback) { this->on_tag_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_tag_removed_callback(F &&callback) {
    this->on_tag_removed_callback_.add(std::forward<F>(callback));
  }

 protected:
  enum class State : uint8_t { STATE_IDLE, STATE_FIELD_SETTLE, STATE_WAIT_RESPONSE };

  void select_(bool selected);
  void write_frame_(const uint8_t *data, size_t len);
  void direct_command_(uint8_t command);
  void write_register_(uint8_t reg, uint8_t value);
  void read_registers_(uint8_t reg, uint8_t *data, size_t len);
  uint8_t read_register_(uint8_t reg);
  uint8_t read_irq_status_();
  bool configure_();
  void set_field_(bool on);
  void send_inventory_();
  bool read_response_(uint8_t *rx);
  void end_scan_();
  // rx is nullptr when no valid response arrived
  void finish_scan_(const uint8_t *rx);
  void remove_tag_();

  TRF7962AReadClient read_client_;
  GPIOPin *select_pin_;
  InternalGPIOPin *irq_pin_;
  // An empty UID means no tag is present
  nfc::NfcTag tag_{nfc::NfcTagUid{}, "ISO 15693"};
  LazyCallbackManager<void(const std::string &, const nfc::NfcTag &)> on_tag_callback_;
  LazyCallbackManager<void(const std::string &, const nfc::NfcTag &)> on_tag_removed_callback_;
  uint32_t state_start_{0};
  State state_{State::STATE_IDLE};
  uint8_t base_chip_status_;
  uint8_t miss_count_{0};
};

}  // namespace esphome::trf7962a
