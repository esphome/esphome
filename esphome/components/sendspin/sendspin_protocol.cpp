#include "sendspin_protocol.h"

#ifdef USE_ESP32

#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.protocol";

SendspinServerToClientMessageType determine_message_type(JsonObject root) {
  if (!root["type"].is<const char *>()) {
    return SendspinServerToClientMessageType::UNKNOWN;
  }

  const std::string type_str = root["type"].as<std::string>();
  if (type_str == "server/hello") {
    return SendspinServerToClientMessageType::SERVER_HELLO;
  } else if (type_str == "server/time") {
    return SendspinServerToClientMessageType::SERVER_TIME;
  } else if (type_str == "server/state") {
    return SendspinServerToClientMessageType::SERVER_STATE;
  } else if (type_str == "server/command") {
    return SendspinServerToClientMessageType::SERVER_COMMAND;
  } else if (type_str == "stream/start") {
    return SendspinServerToClientMessageType::STREAM_START;
  } else if (type_str == "stream/end") {
    return SendspinServerToClientMessageType::STREAM_END;
  } else if (type_str == "stream/clear") {
    return SendspinServerToClientMessageType::STREAM_CLEAR;
  } else if (type_str == "group/update") {
    return SendspinServerToClientMessageType::GROUP_UPDATE;
  }

  return SendspinServerToClientMessageType::UNKNOWN;
}

bool process_server_hello_message(JsonObject root, ServerHelloMessage *hello_msg) {
  if (!root["payload"]["server_id"].is<JsonVariant>() || !root["payload"]["name"].is<JsonVariant>() ||
      !root["payload"]["version"].is<JsonVariant>() || !root["payload"]["active_roles"].is<JsonVariant>() ||
      !root["payload"]["connection_reason"].is<const char *>()) {
    ESP_LOGE(TAG, "Invalid server/hello message");
    return false;
  }

  if (hello_msg != nullptr) {
    hello_msg->server.server_id = root["payload"]["server_id"].as<std::string>();
    hello_msg->server.name = root["payload"]["name"].as<std::string>();
    hello_msg->version = root["payload"]["version"].as<uint16_t>();

    // Parse active_roles array
    hello_msg->active_roles.clear();
    JsonArrayConst active_roles_array = root["payload"]["active_roles"].as<JsonArrayConst>();
    for (JsonVariantConst role_var : active_roles_array) {
      if (role_var.is<const char *>()) {
        hello_msg->active_roles.push_back(role_var.as<std::string>());
      }
    }

    auto reason = connection_reason_from_string(root["payload"]["connection_reason"].as<std::string>());
    if (!reason.has_value()) {
      ESP_LOGE(TAG, "Invalid connection_reason in server/hello message: %s",
               root["payload"]["connection_reason"].as<const char *>());
      return false;
    }
    hello_msg->connection_reason = reason.value();
  }

  return true;
}

bool process_server_time_message(JsonObject root, int64_t timestamp, TimeTransmittedReplacement time_replacement,
                                 int64_t *offset, int64_t *max_error) {
  if (!root["payload"]["client_transmitted"].is<JsonVariant>() ||
      !root["payload"]["server_received"].is<JsonVariant>() ||
      !root["payload"]["server_transmitted"].is<JsonVariant>()) {
    ESP_LOGE(TAG, "Invalid server/time message");
    return false;
  }

  int64_t client_transmitted = root["payload"]["client_transmitted"];

  if (client_transmitted != time_replacement.transmitted_time) {
    ESP_LOGW(TAG, "Mismatched time message history, discarding measurement");
    return false;
  }
  client_transmitted = time_replacement.actual_transmit_time;

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
}

bool process_group_update_message(JsonObject root, GroupUpdateMessage *group_msg) {
  if (group_msg == nullptr) {
    return true;
  }

  // Parse optional playback_state
  JsonVariantConst playback_state_var = root["payload"]["playback_state"];
  if (playback_state_var.is<const char *>()) {
    std::string state_str = playback_state_var.as<std::string>();
    group_msg->group.playback_state = playback_state_from_string(state_str);
  } else if (!playback_state_var.isUnbound() && playback_state_var.isNull()) {
    // Field set to null - clear from state
    group_msg->group.playback_state = std::nullopt;
  }

  // Parse optional group_id - use empty string to signal clearing
  JsonVariantConst group_id_var = root["payload"]["group_id"];
  if (group_id_var.is<const char *>()) {
    group_msg->group.group_id = group_id_var.as<std::string>();
  } else if (!group_id_var.isUnbound() && group_id_var.isNull()) {
    // Field set to null - use empty string to clear
    group_msg->group.group_id = "";
  }

  // Parse optional group_name - use empty string to signal clearing
  JsonVariantConst group_name_var = root["payload"]["group_name"];
  if (group_name_var.is<const char *>()) {
    group_msg->group.group_name = group_name_var.as<std::string>();
  } else if (!group_name_var.isUnbound() && group_name_var.isNull()) {
    // Field set to null - use empty string to clear
    group_msg->group.group_name = "";
  }

  return true;
}

void apply_group_update_deltas(GroupUpdateObject *current, const GroupUpdateObject &updates) {
  if (current == nullptr) {
    return;
  }

  // Update playback_state if present in the delta
  if (updates.playback_state.has_value()) {
    current->playback_state = updates.playback_state;
  }

  // Update group_id if present in the delta (including empty string for clearing)
  if (updates.group_id.has_value()) {
    current->group_id = updates.group_id;
  }

  // Update group_name if present in the delta (including empty string for clearing)
  if (updates.group_name.has_value()) {
    current->group_name = updates.group_name;
  }
}

#ifdef USE_SENDSPIN_PLAYER
static bool process_player_stream_object(const JsonObject player_object, ServerPlayerStreamObject *player_obj,
                                         bool require_all_fields) {
  if (player_obj == nullptr) {
    return false;
  }

  if (require_all_fields) {
    if (!player_object["bit_depth"].is<JsonVariant>() || !player_object["channels"].is<JsonVariant>() ||
        !player_object["sample_rate"].is<JsonVariant>() || !player_object["codec"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid player object: missing required fields");
      return false;
    }
  }

  if (player_object["codec"].is<JsonVariant>()) {
    std::string codec_type = player_object["codec"].as<std::string>();
    auto codec = codec_format_from_string(codec_type);
    player_obj->codec = codec.value_or(SendspinCodecFormat::UNSUPPORTED);
  }

  if (player_object["sample_rate"].is<JsonVariant>()) {
    player_obj->sample_rate = player_object["sample_rate"].as<uint32_t>();
  }

  if (player_object["channels"].is<JsonVariant>()) {
    player_obj->channels = player_object["channels"].as<uint8_t>();
  }

  if (player_object["bit_depth"].is<JsonVariant>()) {
    player_obj->bit_depth = player_object["bit_depth"].as<uint8_t>();
  }

  if (player_object["codec_header"].is<JsonVariant>()) {
    player_obj->codec_header = player_object["codec_header"].as<std::string>();
  }

  // For FLAC, codec_header is required
  if (player_obj->codec.has_value() && player_obj->codec.value() == SendspinCodecFormat::FLAC &&
      !player_obj->codec_header.has_value()) {
    ESP_LOGE(TAG, "Invalid player object: FLAC requires codec_header");
    return false;
  }

  return true;
}
#endif

#ifdef USE_SENDSPIN_ARTWORK
static bool process_artwork_channel_object(const JsonObject channel_object, ServerArtworkChannelObject *channel,
                                           bool require_all_fields) {
  if (channel == nullptr) {
    return false;
  }

  if (require_all_fields) {
    if (!channel_object["source"].is<JsonVariant>() || !channel_object["format"].is<JsonVariant>() ||
        !channel_object["width"].is<JsonVariant>() || !channel_object["height"].is<JsonVariant>()) {
      ESP_LOGE(TAG, "Invalid artwork channel: missing required fields");
      return false;
    }
  }

  if (channel_object["source"].is<const char *>()) {
    std::string source_str = channel_object["source"].as<std::string>();
    channel->source = image_source_from_string(source_str);
  }

  if (channel_object["format"].is<const char *>()) {
    std::string format_str = channel_object["format"].as<std::string>();
    channel->format = image_format_from_string(format_str);
  }

  if (channel_object["width"].is<JsonVariant>()) {
    channel->width = channel_object["width"].as<uint16_t>();
  }

  if (channel_object["height"].is<JsonVariant>()) {
    channel->height = channel_object["height"].as<uint16_t>();
  }

  return true;
}
#endif

bool process_stream_start_message(JsonObject root, StreamStartMessage *stream_msg) {
  if (stream_msg == nullptr) {
    return true;
  }

#ifdef USE_SENDSPIN_PLAYER
  if (root["payload"]["player"].is<JsonObject>()) {
    ServerPlayerStreamObject player_obj;
    if (process_player_stream_object(root["payload"]["player"], &player_obj, true)) {
      if (!player_obj.is_complete()) {
        ESP_LOGE(TAG, "Invalid stream/start message: incomplete player object");
        return false;
      }
      stream_msg->player = player_obj;
    } else {
      return false;
    }
  }
#endif

#ifdef USE_SENDSPIN_ARTWORK
  if (root["payload"]["artwork"]["channels"].is<JsonArray>()) {
    ServerArtworkStreamObject artwork_obj;
    std::vector<ServerArtworkChannelObject> channels;

    for (JsonObject channel_json : root["payload"]["artwork"]["channels"].as<JsonArray>()) {
      ServerArtworkChannelObject channel;
      if (process_artwork_channel_object(channel_json, &channel, true)) {
        if (!channel.is_complete()) {
          ESP_LOGE(TAG, "Invalid stream/start message: incomplete artwork channel");
          return false;
        }
        channels.push_back(channel);
      } else {
        return false;
      }
    }
    artwork_obj.channels = channels;
    stream_msg->artwork = artwork_obj;
  }
#endif

  return true;
}

bool process_stream_end_message(JsonObject root, StreamEndMessage *end_msg) {
  if (end_msg == nullptr) {
    return true;
  }

  // Parse optional roles array
  if (root["payload"]["roles"].is<JsonArray>()) {
    std::vector<std::string> roles;
    for (JsonVariant role_var : root["payload"]["roles"].as<JsonArray>()) {
      if (role_var.is<const char *>()) {
        roles.push_back(role_var.as<std::string>());
      }
    }
    end_msg->roles = roles;
  }

  return true;
}

bool process_stream_clear_message(JsonObject root, StreamClearMessage *clear_msg) {
  if (clear_msg == nullptr) {
    return true;
  }

  // Parse optional roles array
  if (root["payload"]["roles"].is<JsonArray>()) {
    std::vector<std::string> roles;
    for (JsonVariant role_var : root["payload"]["roles"].as<JsonArray>()) {
      if (role_var.is<const char *>()) {
        roles.push_back(role_var.as<std::string>());
      }
    }
    clear_msg->roles = roles;
  }

  return true;
}

#ifdef USE_SENDSPIN_PLAYER
static bool process_server_player_command_object(const JsonObject player_object,
                                                 ServerPlayerCommandObject *player_cmd) {
  if (player_cmd == nullptr || !player_object["command"].is<JsonVariant>()) {
    return false;
  }

  std::string command_str = player_object["command"].as<std::string>();
  auto command = player_command_from_string(command_str);

  if (!command.has_value()) {
    ESP_LOGE(TAG, "Invalid server player command type: %s", command_str.c_str());
    return false;
  }
  player_cmd->command = command.value();

  // Parse optional fields
  if (player_object["volume"].is<JsonVariant>()) {
    player_cmd->volume = player_object["volume"].as<uint8_t>();
  }

  if (player_object["mute"].is<JsonVariant>()) {
    player_cmd->mute = player_object["mute"].as<bool>();
  }

  if (player_object["static_delay_ms"].is<JsonVariant>()) {
    player_cmd->static_delay_ms = player_object["static_delay_ms"].as<uint16_t>();
  }

  return true;
}
#endif

bool process_server_command_message(JsonObject root, ServerCommandMessage *cmd_msg) {
#ifdef USE_SENDSPIN_PLAYER
  if (cmd_msg != nullptr && root["payload"]["player"].is<JsonObject>()) {
    ServerPlayerCommandObject player_cmd;
    if (process_server_player_command_object(root["payload"]["player"], &player_cmd)) {
      cmd_msg->player = player_cmd;
      return true;
    }
    return false;
  }
#endif
  return true;
}

#ifdef USE_SENDSPIN_METADATA
static bool process_server_metadata_state_object(const JsonObject metadata_object,
                                                 ServerMetadataStateObject *metadata_state) {
  if (metadata_state == nullptr) {
    return false;
  }

  // timestamp is required (not optional)
  if (!metadata_object["timestamp"].is<JsonVariant>()) {
    ESP_LOGE(TAG, "Invalid metadata state object: missing timestamp");
    return false;
  }
  metadata_state->timestamp = metadata_object["timestamp"].as<int64_t>();

  // All other fields are optional - handle both values and null (to clear)
  // Null is treated as empty string so it gets applied via delta updates
  // Use isUnbound() to distinguish "key exists with null" from "key doesn't exist"
  JsonVariantConst title_var = metadata_object["title"];
  if (title_var.is<const char *>()) {
    metadata_state->title = title_var.as<std::string>();
  } else if (!title_var.isUnbound() && title_var.isNull()) {
    metadata_state->title = "";
  }

  JsonVariantConst artist_var = metadata_object["artist"];
  if (artist_var.is<const char *>()) {
    metadata_state->artist = artist_var.as<std::string>();
  } else if (!artist_var.isUnbound() && artist_var.isNull()) {
    metadata_state->artist = "";
  }

  JsonVariantConst album_artist_var = metadata_object["album_artist"];
  if (album_artist_var.is<const char *>()) {
    metadata_state->album_artist = album_artist_var.as<std::string>();
  } else if (!album_artist_var.isUnbound() && album_artist_var.isNull()) {
    metadata_state->album_artist = "";
  }

  JsonVariantConst album_var = metadata_object["album"];
  if (album_var.is<const char *>()) {
    metadata_state->album = album_var.as<std::string>();
  } else if (!album_var.isUnbound() && album_var.isNull()) {
    metadata_state->album = "";
  }

  JsonVariantConst artwork_url_var = metadata_object["artwork_url"];
  if (artwork_url_var.is<const char *>()) {
    metadata_state->artwork_url = artwork_url_var.as<std::string>();
  } else if (!artwork_url_var.isUnbound() && artwork_url_var.isNull()) {
    metadata_state->artwork_url = "";
  }

  if (metadata_object["year"].is<JsonVariant>() && !metadata_object["year"].isNull()) {
    metadata_state->year = metadata_object["year"].as<uint16_t>();
  }

  if (metadata_object["track"].is<JsonVariant>() && !metadata_object["track"].isNull()) {
    metadata_state->track = metadata_object["track"].as<uint16_t>();
  }

  // Parse progress object - if any progress field is present, create the object
  if (metadata_object["progress"].is<JsonObject>()) {
    JsonObject progress_object = metadata_object["progress"];
    MetadataProgressObject progress;
    if (progress_object["track_progress"].is<JsonVariant>()) {
      progress.track_progress = progress_object["track_progress"].as<uint32_t>();
    }
    if (progress_object["track_duration"].is<JsonVariant>()) {
      progress.track_duration = progress_object["track_duration"].as<uint32_t>();
    }
    if (progress_object["playback_speed"].is<JsonVariant>()) {
      progress.playback_speed = progress_object["playback_speed"].as<uint32_t>();
    }
    metadata_state->progress = progress;
  } else if (!metadata_object["progress"].isUnbound() && metadata_object["progress"].isNull()) {
    // Explicit null clears progress
    metadata_state->progress = std::nullopt;
  }

  if (metadata_object["repeat"].is<const char *>()) {
    std::string repeat_str = metadata_object["repeat"].as<std::string>();
    metadata_state->repeat = repeat_mode_from_string(repeat_str);
  }

  if (metadata_object["shuffle"].is<JsonVariant>() && !metadata_object["shuffle"].isNull()) {
    metadata_state->shuffle = metadata_object["shuffle"].as<bool>();
  }

  return true;
}

void apply_metadata_state_deltas(ServerMetadataStateObject *current, const ServerMetadataStateObject &updates) {
  if (current == nullptr) {
    return;
  }

  // timestamp is always updated (not optional)
  current->timestamp = updates.timestamp;

  // Update optional fields only if they have values
  if (updates.title.has_value()) {
    current->title = updates.title;
  }

  if (updates.artist.has_value()) {
    current->artist = updates.artist;
  }

  if (updates.album_artist.has_value()) {
    current->album_artist = updates.album_artist;
  }

  if (updates.album.has_value()) {
    current->album = updates.album;
  }

  if (updates.artwork_url.has_value()) {
    current->artwork_url = updates.artwork_url;
  }

  if (updates.year.has_value()) {
    current->year = updates.year;
  }

  if (updates.track.has_value()) {
    current->track = updates.track;
  }

  if (updates.progress.has_value()) {
    current->progress = updates.progress;
  }

  if (updates.repeat.has_value()) {
    current->repeat = updates.repeat;
  }

  if (updates.shuffle.has_value()) {
    current->shuffle = updates.shuffle;
  }
}
#endif

bool process_server_state_message(JsonObject root, ServerStateMessage *state_msg) {
  if (state_msg == nullptr) {
    return true;
  }

#ifdef USE_SENDSPIN_METADATA
  // Parse optional metadata object
  if (root["payload"]["metadata"].is<JsonObject>()) {
    ServerMetadataStateObject metadata_state;
    if (process_server_metadata_state_object(root["payload"]["metadata"], &metadata_state)) {
      state_msg->metadata = metadata_state;
    }
  }
#endif
#ifdef USE_SENDSPIN_CONTROLLER
  if (root["payload"]["controller"].is<JsonObject>()) {
    ServerStateControllerObject controller_state;
    JsonObject controller_object = root["payload"]["controller"];

    // Parse supported_commands array
    if (controller_object["supported_commands"].is<JsonArray>()) {
      std::vector<SendspinControllerCommand> commands;
      for (JsonVariant command_var : controller_object["supported_commands"].as<JsonArray>()) {
        if (command_var.is<const char *>()) {
          std::string command_str = command_var.as<std::string>();
          auto command = controller_command_from_string(command_str);
          if (command.has_value()) {
            commands.push_back(command.value());
          }
        }
      }
      controller_state.supported_commands = commands;
    }

    // Parse volume
    if (controller_object["volume"].is<JsonVariant>()) {
      controller_state.volume = controller_object["volume"].as<uint8_t>();
    }

    // Parse muted
    if (controller_object["muted"].is<JsonVariant>()) {
      controller_state.muted = controller_object["muted"].as<bool>();
    }

    state_msg->controller = controller_state;
  }
#endif

  return true;
}

std::string format_client_hello_message(const ClientHelloMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "client/hello";
    root["payload"]["client_id"] = msg->client_id;
    root["payload"]["name"] = msg->name;
    if (msg->device_info.has_value()) {
      const auto &info = msg->device_info.value();
      if (info.product_name.has_value()) {
        root["payload"]["device_info"]["product_name"] = info.product_name.value();
      }
      if (info.manufacturer.has_value()) {
        root["payload"]["device_info"]["manufacturer"] = info.manufacturer.value();
      }
      if (info.software_version.has_value()) {
        root["payload"]["device_info"]["software_version"] = info.software_version.value();
      }
    }
    root["payload"]["version"] = msg->version;
    JsonArray supported_roles_list = root["payload"]["supported_roles"].to<JsonArray>();
    for (const auto &role : msg->supported_roles) {
      supported_roles_list.add(to_cstr(role));
    }
#ifdef USE_SENDSPIN_PLAYER
    if (msg->player_v1_support.has_value()) {
      JsonArray formats_list = root["payload"]["player@v1_support"]["supported_formats"].to<JsonArray>();
      for (const auto &format : msg->player_v1_support.value().supported_formats) {
        JsonObject format_obj = formats_list.add<JsonObject>();
        format_obj["codec"] = to_cstr(format.codec);
        format_obj["channels"] = format.channels;
        format_obj["sample_rate"] = format.sample_rate;
        format_obj["bit_depth"] = format.bit_depth;
      }
      root["payload"]["player@v1_support"]["buffer_capacity"] = msg->player_v1_support.value().buffer_capacity;
      JsonArray commands_list = root["payload"]["player@v1_support"]["supported_commands"].to<JsonArray>();
      for (const auto &cmd : msg->player_v1_support.value().supported_commands) {
        commands_list.add(to_cstr(cmd));
      }
    }
#endif
#ifdef USE_SENDSPIN_ARTWORK
    if (msg->artwork_v1_support.has_value()) {
      JsonArray channels_list = root["payload"]["artwork@v1_support"]["channels"].to<JsonArray>();
      for (const auto &channel : msg->artwork_v1_support.value().channels) {
        JsonObject channel_obj = channels_list.add<JsonObject>();
        channel_obj["source"] = to_cstr(channel.source);
        channel_obj["format"] = to_cstr(channel.format);
        channel_obj["media_width"] = channel.media_width;
        channel_obj["media_height"] = channel.media_height;
      }
    }
#endif
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

std::string format_client_state_message(const ClientStateMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "client/state";
    root["payload"]["state"] = to_cstr(msg->state);
#ifdef USE_SENDSPIN_PLAYER
    if (msg->player.has_value()) {
      const ClientPlayerStateObject &player_state = msg->player.value();
      root["payload"]["player"]["volume"] = player_state.volume;
      root["payload"]["player"]["muted"] = player_state.muted;
      root["payload"]["player"]["static_delay_ms"] = player_state.static_delay_ms;
      if (!player_state.supported_commands.empty()) {
        JsonArray commands_list = root["payload"]["player"]["supported_commands"].to<JsonArray>();
        for (const auto &cmd : player_state.supported_commands) {
          commands_list.add(to_cstr(cmd));
        }
      }
    }
#endif
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

std::string format_stream_request_format_message(const StreamRequestFormatMessage *msg) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([msg](JsonObject root) {
    root["type"] = "stream/request-format";
#ifdef USE_SENDSPIN_PLAYER
    if (msg->player.has_value()) {
      const auto &player = msg->player.value();
      if (player.codec.has_value()) {
        root["payload"]["player"]["codec"] = to_cstr(player.codec.value());
      }
      if (player.sample_rate.has_value()) {
        root["payload"]["player"]["sample_rate"] = player.sample_rate.value();
      }
      if (player.channels.has_value()) {
        root["payload"]["player"]["channels"] = player.channels.value();
      }
      if (player.bit_depth.has_value()) {
        root["payload"]["player"]["bit_depth"] = player.bit_depth.value();
      }
    }
#endif
#ifdef USE_SENDSPIN_ARTWORK
    if (msg->artwork.has_value()) {
      const auto &artwork = msg->artwork.value();
      root["payload"]["artwork"]["channel"] = artwork.channel;
      if (artwork.source.has_value()) {
        root["payload"]["artwork"]["source"] = to_cstr(artwork.source.value());
      }
      if (artwork.format.has_value()) {
        root["payload"]["artwork"]["format"] = to_cstr(artwork.format.value());
      }
      if (artwork.media_width.has_value()) {
        root["payload"]["artwork"]["media_width"] = artwork.media_width.value();
      }
      if (artwork.media_height.has_value()) {
        root["payload"]["artwork"]["media_height"] = artwork.media_height.value();
      }
    }
#endif
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

#ifdef USE_SENDSPIN_CONTROLLER
std::string format_client_command_message(SendspinControllerCommand command, std::optional<uint8_t> volume,
                                          std::optional<bool> mute) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([command, volume, mute](JsonObject root) {
    root["type"] = "client/command";
    root["payload"]["controller"]["command"] = to_cstr(command);
    if (command == SendspinControllerCommand::VOLUME && volume.has_value()) {
      root["payload"]["controller"]["volume"] = volume.value();
    }
    if (command == SendspinControllerCommand::MUTE && mute.has_value()) {
      root["payload"]["controller"]["mute"] = mute.value();
    }
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
#endif

std::string format_client_goodbye_message(SendspinGoodbyeReason reason) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return json::build_json([reason](JsonObject root) {
    root["type"] = "client/goodbye";
    root["payload"]["reason"] = to_cstr(reason);
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
