#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace nfc {

class NfcTagBinarySensor : public binary_sensor::BinarySensor,
                           public Component,
                           public NfcTagListener,
                           public Parented<Nfcc> {
 public:
  void setup() override;
  void dump_config() override;

  void set_ndef_match_string(const std::string &str);
  void set_tag_name(const std::string &str);
  void set_uid(const std::vector<uint8_t> &uid);

  bool tag_match_ndef_string(const std::shared_ptr<NdefMessage> &msg);
  bool tag_match_tag_name(const std::shared_ptr<NdefMessage> &msg);
  bool tag_match_uid(const std::vector<uint8_t> &data);

  void tag_off(NfcTag &tag) override;
  void tag_on(NfcTag &tag) override;

 protected:
  bool match_tag_name_{false};
  std::string match_string_;
  std::vector<uint8_t> uid_;
};

}  // namespace nfc
}  // namespace esphome
