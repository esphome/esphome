#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::nfc {

class NfcTagBinarySensor final : public binary_sensor::BinarySensor, public Component, public NfcTagListener {
 public:
  void setup() override;
  void dump_config() override;

  // Both keep the pointer; the string must outlive the sensor (codegen passes a literal)
  void set_ndef_match_string(const char *str);
  void set_tag_name(const char *str);
  void set_uid(const NfcTagUid &uid);

  bool tag_match_ndef_string(const std::shared_ptr<NdefMessage> &msg);
  bool tag_match_tag_name(const std::shared_ptr<NdefMessage> &msg);
  bool tag_match_uid(const NfcTagUid &data);

  void tag_off(NfcTag &tag) override;
  void tag_on(NfcTag &tag) override;

 protected:
  bool match_tag_name_{false};
  StringRef match_string_;
  NfcTagUid uid_;
};

}  // namespace esphome::nfc
