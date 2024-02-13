#pragma once

#include "esphome/components/ota/ota_component.h"

struct img_mgmt_upload_check;

namespace esphome {
namespace zephyr_ota_mcumgr {

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

 protected:
  uint32_t last_progress_ = 0;
  float percentage_ = 0;
  bool is_confirmed_ = false;
};

}  // namespace zephyr_ota_mcumgr
}  // namespace esphome
