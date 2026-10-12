#pragma once

#include <string>
#include "esphome/core/automation.h"

#include "nfc.h"

namespace esphome::nfc {

/// Fires with the formatted UID and the tag itself; the tag is passed by reference so no copy is made per trigger
class NfcOnTagTrigger final : public Trigger<std::string, const NfcTag &> {
 public:
  void process(const std::unique_ptr<NfcTag> &tag);
};

}  // namespace esphome::nfc
