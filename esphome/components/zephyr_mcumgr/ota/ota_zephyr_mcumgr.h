#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ZEPHYR
#include "esphome/components/ota/ota_backend.h"
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

struct img_mgmt_upload_check;

namespace esphome::zephyr_mcumgr {

class OTAComponent : public ota::OTAComponent {
 public:
  OTAComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  void update_chunk(const img_mgmt_upload_check &upload);
  void update_started();
  void update_chunk_wrote();
  void update_pending();
  void set_cdc_uart() { cdc_uart_ = true; };

 protected:
  uint32_t last_progress_ = 0;
  float percentage_ = 0;
  bool is_confirmed_ = false;
  mgmt_callback img_mgmt_callback_;
  bool cdc_uart_ = false;
};

}  // namespace esphome::zephyr_mcumgr
#endif
