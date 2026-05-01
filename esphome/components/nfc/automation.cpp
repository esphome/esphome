#include "automation.h"
#include "nfc.h"

namespace esphome {
namespace nfc {

void NfcOnTagTrigger::process(const std::unique_ptr<NfcTag> &tag) {
  char uid_buf[FORMAT_UID_BUFFER_SIZE];
  this->trigger(std::string(format_uid_to(uid_buf, tag->get_uid())), *tag);
}

}  // namespace nfc
}  // namespace esphome
