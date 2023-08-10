#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_bt_classic/bt_classic.h"

#ifdef USE_ESP32

namespace esphome {
namespace bt_classic_presence {

class BTClassicPresenceDevice : public PollingComponent,
                                public binary_sensor::BinarySensorInitiallyOff,
                                public esp32_bt_classic::BtClassicScanResultListner {
 public:
  BTClassicPresenceDevice(esp32_bt_classic::ESP32BtClassic *bt_client, uint64_t mac_address, uint8_t num_scans)
      : num_scans(num_scans), u64_addr(mac_address) {
    bt_client->register_scan_result_listener(this);
  }

  void dump_config() override;
  void update() override;
  void on_scan_result(const esp32_bt_classic::rmt_name_result &result) override;

 protected:
  uint8_t scans_remaining{0};
  const uint8_t num_scans;
  const uint64_t u64_addr;
};

}  // namespace bt_classic_presence
}  // namespace esphome

#endif
