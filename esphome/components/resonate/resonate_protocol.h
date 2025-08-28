#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF)

#ifdef USE_AUDIO
#include "esphome/components/audio/audio.h"
#endif

#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace resonate {

#ifdef USE_RESONATE_AUDIO
enum class ResonateCodecFormat {
  FLAC,
  OPUS,
  PCM,
  UNSUPPORTED,
};
#endif

#ifdef USE_RESONATE_IMAGE
// Format numbers not currently in spec
enum ResonateImageFormat : uint8_t {
  RESONATE_IMAGE_BMP = 0,
  RESONATE_IMAGE_JPG = 1,
  RESONATE_IMAGE_PNG = 2,
};
#endif

enum ResonateBinaryType : uint8_t {
  RESONATE_AUDIO_BINARY = 1,
  RESONATE_IMAGE_BINARY = 2,
};

enum class ResonateServerToPlayerMessageType {
  UNKNOWN,
  SERVER_HELLO,
  SERVER_TIME,
  SESSION_START,
  SESSION_END,
  METADATA_UPDATE,
  GROUP_LIST,
  VOLUME_SET,
  MUTE_SET,
};

enum class ResonatePlayerToServerMessageTypes {
  HELLO,
  TIME,
  STREAM_COMMAND,
};

struct ClientHelloMessage {
  std::string client_id;
  std::string name;
  std::vector<std::string> support_codecs;
  std::vector<uint8_t> support_channels;
  std::vector<uint32_t> support_sample_rates;
  std::vector<uint8_t> support_bit_depth;
  size_t buffer_capacity;
  std::vector<std::string> support_streams;
  std::vector<std::string> support_pictures_formats;
  std::string media_display_size;
};

struct PlayerStateMessage {
  std::string state;  // Playing, paused, idle TODO: String or enum?
  uint8_t volume;     // 0-100
  bool muted;
};

struct TimeTransmittedReplacement {
  int64_t transmitted_time = 0;
  int64_t actual_transmit_time = 0;
};

#ifdef USE_RESONATE_METADATA
struct ResonateMetadata {
  std::string title;
  std::string artist;
  std::string album;
  uint16_t year;
  uint16_t track;
};
#endif

ResonateServerToPlayerMessageType determine_message_type(const std::string &message);

bool process_server_hello_message(const std::string &message, std::string *server_id, std::string *server_name);
bool process_server_time_message(const std::string &message, int64_t timestamp,
                                 TimeTransmittedReplacement time_replacement, int64_t *offset, int64_t *max_error);

#ifdef USE_RESONATE_AUDIO
bool process_session_start_message(const std::string &message, audio::AudioStreamInfo *stream_info,
                                   ResonateCodecFormat *codec_format, std::string *codec_header);
#endif

bool process_mute_set_message(const std::string &message, bool *is_muted);
bool process_volume_set_message(const std::string &message, uint8_t *volume);

#ifdef USE_RESONATE_METADATA
bool process_metadata_update_message(const std::string &message, ResonateMetadata *metadata);
#endif

/// @brief Formats a client hello message as a JSON string for sending to the server.
/// @param msg (ClientHelloMessage *) Message to serialize
/// @return (std::string) Hello message serialized into JSON format
std::string format_player_hello_message(const ClientHelloMessage *msg);

std::string format_player_state_message(const PlayerStateMessage *msg);

#ifdef USE_MEDIA_PLAYER
std::string format_stream_command_message(const media_player::MediaPlayerCall &call);
#endif

PlayerStateMessage build_player_state_message(std::string state, uint8_t volume, bool muted);

}  // namespace resonate
}  // namespace esphome

#endif
