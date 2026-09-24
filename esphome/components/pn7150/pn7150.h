#pragma once

#include "esphome/components/pn71xx/pn71xx.h"

namespace esphome::pn7150 {

static constexpr uint8_t PMU_CFG[] = {
    0x01,        // Number of parameters
    0xA0, 0x0E,  // ext. tag
    3,           // length
    0x06,        // VBAT1 connected to 5V (CFG2)
    0x64,        // TVDD monitoring threshold = 5.0V; TxLDO voltage = 4.7V (in reader & card modes)
    0x01,        // RFU; must be 0x00 for CFG1 and 0x01 for CFG2
};

static constexpr uint8_t RF_LISTEN_MODE_ROUTING_CONFIG[] = {0x00,  // "more" (another message is coming)
                                                            1,     // number of table entries
                                                            0x01,  // type = protocol-based
                                                            3,     // length
                                                            0,     // DH NFCEE ID, a static ID representing the DH-NFCEE
                                                            0x01,  // power state
                                                            nfc::PROT_ISODEP};  // protocol

class PN7150 : public pn71xx::PN71xx {
 public:
  void dump_config() override;

 protected:
  uint8_t verify_reset_(nfc::NciMessage &rx, bool reset_config) override;
  uint8_t process_init_response_(nfc::NciMessage &rx) override;
  std::span<const uint8_t> pmu_config_() const override { return PMU_CFG; }
  std::span<const uint8_t> listen_mode_routing_config_() const override { return RF_LISTEN_MODE_ROUTING_CONFIG; }
};

}  // namespace esphome::pn7150
