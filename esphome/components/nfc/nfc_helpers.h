#pragma once

#include "nfc_tag.h"

namespace esphome {
namespace nfc {

static const char HA_TAG_ID_EXT_RECORD_TYPE[] = "android.com:pkg";
static const char HA_TAG_ID_EXT_RECORD_PAYLOAD[] = "io.homeassistant.companion.android";
static const char HA_TAG_ID_PREFIX[] = "https://www.home-assistant.io/tag/";

std::string get_ha_tag_ndef(NfcTag &tag);
std::string get_random_ha_tag_ndef();
bool has_ha_tag_ndef(NfcTag &tag);

}  // namespace nfc
}  // namespace esphome
