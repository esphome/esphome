#if defined(USE_ESP_IDF)

#include "resonate_protocol.h"

#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.protocol";

ResonateServerToPlayerMessageType determine_message_type(const std::string &message) {
  ResonateServerToPlayerMessageType type = ResonateServerToPlayerMessageType::UNKNOWN;
  if (json::parse_json(message, [&type](JsonObject root) -> bool {
        if (root["type"].is<JsonVariant>()) {
          if (root["type"].as<std::string>() == "source/hello") {
            type = ResonateServerToPlayerMessageType::SERVER_HELLO;
          } else if (root["type"].as<std::string>() == "source/time") {
            type = ResonateServerToPlayerMessageType::SERVER_TIME;
          } else if (root["type"].as<std::string>() == "session/start") {
            type = ResonateServerToPlayerMessageType::SESSION_START;
          } else if (root["type"].as<std::string>() == "session/end") {
            type = ResonateServerToPlayerMessageType::SESSION_END;
          } else if (root["type"].as<std::string>() == "metadata/update") {
            type = ResonateServerToPlayerMessageType::METADATA_UPDATE;
          } else if (root["type"].as<std::string>() == "group/list") {
            type = ResonateServerToPlayerMessageType::GROUP_LIST;
          } else if (root["type"].as<std::string>() == "volume/set") {
            type = ResonateServerToPlayerMessageType::VOLUME_SET;
          } else if (root["type"].as<std::string>() == "mute/set") {
            type = ResonateServerToPlayerMessageType::MUTE_SET;
          }

          return true;
        }
        return false;
      })) {
    return type;
  }

  return ResonateServerToPlayerMessageType::UNKNOWN;
}

bool process_server_hello_message(const std::string &message, std::string *server_id, std::string *server_name) {
  return (json::parse_json(message, [&server_id, &server_name](JsonObject root) -> bool {
    if ((root["type"].as<std::string>() != "source/hello") || !root["payload"]["source_id"].is<JsonVariant>() ||
        !root["payload"]["name"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid server/hello message");
      return false;
    }

    if (server_id != nullptr) {
      *server_id = root["payload"]["source_id"].as<std::string>();
    }
    if (server_name != nullptr) {
      *server_name = root["payload"]["name"].as<std::string>();
    }

    return true;
  }));
}

bool process_server_time_message(const std::string &message, int64_t timestamp,
                                 TimeTransmittedReplacement time_replacement, int64_t *offset, int64_t *max_error) {
  return (json::parse_json(message, [timestamp, time_replacement, offset, max_error](JsonObject root) -> bool {
    if ((root["type"].as<std::string>() != "source/time") || !root["payload"]["player_transmitted"].is<JsonVariant>() ||
        !root["payload"]["source_received"].is<JsonVariant>() ||
        !root["payload"]["source_transmitted"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid server/time message");
      return false;
    }

    int64_t player_transmitted = root["payload"]["player_transmitted"];

    if (player_transmitted == time_replacement.transmitted_time) {
      player_transmitted = time_replacement.actual_transmit_time;
    } else {
      ESP_LOGW(TAG, "Mismatched time message history");
    }

    const int64_t source_received = root["payload"]["source_received"];
    const int64_t source_transmitted = root["payload"]["source_transmitted"];
    const int64_t player_received = timestamp;

    if (offset != nullptr) {
      *offset = ((source_received - player_transmitted) + (source_transmitted - player_received)) / 2;
    }

    if (max_error != nullptr) {
      const int64_t delay = (player_received - player_transmitted) - (source_transmitted - source_received);
      *max_error = delay / 2;
    }

    return true;
  }));
}

#ifdef USE_AUDIO
bool process_session_start_message(const std::string &message, audio::AudioStreamInfo *stream_info,
                                   ResonateCodecFormat *codec_format, std::string *codec_header) {
  return (json::parse_json(message, [stream_info, codec_format, &codec_header](JsonObject root) -> bool {
    if (!root["payload"]["bit_depth"].is<JsonVariant>() || !root["payload"]["channels"].is<JsonVariant>() ||
        !root["payload"]["sample_rate"].is<JsonVariant>() || !root["payload"]["codec"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid session/start message");
      return false;
    }

    if (stream_info != nullptr) {
      const uint8_t bit_depth = root["payload"]["bit_depth"].as<uint8_t>();
      const uint8_t channels = root["payload"]["channels"].as<uint8_t>();
      const uint32_t sample_rate = root["payload"]["sample_rate"].as<uint32_t>();

      *stream_info = audio::AudioStreamInfo(bit_depth, channels, sample_rate);
    }

    if (codec_format != nullptr) {
      std::string codec_type = root["payload"]["codec"].as<std::string>();

      if (codec_type == "pcm") {
        *codec_format = ResonateCodecFormat::RESONATE_CODEC_PCM;
      } else if (codec_type == "opus") {
        *codec_format = ResonateCodecFormat::RESONATE_CODEC_OPUS;
      } else if (codec_type == "flac") {
        if (!root["payload"]["codec_header"].is<JsonVariant>()) {
          ESP_LOGE(TAG, "Invalid session/start message");
          return false;
        }
        *codec_format = ResonateCodecFormat::RESONATE_CODEC_FLAC;
        *codec_header = root["payload"]["codec_header"].as<std::string>();
      } else {
        *codec_format = ResonateCodecFormat::RESONATE_CODEC_UNSUPPORTED;
      }
    }
    return true;
  }));
}
#endif

bool process_mute_set_message(const std::string &message, bool *is_muted) {
  return (json::parse_json(message, [is_muted](JsonObject root) -> bool {
    if ((root["type"].as<std::string>() != "mute/set") || !root["payload"]["mute"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid mute/set message");
      return false;
    }

    if (is_muted != nullptr) {
      *is_muted = root["payload"]["mute"].as<bool>();
    }

    return true;
  }));
}

bool process_volume_set_message(const std::string &message, uint8_t *volume) {
  return (json::parse_json(message, [volume](JsonObject root) -> bool {
    if ((root["type"].as<std::string>() != "volume/set") || !root["payload"]["volume"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid volume/set message");
      return false;
    }

    if (volume != nullptr) {
      *volume = root["payload"]["volume"].as<bool>();
    }

    return true;
  }));
}

#ifdef USE_RESONATE_METADATA
bool process_metadata_update_message(const std::string &message, ResonateMetadata *metadata) {
  return (json::parse_json(message, [metadata](JsonObject root) -> bool {
    if (root["type"].as<std::string>() != "metadata/update") {
      ESP_LOGE(TAG, "Invalid metadata/update message");
      return false;
    }

    if (metadata != nullptr) {
      if (!root["payload"]["title"].is<const char *>()) {
        metadata->title = root["payload"]["title"].as<std::string>();
      }
      if (!root["payload"]["artist"].is<const char *>()) {
        metadata->artist = root["payload"]["artist"].as<std::string>();
      }
      if (!root["payload"]["album"].is<const char *>()) {
        metadata->album = root["payload"]["album"].as<std::string>();
      }
      if (!root["payload"]["year"].is<uint16_t>()) {
        metadata->year = root["payload"]["year"].as<uint16_t>();
      }
      if (!root["payload"]["track"].is<uint16_t>()) {
        metadata->track = root["payload"]["track"].as<uint16_t>();
      }
    }

    return true;
  }));
}
#endif

std::string format_player_hello_message(const PlayerHelloMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "player/hello";
    root["payload"]["player_id"] = msg->player_id;
    root["payload"]["name"] = msg->name;
    root["payload"]["role"] = "play";
    JsonArray codec_list = root["payload"]["support_codecs"].to<JsonArray>();
    for (const std::string &codec : msg->support_codecs) {
      codec_list.add(codec);
    }
    JsonArray channels_list = root["payload"]["support_channels"].to<JsonArray>();
    for (const uint8_t &channel : msg->support_channels) {
      channels_list.add(channel);
    }
    JsonArray sample_rates_list = root["payload"]["support_sample_rates"].to<JsonArray>();
    for (const uint32_t &sample_rate : msg->support_sample_rates) {
      sample_rates_list.add(sample_rate);
    }
    JsonArray bit_depth_list = root["payload"]["support_bit_depth"].to<JsonArray>();
    for (const uint8_t &bit_depth : msg->support_bit_depth) {
      bit_depth_list.add(bit_depth);
    }
    JsonArray streams_list = root["payload"]["support_streams"].to<JsonArray>();
    for (const std::string &stream : msg->support_streams) {
      streams_list.add(stream);
    }
    root["payload"]["buffer_capacity"] = msg->buffer_capacity;

    // TODO: Handle cover art
    root["payload"]["support_picture_formats"].to<JsonArray>();
    root["payload"]["media_display_size"] = serialized("null");
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

std::string format_player_state_message(const PlayerStateMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "player/state";
    root["payload"]["state"] = msg->state;
    root["payload"]["volume"] = msg->volume;
    root["payload"]["muted"] = msg->muted;
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

#ifdef USE_MEDIA_PLAYER
std::string format_stream_command_message(const media_player::MediaPlayerCall &call) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([call](JsonObject root) {
    root["type"] = "stream/command";
    if (call.get_command().has_value()) {
      switch (call.get_command().value()) {
        case media_player::MEDIA_PLAYER_COMMAND_PLAY:
          root["payload"]["command"] = "play";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
          root["payload"]["command"] = "pause";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_STOP:
          root["payload"]["command"] = "stop";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_MUTE:
          root["payload"]["command"] = "mute";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
          root["payload"]["command"] = "unmute";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
          root["payload"]["command"] = "repeat_off";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
          root["payload"]["command"] = "repeat_one";
          break;
        case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
          root["payload"]["command"] = "clear_playlist";
          break;
        default:  // TODO: properly handle this? don't send anything
          break;
      }
    } else if (call.get_volume().has_value()) {
      // TODO: This is a float operation!
      root["payload"]["volume"] = call.get_volume().value();
    }
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
#endif

PlayerStateMessage build_player_state_message(std::string state, uint8_t volume, bool muted) {
  return {.state = std::move(state), .volume = volume, .muted = muted};
}

}  // namespace resonate
}  // namespace esphome

#endif
