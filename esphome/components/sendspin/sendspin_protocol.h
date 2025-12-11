#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF)

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// TODO: don't always enable
#define USE_SENDSPIN_CONTROLLER

namespace esphome {
namespace sendspin {

#ifdef USE_SENDSPIN_PLAYER
enum class SendspinCodecFormat {
  FLAC,
  OPUS,
  PCM,
  UNSUPPORTED,
};

inline const char *to_string(SendspinCodecFormat format) {
  switch (format) {
    case SendspinCodecFormat::FLAC:
      return "flac";
    case SendspinCodecFormat::OPUS:
      return "opus";
    case SendspinCodecFormat::PCM:
      return "pcm";
    default:
      return "unsupported";
  }
}

inline std::optional<SendspinCodecFormat> codec_format_from_string(const std::string &str) {
  if (str == "flac")
    return SendspinCodecFormat::FLAC;
  if (str == "opus")
    return SendspinCodecFormat::OPUS;
  if (str == "pcm")
    return SendspinCodecFormat::PCM;
  return std::nullopt;
}

enum class SendspinPlayerState {
  SYNCHRONIZED,
  ERROR,
};

inline const char *to_string(SendspinPlayerState state) {
  switch (state) {
    case SendspinPlayerState::SYNCHRONIZED:
      return "synchronized";
    case SendspinPlayerState::ERROR:
    default:
      return "error";
  }
}

struct AudioSupportedFormatObject {
  SendspinCodecFormat codec;
  uint8_t channels;
  uint32_t sample_rate;
  uint8_t bit_depth;
};

enum class SendspinPlayerCommand {
  VOLUME,
  MUTE,
};

inline const char *to_string(SendspinPlayerCommand cmd) {
  switch (cmd) {
    case SendspinPlayerCommand::VOLUME:
      return "volume";
    case SendspinPlayerCommand::MUTE:
      return "mute";
    default:
      return "unknown";
  }
}

struct PlayerSupportObject {
  std::vector<AudioSupportedFormatObject> supported_formats;
  size_t buffer_capacity;
  std::vector<SendspinPlayerCommand> supported_commands;
};

struct ClientPlayerStateObject {
  SendspinPlayerState state;
  uint8_t volume;
  bool muted;
};

struct ServerPlayerStreamObject {
  std::optional<SendspinCodecFormat> codec;
  std::optional<uint32_t> sample_rate;
  std::optional<uint8_t> channels;
  std::optional<uint8_t> bit_depth;
  std::optional<std::string> codec_header;

  bool is_complete() const {
    return codec.has_value() && sample_rate.has_value() && channels.has_value() && bit_depth.has_value();
  }
};

#endif  // USE_SENDSPIN_PLAYER

#ifdef USE_SENDSPIN_ARTWORK
enum class SendspinImageFormat {
  JPEG,
  PNG,
  BMP,
};

inline const char *to_string(SendspinImageFormat format) {
  switch (format) {
    case SendspinImageFormat::JPEG:
      return "jpeg";
    case SendspinImageFormat::PNG:
      return "png";
    case SendspinImageFormat::BMP:
      return "bmp";
    default:
      return "jpeg";
  }
}

inline std::optional<SendspinImageFormat> image_format_from_string(const std::string &str) {
  if (str == "jpeg")
    return SendspinImageFormat::JPEG;
  if (str == "png")
    return SendspinImageFormat::PNG;
  if (str == "bmp")
    return SendspinImageFormat::BMP;
  return std::nullopt;
}

enum class SendspinImageSource {
  ALBUM,
  ARTIST,
  NONE,
};

inline const char *to_string(SendspinImageSource source) {
  switch (source) {
    case SendspinImageSource::ALBUM:
      return "album";
    case SendspinImageSource::ARTIST:
      return "artist";
    case SendspinImageSource::NONE:
      return "none";
    default:
      return "none";
  }
}

inline std::optional<SendspinImageSource> image_source_from_string(const std::string &str) {
  if (str == "album")
    return SendspinImageSource::ALBUM;
  if (str == "artist")
    return SendspinImageSource::ARTIST;
  if (str == "none")
    return SendspinImageSource::NONE;
  return std::nullopt;
}

struct ArtworkChannelFormatObject {
  SendspinImageSource source;
  SendspinImageFormat format;
  uint16_t media_width;
  uint16_t media_height;
};

struct ArtworkSupportObject {
  std::vector<ArtworkChannelFormatObject> channels;
};

struct ServerArtworkChannelObject {
  std::optional<SendspinImageSource> source;
  std::optional<SendspinImageFormat> format;
  std::optional<uint16_t> width;
  std::optional<uint16_t> height;

  bool is_complete() const {
    return source.has_value() && format.has_value() && width.has_value() && height.has_value();
  }
};

struct ServerArtworkStreamObject {
  std::optional<std::vector<ServerArtworkChannelObject>> channels;
};

struct ClientArtworkRequestObject {
  uint8_t channel;
  std::optional<SendspinImageSource> source;
  std::optional<SendspinImageFormat> format;
  std::optional<uint16_t> media_width;
  std::optional<uint16_t> media_height;
};
#endif  // USE_SENDSPIN_ARTWORK

#ifdef USE_SENDSPIN_VISUALIZER
struct VisualizerSupportObject {
  size_t buffer_capacity;
  // TODO: FFT details (to be determined in spec)
};
#endif

#ifdef USE_SENDSPIN_CONTROLLER
enum class SendspinCommandType {
  PLAY,
  PAUSE,
  STOP,
  NEXT,
  PREVIOUS,
  VOLUME,
  MUTE,
  REPEAT_OFF,
  REPEAT_ONE,
  REPEAT_ALL,
  SHUFFLE,
  UNSHUFFLE,
  SWITCH,
};

inline const char *to_string(SendspinCommandType cmd) {
  switch (cmd) {
    case SendspinCommandType::PLAY:
      return "play";
    case SendspinCommandType::PAUSE:
      return "pause";
    case SendspinCommandType::STOP:
      return "stop";
    case SendspinCommandType::NEXT:
      return "next";
    case SendspinCommandType::PREVIOUS:
      return "previous";
    case SendspinCommandType::VOLUME:
      return "volume";
    case SendspinCommandType::MUTE:
      return "mute";
    case SendspinCommandType::REPEAT_OFF:
      return "repeat_off";
    case SendspinCommandType::REPEAT_ONE:
      return "repeat_one";
    case SendspinCommandType::REPEAT_ALL:
      return "repeat_all";
    case SendspinCommandType::SHUFFLE:
      return "shuffle";
    case SendspinCommandType::UNSHUFFLE:
      return "unshuffle";
    case SendspinCommandType::SWITCH:
      return "switch";
    default:
      return "unknown";
  }
}

inline std::optional<SendspinCommandType> command_type_from_string(const std::string &str) {
  if (str == "play")
    return SendspinCommandType::PLAY;
  if (str == "pause")
    return SendspinCommandType::PAUSE;
  if (str == "stop")
    return SendspinCommandType::STOP;
  if (str == "next")
    return SendspinCommandType::NEXT;
  if (str == "previous")
    return SendspinCommandType::PREVIOUS;
  if (str == "volume")
    return SendspinCommandType::VOLUME;
  if (str == "mute")
    return SendspinCommandType::MUTE;
  if (str == "repeat_off")
    return SendspinCommandType::REPEAT_OFF;
  if (str == "repeat_one")
    return SendspinCommandType::REPEAT_ONE;
  if (str == "repeat_all")
    return SendspinCommandType::REPEAT_ALL;
  if (str == "shuffle")
    return SendspinCommandType::SHUFFLE;
  if (str == "unshuffle")
    return SendspinCommandType::UNSHUFFLE;
  if (str == "switch")
    return SendspinCommandType::SWITCH;
  return std::nullopt;
}

struct ClientCommandControllerObject {
  SendspinCommandType command;
  std::optional<uint8_t> volume;
  std::optional<bool> mute;
};

struct ServerStateControllerObject {
  std::vector<SendspinCommandType> supported_commands;
  uint8_t volume;
  bool muted;
};
#endif

#ifdef USE_SENDSPIN_METADATA
struct MetadataProgressObject {
  uint32_t track_progress;
  uint32_t track_duration;
  uint32_t playback_speed;
};

enum class SendspinRepeatMode {
  OFF,
  ONE,
  ALL,
};

inline const char *to_string(SendspinRepeatMode mode) {
  switch (mode) {
    case SendspinRepeatMode::OFF:
      return "off";
    case SendspinRepeatMode::ONE:
      return "one";
    case SendspinRepeatMode::ALL:
      return "all";
    default:
      return "off";
  }
}

inline std::optional<SendspinRepeatMode> repeat_mode_from_string(const std::string &str) {
  if (str == "off")
    return SendspinRepeatMode::OFF;
  if (str == "one")
    return SendspinRepeatMode::ONE;
  if (str == "all")
    return SendspinRepeatMode::ALL;
  return std::nullopt;
}

struct ServerMetadataStateObject {
  int64_t timestamp;
  std::optional<std::string> title;
  std::optional<std::string> artist;
  std::optional<std::string> album_artist;
  std::optional<std::string> album;
  std::optional<std::string> artwork_url;
  std::optional<uint16_t> year;
  std::optional<uint16_t> track;
  std::optional<MetadataProgressObject> progress;
  std::optional<SendspinRepeatMode> repeat;
  std::optional<bool> shuffle;
};
#endif  // USE_SENDSPIN_METADATA

// Binary message ID structure:
// Typically bits 7-2 for role type, bits 1-0 for message slot (4 IDs per role)
// Roles with expanded allocations use bits 2-0 for message slot (8 IDs)
enum SendspinBinaryRole : uint8_t {
  SENDSPIN_ROLE_PLAYER = 1,      // 000001xx (4-7)
  SENDSPIN_ROLE_ARTWORK = 2,     // 000010xx (8-11)
  SENDSPIN_ROLE_VISUALIZER = 4,  // 00010xxx (16-23) - expanded allocation
};

// Helper to extract role from binary message type (for standard 4-slot roles)
inline uint8_t get_binary_role(uint8_t type) { return type >> 2; }
// Helper to extract slot from binary message type (for standard 4-slot roles)
inline uint8_t get_binary_slot(uint8_t type) { return type & 0x03; }

// Common binary message types
enum SendspinBinaryType : uint8_t {
  SENDSPIN_BINARY_PLAYER_AUDIO = 4,   // Player slot 0
  SENDSPIN_BINARY_ARTWORK_IMAGE = 8,  // Artwork slot 0
  SENDSPIN_BINARY_VISUALIZER = 16,    // Visualizer slot 0
};

enum class SendspinServerToClientMessageType {
  SERVER_HELLO,
  SERVER_TIME,
  SERVER_STATE,
  SERVER_COMMAND,
  STREAM_START,
  STREAM_END,
  STREAM_CLEAR,
  GROUP_UPDATE,
  UNKNOWN,
};

enum class SendspinClientToServerMessageType {
  CLIENT_HELLO,
  CLIENT_TIME,
  CLIENT_STATE,
  CLIENT_COMMAND,
  STREAM_REQUEST_FORMAT,
  CLIENT_GOODBYE,
};

enum class SendspinRole {
  PLAYER,
  CONTROLLER,
  METADATA,
  ARTWORK,
  VISUALIZER,
};

inline const char *to_string(SendspinRole role) {
  switch (role) {
    case SendspinRole::PLAYER:
      return "player@v1";
    case SendspinRole::CONTROLLER:
      return "controller@v1";
    case SendspinRole::METADATA:
      return "metadata@v1";
    case SendspinRole::ARTWORK:
      return "artwork@v1";
    case SendspinRole::VISUALIZER:
      return "visualizer@v1";
    default:
      return "unknown";
  }
}

enum class SendspinConnectionReason {
  DISCOVERY,
  PLAYBACK,
};

inline const char *to_string(SendspinConnectionReason reason) {
  switch (reason) {
    case SendspinConnectionReason::DISCOVERY:
      return "discovery";
    case SendspinConnectionReason::PLAYBACK:
      return "playback";
    default:
      return "discovery";
  }
}

inline std::optional<SendspinConnectionReason> connection_reason_from_string(const std::string &str) {
  if (str == "discovery")
    return SendspinConnectionReason::DISCOVERY;
  if (str == "playback")
    return SendspinConnectionReason::PLAYBACK;
  return std::nullopt;
}

enum class SendspinGoodbyeReason {
  ANOTHER_SERVER,
  SHUTDOWN,
  RESTART,
  USER_REQUEST,
};

inline const char *to_string(SendspinGoodbyeReason reason) {
  switch (reason) {
    case SendspinGoodbyeReason::ANOTHER_SERVER:
      return "another_server";
    case SendspinGoodbyeReason::SHUTDOWN:
      return "shutdown";
    case SendspinGoodbyeReason::RESTART:
      return "restart";
    case SendspinGoodbyeReason::USER_REQUEST:
      return "user_request";
    default:
      return "shutdown";
  }
}

inline std::optional<SendspinGoodbyeReason> goodbye_reason_from_string(const std::string &str) {
  if (str == "another_server")
    return SendspinGoodbyeReason::ANOTHER_SERVER;
  if (str == "shutdown")
    return SendspinGoodbyeReason::SHUTDOWN;
  if (str == "restart")
    return SendspinGoodbyeReason::RESTART;
  if (str == "user_request")
    return SendspinGoodbyeReason::USER_REQUEST;
  return std::nullopt;
}

struct DeviceInfoObject {
  std::optional<std::string> product_name;
  std::optional<std::string> manufacturer;
  std::optional<std::string> software_version;
};

struct ClientHelloMessage {
  std::string client_id;
  std::string name;
  std::optional<DeviceInfoObject> device_info;
  uint8_t version;
  std::vector<SendspinRole> supported_roles;
#ifdef USE_SENDSPIN_PLAYER
  std::optional<PlayerSupportObject> player_v1_support;
#endif
#ifdef USE_SENDSPIN_ARTWORK
  std::optional<ArtworkSupportObject> artwork_v1_support;
#endif
#ifdef USE_SENDSPIN_VISUALIZER
  std::optional<VisualizerSupportObject> visualizer_v1_support;
#endif
};

struct ClientStateMessage {
#ifdef USE_SENDSPIN_PLAYER
  std::optional<ClientPlayerStateObject> player;
#endif
};

struct ClientCommandMessage {
#ifdef USE_SENDSPIN_CONTROLLER
  std::optional<ClientCommandControllerObject> controller;
#endif
};

struct ClientGoodbyeMessage {
  SendspinGoodbyeReason reason;
};

struct ServerStateMessage {
#ifdef USE_SENDSPIN_CONTROLLER
  std::optional<ServerStateControllerObject> controller;
#endif
#ifdef USE_SENDSPIN_METADATA
  std::optional<ServerMetadataStateObject> metadata;
#endif
};

struct ServerInformationObject {
  std::string server_id;
  std::string name;
};

struct ServerHelloMessage {
  ServerInformationObject server;
  uint16_t version;
  std::vector<std::string> active_roles;
  SendspinConnectionReason connection_reason;
};

enum class SendspinPlaybackState {
  PLAYING,
  PAUSED,
  STOPPED,
};

inline const char *to_string(SendspinPlaybackState state) {
  switch (state) {
    case SendspinPlaybackState::PLAYING:
      return "playing";
    case SendspinPlaybackState::PAUSED:
      return "paused";
    case SendspinPlaybackState::STOPPED:
      return "stopped";
    default:
      return "stopped";
  }
}

inline std::optional<SendspinPlaybackState> playback_state_from_string(const std::string &str) {
  if (str == "playing")
    return SendspinPlaybackState::PLAYING;
  if (str == "paused")
    return SendspinPlaybackState::PAUSED;
  if (str == "stopped")
    return SendspinPlaybackState::STOPPED;
  return std::nullopt;
}

struct GroupUpdateObject {
  std::optional<SendspinPlaybackState> playback_state;
  std::optional<std::string> group_id;
  std::optional<std::string> group_name;
};

struct GroupUpdateMessage {
  GroupUpdateObject group;
};

struct StreamStartMessage {
#ifdef USE_SENDSPIN_PLAYER
  std::optional<ServerPlayerStreamObject> player;
#endif
#ifdef USE_SENDSPIN_ARTWORK
  std::optional<ServerArtworkStreamObject> artwork;
#endif
};

struct StreamRequestFormatMessage {
#ifdef USE_SENDSPIN_PLAYER
  std::optional<ServerPlayerStreamObject> player;
#endif
#ifdef USE_SENDSPIN_ARTWORK
  std::optional<ClientArtworkRequestObject> artwork;
#endif
};

struct StreamEndMessage {
  std::optional<std::vector<std::string>> roles;
};

struct StreamClearMessage {
  std::optional<std::vector<std::string>> roles;
};

struct ServerPlayerCommandObject {
  SendspinCommandType command;  // only volume and mute are possible
  std::optional<uint8_t> volume;
  std::optional<bool> mute;
};
struct ServerCommandMessage {
  std::optional<ServerPlayerCommandObject> player;
};

struct TimeTransmittedReplacement {
  int64_t transmitted_time = 0;
  int64_t actual_transmit_time = 0;
};

SendspinServerToClientMessageType determine_message_type(const std::string &message);

bool process_server_hello_message(const std::string &message, ServerHelloMessage *hello_msg);
bool process_server_time_message(const std::string &message, int64_t timestamp,
                                 TimeTransmittedReplacement time_replacement, int64_t *offset, int64_t *max_error);
bool process_group_update_message(const std::string &message, GroupUpdateMessage *group_msg);
void apply_group_update_deltas(GroupUpdateObject *current, const GroupUpdateObject &updates);

bool process_server_command_message(const std::string &message, ServerCommandMessage *cmd_msg);
bool process_server_state_message(const std::string &message, ServerStateMessage *state_msg);

bool process_stream_start_message(const std::string &message, StreamStartMessage *stream_msg);
bool process_stream_end_message(const std::string &message, StreamEndMessage *end_msg);
bool process_stream_clear_message(const std::string &message, StreamClearMessage *clear_msg);

#ifdef USE_SENDSPIN_METADATA
void apply_metadata_state_deltas(ServerMetadataStateObject *current, const ServerMetadataStateObject &updates);
#endif

/// @brief Formats a client hello message as a JSON string for sending to the server.
/// @param msg (ClientHelloMessage *) Message to serialize
/// @return (std::string) Hello message serialized into JSON format
std::string format_client_hello_message(const ClientHelloMessage *msg);

std::string format_client_state_message(const ClientStateMessage *msg);

std::string format_stream_request_format_message(const StreamRequestFormatMessage *msg);

std::string format_client_goodbye_message(SendspinGoodbyeReason reason);

#ifdef USE_SENDSPIN_CONTROLLER
std::string format_client_command_message(SendspinCommandType command, std::optional<uint8_t> volume = std::nullopt,
                                          std::optional<bool> mute = std::nullopt);
#endif

}  // namespace sendspin
}  // namespace esphome

#endif
