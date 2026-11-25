// Identity (0x1018), device information (0x1000, 0x1008-0x100A) and the default SDO server (0x1200).
#include "canopen.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

void CANopenComponent::add_device_info_od_() {
  auto &od = this->od_;
  od.add_var(0x1000, ODA_SDO_R, &this->dev_type_, sizeof(this->dev_type_));
  if (this->device_name_ == nullptr)
    this->device_name_ = App.get_name().c_str();
  if (this->software_version_ == nullptr)
    this->software_version_ = ESPHOME_VERSION;
  od.add_string(0x1008, this->device_name_);
  if (this->hardware_version_ != nullptr)
    od.add_string(0x1009, this->hardware_version_);
  od.add_string(0x100A, this->software_version_);
  od.add_record(0x1018, {{0, ODA_SDO_R, &this->identity_highest_sub_, 1},
                         {1, ODA_SDO_R, &this->identity_[0], 4},
                         {2, ODA_SDO_R, &this->identity_[1], 4},
                         {3, ODA_SDO_R, &this->identity_[2], 4},
                         {4, ODA_SDO_R, &this->identity_[3], 4}});
  od.add_record(0x1200, {{0, ODA_SDO_R, &this->sdo_srv_highest_sub_, 1},
                         {1, ODA_SDO_R, &this->sdo_srv_cob_rx_, 4},
                         {2, ODA_SDO_R, &this->sdo_srv_cob_tx_, 4}});
}

void CANopenComponent::config_device_info_(CO_config_t &cfg) {
  // the default SDO server (CiA 301: mandatory)
  cfg.CNT_SDO_SRV = 1;
  cfg.ENTRY_H1200 = this->od_.find(0x1200);
}

void CANopenComponent::dump_device_info_config_() {
  ESP_LOGCONFIG(
      TAG, "  Identity: vendor 0x%08" PRIx32 ", product 0x%08" PRIx32 ", revision 0x%08" PRIx32 ", serial 0x%08" PRIx32,
      this->identity_[0], this->identity_[1], this->identity_[2], this->identity_[3]);
  ESP_LOGCONFIG(TAG, "  Device name: %s, software version: %s", this->device_name_, this->software_version_);
  if (this->hardware_version_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Hardware version: %s", this->hardware_version_);
  }
}

}  // namespace esphome::canopen
