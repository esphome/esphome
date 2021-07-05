#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_base/keeloq_protocol.h"

namespace esphome {
namespace keeloq_normal_crypter {

class KeeloqNormalCrypter : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_manufacturer_key(const uint64_t key) { this->mkey_ = key; }
  // Decrypts and validates the decrypted data against the serial number
  bool decrypt(remote_base::KeeloqData &data);
  bool encrypt(remote_base::KeeloqData &data);
  bool decrypt(remote_base::KeeloqData &data, uint64_t key);
  void encrypt(remote_base::KeeloqData &data, uint64_t key);
  uint64_t normal_keygen(uint32_t serial);

 protected:
  uint64_t mkey_{0};
};


}  // namespace empty_component
}  // namespace esphome