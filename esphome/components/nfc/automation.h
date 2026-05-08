#pragma once

#include <string>
#include "esphome/core/automation.h"

#include "nfc.h"

namespace esphome::nfc {

class NfcOnTagTrigger : public Trigger<std::string, NfcTag> {
 public:
  void process(const std::unique_ptr<NfcTag> &tag);
};

}  // namespace esphome::nfc
