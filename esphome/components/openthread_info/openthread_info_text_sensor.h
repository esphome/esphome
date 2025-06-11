#pragma once

#include "esphome/components/openthread/openthread.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#ifdef USE_OPENTHREAD

namespace esphome {
namespace openthread_info {

using esphome::openthread::InstanceLock;

class OpenThreadInstancePollingComponent : public PollingComponent {
 public:
  void update() override {
    auto lock = InstanceLock::try_acquire(10);
    if (!lock) {
      return;
    }

    this->update_instance(lock->get_instance());
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  virtual void update_instance(otInstance *instance) = 0;
};

class IPAddressOpenThreadInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    std::optional<otIp6Address> address = openthread::global_openthread_component->get_omr_address();
    if (!address) {
      return;
    }

    char address_as_string[40];
    otIp6AddressToString(&*address, address_as_string, 40);
    std::string ip = address_as_string;

    if (this->last_ip_ != ip) {
      this->last_ip_ = ip;
      this->publish_state(this->last_ip_);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::string last_ip_;
};

class RoleOpenThreadInfo : public OpenThreadInstancePollingComponent, public text_sensor::TextSensor {
 public:
  void update_instance(otInstance *instance) override {
    otDeviceRole role = otThreadGetDeviceRole(instance);

    if (this->last_role_ != role) {
      this->last_role_ = role;
      this->publish_state(otThreadDeviceRoleToString(this->last_role_));
    }
  }
  void dump_config() override;

 protected:
  otDeviceRole last_role_;
};

class Rloc16OpenThreadInfo : public OpenThreadInstancePollingComponent, public text_sensor::TextSensor {
 public:
  void update_instance(otInstance *instance) override {
    uint16_t rloc16 = otThreadGetRloc16(instance);
    if (this->last_rloc16_ != rloc16) {
      this->last_rloc16_ = rloc16;
      char buf[5];
      snprintf(buf, sizeof(buf), "%04x", rloc16);
      this->publish_state(buf);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  uint16_t last_rloc16_;
};

class ExtAddrOpenThreadInfo : public OpenThreadInstancePollingComponent, public text_sensor::TextSensor {
 public:
  void update_instance(otInstance *instance) override {
    const auto *extaddr = otLinkGetExtendedAddress(instance);
    if (!std::equal(this->last_extaddr_.begin(), this->last_extaddr_.end(), extaddr->m8)) {
      std::copy(extaddr->m8, extaddr->m8 + 8, this->last_extaddr_.begin());
      this->publish_state(format_hex(extaddr->m8, 8));
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::array<uint8_t, 8> last_extaddr_{};
};

class Eui64OpenThreadInfo : public OpenThreadInstancePollingComponent, public text_sensor::TextSensor {
 public:
  void update_instance(otInstance *instance) override {
    otExtAddress addr;
    otLinkGetFactoryAssignedIeeeEui64(instance, &addr);

    if (!std::equal(this->last_eui64_.begin(), this->last_eui64_.end(), addr.m8)) {
      std::copy(addr.m8, addr.m8 + 8, this->last_eui64_.begin());
      this->publish_state(format_hex(this->last_eui64_.begin(), 8));
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::array<uint8_t, 8> last_eui64_{};
};

class ChannelOpenThreadInfo : public OpenThreadInstancePollingComponent, public text_sensor::TextSensor {
 public:
  void update_instance(otInstance *instance) override {
    uint8_t channel = otLinkGetChannel(instance);
    if (this->last_channel_ != channel) {
      this->last_channel_ = channel;
      this->publish_state(std::to_string(this->last_channel_));
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  uint8_t last_channel_;
};

class DatasetOpenThreadInfo : public OpenThreadInstancePollingComponent {
 public:
  void update_instance(otInstance *instance) override {
    otOperationalDataset dataset;
    if (otDatasetGetActive(instance, &dataset) != OT_ERROR_NONE) {
      return;
    }

    this->update_dataset(&dataset);
  }

 protected:
  virtual void update_dataset(otOperationalDataset *dataset) = 0;
};

class NetworkNameOpenThreadInfo : public DatasetOpenThreadInfo, public text_sensor::TextSensor {
 public:
  void update_dataset(otOperationalDataset *dataset) override {
    if (this->last_network_name_ != dataset->mNetworkName.m8) {
      this->last_network_name_ = dataset->mNetworkName.m8;
      this->publish_state(this->last_network_name_);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::string last_network_name_;
};

class NetworkKeyOpenThreadInfo : public DatasetOpenThreadInfo, public text_sensor::TextSensor {
 public:
  void update_dataset(otOperationalDataset *dataset) override {
    if (!std::equal(this->last_key_.begin(), this->last_key_.end(), dataset->mNetworkKey.m8)) {
      std::copy(dataset->mNetworkKey.m8, dataset->mNetworkKey.m8 + 16, this->last_key_.begin());
      this->publish_state(format_hex(dataset->mNetworkKey.m8, 16));
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::array<uint8_t, 16> last_key_{};
};

class PanIdOpenThreadInfo : public DatasetOpenThreadInfo, public text_sensor::TextSensor {
 public:
  void update_dataset(otOperationalDataset *dataset) override {
    uint16_t panid = dataset->mPanId;
    if (this->last_panid_ != panid) {
      this->last_panid_ = panid;
      char buf[5];
      snprintf(buf, sizeof(buf), "%04x", panid);
      this->publish_state(buf);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  uint16_t last_panid_;
};

class ExtPanIdOpenThreadInfo : public DatasetOpenThreadInfo, public text_sensor::TextSensor {
 public:
  void update_dataset(otOperationalDataset *dataset) override {
    if (!std::equal(this->last_extpanid_.begin(), this->last_extpanid_.end(), dataset->mExtendedPanId.m8)) {
      std::copy(dataset->mExtendedPanId.m8, dataset->mExtendedPanId.m8 + 8, this->last_extpanid_.begin());
      this->publish_state(format_hex(this->last_extpanid_.begin(), 8));
    }
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::array<uint8_t, 8> last_extpanid_{};
};

}  // namespace openthread_info
}  // namespace esphome
#endif
