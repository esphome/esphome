#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO) && !defined(USE_SPEAKER_AUDIO_PIPELINE_BRIDGE)
#define USE_SPEAKER_AUDIO_PIPELINE_BRIDGE

// Remap the local TAG symbol before including the upstream source to avoid
// duplicate definitions when this bridge compiles alongside other media
// player translation units.
#define TAG SPEAKER_AUDIO_PIPELINE_BRIDGE_LOG_TAG
#include "speaker/media_player/audio_pipeline.cpp"
#undef TAG

namespace esphome {
namespace usb_audio {}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO) && !defined(USE_SPEAKER_AUDIO_PIPELINE_BRIDGE)
