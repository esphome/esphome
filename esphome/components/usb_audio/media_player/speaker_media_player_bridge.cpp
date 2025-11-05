#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO) && !defined(USE_SPEAKER_MEDIA_PLAYER_BRIDGE)
#define USE_SPEAKER_MEDIA_PLAYER_BRIDGE

#include "speaker/media_player/speaker_media_player.cpp"

namespace esphome {
namespace usb_audio {}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO) && !defined(USE_SPEAKER_MEDIA_PLAYER_BRIDGE)
