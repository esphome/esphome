#if defined(USE_ESP_IDF)

#include "resonate_protocol.h"

#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.protocol";

ResonateServerToClientMessageType determine_message_type(const std::string &message) {
  ResonateServerToClientMessageType type = ResonateServerToClientMessageType::UNKNOWN;
  if (json::parse_json(message, [&type](JsonObject root) -> bool {
        if (root["type"].is<JsonVariant>()) {
          if (root["type"].as<std::string>() == "server/hello") {
            type = ResonateServerToClientMessageType::SERVER_HELLO;
          } else if (root["type"].as<std::string>() == "server/time") {
            type = ResonateServerToClientMessageType::SERVER_TIME;
          } else if (root["type"].as<std::string>() == "stream/start") {
            type = ResonateServerToClientMessageType::STREAM_START;
          } else if (root["type"].as<std::string>() == "stream/update") {
            type = ResonateServerToClientMessageType::STREAM_UPDATE;
          } else if (root["type"].as<std::string>() == "stream/end") {
            type = ResonateServerToClientMessageType::STREAM_END;
          } else if (root["type"].as<std::string>() == "session/update") {
            type = ResonateServerToClientMessageType::SESSION_UPDATE;
          } else if (root["type"].as<std::string>() == "group/list") {
            type = ResonateServerToClientMessageType::GROUP_LIST;
          } else if (root["type"].as<std::string>() == "volume/set") {
            type = ResonateServerToClientMessageType::VOLUME_SET;
          } else if (root["type"].as<std::string>() == "mute/set") {
            type = ResonateServerToClientMessageType::MUTE_SET;
          }

          return true;
        }
        return false;
      })) {
    return type;
  }

  return ResonateServerToClientMessageType::UNKNOWN;
}

bool process_server_hello_message(const std::string &message, std::string *server_id, std::string *server_name) {
  return (json::parse_json(message, [&server_id, &server_name, message](JsonObject root) -> bool {
    if ((root["type"].as<std::string>() != "server/hello") || !root["payload"]["server_id"].is<JsonVariant>() ||
        !root["payload"]["name"].is<JsonVariant>() || !root["payload"]["version"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid server/hello message: %s", message.c_str());
      return false;
    }

    if (server_id != nullptr) {
      *server_id = root["payload"]["server_id"].as<std::string>();
    }
    if (server_name != nullptr) {
      *server_name = root["payload"]["name"].as<std::string>();
    }
    uint8_t version = root["payload"]["version"].as<uint8_t>();

    return true;
  }));
}

bool process_server_time_message(const std::string &message, int64_t timestamp,
                                 TimeTransmittedReplacement time_replacement, int64_t *offset, int64_t *max_error) {
  return (json::parse_json(message, [timestamp, time_replacement, offset, max_error](JsonObject root) -> bool {
    if ((root["type"].as<std::string>() != "server/time") || !root["payload"]["client_transmitted"].is<JsonVariant>() ||
        !root["payload"]["server_received"].is<JsonVariant>() ||
        !root["payload"]["server_transmitted"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid server/time message");
      return false;
    }

    int64_t client_transmitted = root["payload"]["client_transmitted"];

    if (client_transmitted == time_replacement.transmitted_time) {
      client_transmitted = time_replacement.actual_transmit_time;
    } else {
      ESP_LOGW(TAG, "Mismatched time message history");
    }

    const int64_t server_received = root["payload"]["server_received"];
    const int64_t server_transmitted = root["payload"]["server_transmitted"];
    const int64_t client_received = timestamp;

    if (offset != nullptr) {
      *offset = ((server_received - client_transmitted) + (server_transmitted - client_received)) / 2;
    }

    if (max_error != nullptr) {
      const int64_t delay = (client_received - client_transmitted) - (server_transmitted - server_received);
      *max_error = delay / 2;
    }

    return true;
  }));
}

#ifdef USE_RESONATE_AUDIO
bool process_stream_start_message(const std::string &message, audio::AudioStreamInfo *stream_info,
                                  ResonateCodecFormat *codec_format, std::string *codec_header) {
  return (json::parse_json(message, [stream_info, codec_format, &codec_header](JsonObject root) -> bool {
    if (!root["payload"]["player"].is<JsonObject>()) {
      ESP_LOGE(TAG, "Invalid stream/start message");
      return false;
    }

    return process_player_object_message(root["payload"]["player"], stream_info, codec_format, codec_header);

    // if (stream_info != nullptr) {
    //   const uint8_t bit_depth = root["payload"]["player"]["bit_depth"].as<uint8_t>();
    //   const uint8_t channels = root["payload"]["player"]["channels"].as<uint8_t>();
    //   const uint32_t sample_rate = root["payload"]["player"]["sample_rate"].as<uint32_t>();

    //   *stream_info = audio::AudioStreamInfo(bit_depth, channels, sample_rate);
    // }

    // if (codec_format != nullptr) {
    //   std::string codec_type = root["payload"]["player"]["codec"].as<std::string>();

    //   if (codec_type == "pcm") {
    //     *codec_format = ResonateCodecFormat::PCM;
    //   } else if (codec_type == "opus") {
    //     *codec_format = ResonateCodecFormat::OPUS;
    //   } else if (codec_type == "flac") {
    //     if (!root["payload"]["codec_header"].is<JsonVariant>()) {
    //       ESP_LOGE(TAG, "Invalid stream/start message");
    //       return false;
    //     }
    //     *codec_format = ResonateCodecFormat::FLAC;
    //     *codec_header = root["payload"]["player"]["codec_header"].as<std::string>();
    //   } else {
    //     *codec_format = ResonateCodecFormat::UNSUPPORTED;
    //   }
    // }
    // return true;
  }));
}
bool process_player_object_message(const JsonObject player_object, audio::AudioStreamInfo *stream_info,
                                   ResonateCodecFormat *codec_format, std::string *codec_header) {
  if (!player_object["bit_depth"].is<JsonVariant>() || !player_object["channels"].is<JsonVariant>() ||
      !player_object["sample_rate"].is<JsonVariant>() || !player_object["codec"].is<JsonVariant>()) {
    ESP_LOGE(TAG, "Invalid player object");
    return false;
  }

  if (stream_info != nullptr) {
    const uint8_t bit_depth = player_object["bit_depth"].as<uint8_t>();
    const uint8_t channels = player_object["channels"].as<uint8_t>();
    const uint32_t sample_rate = player_object["sample_rate"].as<uint32_t>();

    *stream_info = audio::AudioStreamInfo(bit_depth, channels, sample_rate);
  }

  if (codec_format != nullptr) {
    std::string codec_type = player_object["codec"].as<std::string>();

    if (codec_type == "pcm") {
      *codec_format = ResonateCodecFormat::PCM;
    } else if (codec_type == "opus") {
      *codec_format = ResonateCodecFormat::OPUS;
    } else if (codec_type == "flac") {
      if (!player_object["codec_header"].is<JsonVariant>()) {
        ESP_LOGE(TAG, "Invalid player object");
        return false;
      }
      *codec_format = ResonateCodecFormat::FLAC;
      *codec_header = player_object["codec_header"].as<std::string>();
    } else {
      *codec_format = ResonateCodecFormat::UNSUPPORTED;
    }
  }
  return true;

  // return (json::parse_json(message, [stream_info, codec_format, &codec_header](JsonObject root) -> bool {
  //   if (!root["bit_depth"].is<JsonVariant>() || !root["channels"].is<JsonVariant>() ||
  //       !root["sample_rate"].is<JsonVariant>() || !root["codec"].is<JsonVariant>()) {
  //     ESP_LOGE(TAG, "Invalid player object message");
  //     return false;
  //   }

  //   if (stream_info != nullptr) {
  //     const uint8_t bit_depth = root["bit_depth"].as<uint8_t>();
  //     const uint8_t channels = root["channels"].as<uint8_t>();
  //     const uint32_t sample_rate = root["sample_rate"].as<uint32_t>();

  //     *stream_info = audio::AudioStreamInfo(bit_depth, channels, sample_rate);
  //   }

  //   if (codec_format != nullptr) {
  //     std::string codec_type = root["codec"].as<std::string>();

  //     if (codec_type == "pcm") {
  //       *codec_format = ResonateCodecFormat::PCM;
  //     } else if (codec_type == "opus") {
  //       *codec_format = ResonateCodecFormat::OPUS;
  //     } else if (codec_type == "flac") {
  //       if (!root["codec_header"].is<JsonVariant>()) {
  //         ESP_LOGE(TAG, "Invalid player object");
  //         return false;
  //       }
  //       *codec_format = ResonateCodecFormat::FLAC;
  //       *codec_header = root["codec_header"].as<std::string>();
  //     } else {
  //       *codec_format = ResonateCodecFormat::UNSUPPORTED;
  //     }
  //   }
  //   return true;
  // }));
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
      *volume = root["payload"]["volume"].as<uint8_t>();
    }

    return true;
  }));
}

#ifdef USE_RESONATE_METADATA
bool process_session_update_message(const std::string &message, ResonateMetadata *metadata) {
  return (json::parse_json(message, [&metadata](JsonObject root) -> bool {
    if (root["type"].as<std::string>() != "session/update") {
      ESP_LOGE(TAG, "Invalid session/update message");
      return false;
    }

    if (metadata != nullptr) {
      if (root["payload"]["metadata"]["title"].is<const char *>()) {
        metadata->title = root["payload"]["metadata"]["title"].as<std::string>();
      }
      if (root["payload"]["metadata"]["artist"].is<const char *>()) {
        metadata->artist = root["payload"]["metadata"]["artist"].as<std::string>();
      }
      if (root["payload"]["metadata"]["album"].is<const char *>()) {
        metadata->album = root["payload"]["metadata"]["album"].as<std::string>();
      }
      if (root["payload"]["metadata"]["year"].is<uint16_t>()) {
        metadata->year = root["payload"]["metadata"]["year"].as<uint16_t>();
      }
      if (root["payload"]["metadata"]["track"].is<uint16_t>()) {
        metadata->track = root["payload"]["metadata"]["track"].as<uint16_t>();
      }
    }

    return true;
  }));
}
#endif

std::string format_player_hello_message(const ClientHelloMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "client/hello";
    root["payload"]["client_id"] = msg->client_id;
    root["payload"]["name"] = msg->name;
    root["payload"]["version"] = msg->version;
    JsonArray supported_roles_list = root["payload"]["supported_roles"].to<JsonArray>();
    for (const std::string &role : msg->supported_roles) {
      supported_roles_list.add(role);
    }
    if (msg->player_support.has_value()) {
      JsonArray codec_list = root["payload"]["player_support"]["support_codecs"].to<JsonArray>();
      for (const std::string &codec : msg->player_support.value().support_codecs) {
        codec_list.add(codec);
      }
      JsonArray bit_depth_list = root["payload"]["player_support"]["support_bit_depth"].to<JsonArray>();
      for (const uint8_t &bit_depth : msg->player_support.value().support_bit_depth) {
        bit_depth_list.add(bit_depth);
      }
      JsonArray channels_list = root["payload"]["player_support"]["support_channels"].to<JsonArray>();
      for (const uint8_t &channel : msg->player_support.value().support_channels) {
        channels_list.add(channel);
      }
      JsonArray sample_rates_list = root["payload"]["player_support"]["support_sample_rates"].to<JsonArray>();
      for (const uint32_t &sample_rate : msg->player_support.value().support_sample_rates) {
        sample_rates_list.add(sample_rate);
      }
      root["payload"]["player_support"]["buffer_capacity"] = msg->player_support.value().buffer_capacity;
    }
    if (msg->metadata_support.has_value()) {
      JsonArray support_picture_formats =
          root["payload"]["metadata_support"]["support_picture_formats"].to<JsonArray>();
      for (const std::string &picture_format : msg->metadata_support.value().support_picture_formats) {
        support_picture_formats.add(picture_format);
      }
      if (msg->metadata_support.value().media_width.has_value()) {
        root["payload"]["metadata_support"]["media_width"] = msg->metadata_support.value().media_width.value();
      }
      if (msg->metadata_support.value().media_height.has_value()) {
        root["payload"]["metadata_support"]["media_height"] = msg->metadata_support.value().media_height.value();
      }
    }
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

std::string format_player_update_message(const PlayerUpdateMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "player/update";
    root["payload"]["state"] = msg->state;
    root["payload"]["volume"] = msg->volume;
    root["payload"]["muted"] = msg->muted;
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

#ifdef USE_MEDIA_PLAYER
std::string format_group_command_message(const media_player::MediaPlayerCommand &command) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([command](JsonObject root) {
    root["type"] = "group/command";
    switch (command) {
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
        // TODO: Switching to the media player command directly means we can't send volume this way
        // } else if (call.get_volume().has_value()) {
        //   // TODO: This is a float operation!
        //   root["payload"]["volume"] = call.get_volume().value();
    }
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
#endif

PlayerUpdateMessage build_player_state_message(std::string state, uint8_t volume, bool muted) {
  return {.state = std::move(state), .volume = volume, .muted = muted};
}

}  // namespace resonate
}  // namespace esphome

#endif
