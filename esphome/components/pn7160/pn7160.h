#pragma once

#include "esphome/components/pn71xx/pn71xx.h"

namespace esphome::pn7160 {

static constexpr uint8_t PMU_CFG[] = {
    0x01,        // Number of parameters
    0xA0, 0x0E,  // ext. tag
    11,          // length
    0x11,        // IRQ Enable: PVDD + temp sensor IRQs
    0x01,        // RFU
    0x01,        // Power and Clock Configuration, device on (CFG1)
    0x01,        // Power and Clock Configuration, device off (CFG1)
    0x00,        // RFU
    0x00,        // DC-DC 0
    0x00,        // DC-DC 1
    // 0x14,        // TXLDO (3.3V / 4.75V)
    // 0xBB,        // TXLDO (4.7V / 4.7V)
    0xFF,  // TXLDO (5.0V / 5.0V)
    0x00,  // RFU
    0xD0,  // TXLDO check
    0x0C,  // RFU
};

static constexpr uint8_t RF_LISTEN_MODE_ROUTING_CONFIG[] = {0x00,  // "more" (another message is coming)
                                                            2,     // number of table entries
                                                            0x01,  // type = protocol-based
                                                            3,     // length
                                                            0,     // DH NFCEE ID, a static ID representing the DH-NFCEE
                                                            0x07,  // power state
                                                            nfc::PROT_ISODEP,  // protocol
                                                            0x00,              // type = technology-based
                                                            3,                 // length
                                                            0,     // DH NFCEE ID, a static ID representing the DH-NFCEE
                                                            0x07,  // power state
                                                            nfc::TECH_PASSIVE_NFCA};  // technology

class PN7160 : public pn71xx::PN71xx {
 public:
  void setup() override;
  void dump_config() override;

  void set_dwl_req_pin(GPIOPin *dwl_req_pin) { this->dwl_req_pin_ = dwl_req_pin; }
  void set_wkup_req_pin(GPIOPin *wkup_req_pin) { this->wkup_req_pin_ = wkup_req_pin; }

 protected:
  void prepare_reset() override;
  uint8_t verify_reset(nfc::NciMessage &rx, bool reset_config) override;
  uint8_t process_init_response(nfc::NciMessage &rx) override;
  std::span<const uint8_t> pmu_config() const override { return PMU_CFG; }
  std::span<const uint8_t> listen_mode_routing_config() const override { return RF_LISTEN_MODE_ROUTING_CONFIG; }

  GPIOPin *dwl_req_pin_{nullptr};
  GPIOPin *wkup_req_pin_{nullptr};
};

}  // namespace esphome::pn7160
