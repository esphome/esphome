#pragma once
#ifdef USE_ZEPHYR
#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/component.h"

// Include Zephyr headers needed for UDP transport
#include <zephyr/mgmt/mcumgr/transport/smp_udp.h>

struct img_mgmt_upload_check;

namespace esphome {
namespace zephyr_mcumgr {

// Inherit only from ota::OTAComponent (which should inherit from Component)
class OTAComponent : public ota::OTAComponent {
 public:
  OTAComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void on_network_update(bool connected, bool ip_address_changed);

  void set_udp_transport(bool enabled) { this->use_udp_transport_ = enabled; }
  void set_cdc_transport(bool enabled) { this->use_cdc_transport_ = enabled; }

  void update_chunk(const img_mgmt_upload_check &upload);
  void update_started();
  void update_chunk_wrote();
  void update_pending();

 private:
  void start_udp_transport();
  void stop_udp_transport();
  bool udp_transport_started_{false};
  bool use_udp_transport_{false};
  bool use_cdc_transport_{false};

 protected:
  uint32_t last_progress_ = 0;
  float percentage_ = 0;
  bool is_confirmed_ = false;
};

}  // namespace zephyr_mcumgr
}  // namespace esphome
#endif
