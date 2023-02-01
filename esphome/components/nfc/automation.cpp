#include "automation.h"

namespace esphome {
namespace nfc {

void NfcOnTagTrigger::process(const std::unique_ptr<NfcTag> &tag) { this->trigger(format_uid(tag->get_uid()), *tag); }

}  // namespace nfc
}  // namespace esphome
