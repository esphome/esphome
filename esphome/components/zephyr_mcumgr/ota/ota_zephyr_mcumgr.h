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
  void dump_config() override;
  void update_chunk(const img_mgmt_upload_check &upload);
  void update_started();
  void update_chunk_wrote();
  void update_pending();
  void update_stopped();

 protected:
  uint32_t last_progress_ = 0;
  float percentage_ = 0;
  mgmt_callback img_mgmt_callback_{};
};

}  // namespace esphome::zephyr_mcumgr
#endif
