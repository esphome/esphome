#include "automation.h"
#include "nfc.h"

namespace esphome {
namespace nfc {

void NfcOnTagTrigger::process(const std::unique_ptr<NfcTag> &tag) {
  char uid_buf[FORMAT_UID_BUFFER_SIZE];
  format_uid_to(uid_buf, tag->get_uid());
  this->trigger(std::string(uid_buf), *tag);
}

}  // namespace nfc
}  // namespace esphome
