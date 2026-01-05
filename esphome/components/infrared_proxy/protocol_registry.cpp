#include "protocol_registry.h"
#include "esphome/core/log.h"

// Include all remote_base protocol headers
#include "esphome/components/remote_base/abbwelcome_protocol.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/beo4_protocol.h"
#include "esphome/components/remote_base/byronsx_protocol.h"
#include "esphome/components/remote_base/canalsat_protocol.h"
#include "esphome/components/remote_base/coolix_protocol.h"
#include "esphome/components/remote_base/dish_protocol.h"
#include "esphome/components/remote_base/dooya_protocol.h"
#include "esphome/components/remote_base/drayton_protocol.h"
#include "esphome/components/remote_base/dyson_protocol.h"
#include "esphome/components/remote_base/gobox_protocol.h"
#include "esphome/components/remote_base/haier_protocol.h"
#include "esphome/components/remote_base/jvc_protocol.h"
#include "esphome/components/remote_base/keeloq_protocol.h"
#include "esphome/components/remote_base/lg_protocol.h"
#include "esphome/components/remote_base/magiquest_protocol.h"
#include "esphome/components/remote_base/midea_protocol.h"
#include "esphome/components/remote_base/mirage_protocol.h"
#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/components/remote_base/nexa_protocol.h"
#include "esphome/components/remote_base/panasonic_protocol.h"
#include "esphome/components/remote_base/pioneer_protocol.h"
#include "esphome/components/remote_base/pronto_protocol.h"
#include "esphome/components/remote_base/raw_protocol.h"
#include "esphome/components/remote_base/rc5_protocol.h"
#include "esphome/components/remote_base/rc6_protocol.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include "esphome/components/remote_base/roomba_protocol.h"
#include "esphome/components/remote_base/samsung36_protocol.h"
#include "esphome/components/remote_base/samsung_protocol.h"
#include "esphome/components/remote_base/sony_protocol.h"
#include "esphome/components/remote_base/symphony_protocol.h"
#include "esphome/components/remote_base/toshiba_ac_protocol.h"
#include "esphome/components/remote_base/toto_protocol.h"

namespace esphome::infrared_proxy {

static const char *const TAG = "infrared_proxy.protocol";

// Helper function to check if a JSON field exists and has the correct type
template<typename T> static bool check_field(JsonObject root, const char *field, const char *protocol) {
  if (!root[field].is<T>()) {
    ESP_LOGE(TAG, "%s: Missing or invalid required field '%s'", protocol, field);
    return false;
  }
  return true;
}

// Helper function to check if a JSON array field exists and has the correct size
static bool check_array_field(JsonObject root, const char *field, const char *protocol, size_t expected_size = 0) {
  if (!root[field].is<JsonArray>()) {
    ESP_LOGE(TAG, "%s: Missing or invalid required field '%s' (must be array)", protocol, field);
    return false;
  }
  if (expected_size > 0) {
    JsonArray arr = root[field].as<JsonArray>();
    if (arr.size() != expected_size) {
      ESP_LOGE(TAG, "%s: Field '%s' must have exactly %zu elements (got %zu)", protocol, field, expected_size,
               arr.size());
      return false;
    }
  }
  return true;
}

// Protocol parser implementations

static bool parse_abbwelcome(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "source_address", "abbwelcome") ||
      !check_field<uint32_t>(root, "destination_address", "abbwelcome") ||
      !check_field<uint32_t>(root, "message_type", "abbwelcome")) {
    return false;
  }

  remote_base::ABBWelcomeData data{};

  // Set three_byte_address first (must be called before set_source_address, set_destination_address, set_message_id)
  bool three_byte_address = root["three_byte_address"].is<bool>() ? root["three_byte_address"].as<bool>() : false;
  data.set_three_byte_address(three_byte_address);

  data.set_source_address(root["source_address"].as<uint32_t>());
  data.set_destination_address(root["destination_address"].as<uint32_t>());
  data.set_message_type(root["message_type"].as<uint8_t>());
  data.set_retransmission(root["retransmission"].is<bool>() ? root["retransmission"].as<bool>() : false);
  data.set_message_id(root["message_id"].is<uint32_t>() ? root["message_id"].as<uint8_t>() : 0);

  // Optional data array (0-15 bytes)
  if (root["data"].is<JsonArray>()) {
    JsonArray data_arr = root["data"].as<JsonArray>();
    if (data_arr.size() > 15) {
      ESP_LOGE(TAG, "abbwelcome: 'data' array cannot exceed 15 elements");
      return false;
    }
    std::vector<uint8_t> data_vec;
    data_vec.reserve(data_arr.size());
    for (JsonVariant item : data_arr) {
      data_vec.push_back(item.as<uint8_t>());
    }
    data.set_data(data_vec);
  }

  data.finalize();
  remote_base::ABBWelcomeProtocol().encode(dst, data);
  return true;
}

static bool parse_aeha(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "aeha") || !check_array_field(root, "data", "aeha")) {
    return false;
  }

  JsonArray data_arr = root["data"].as<JsonArray>();
  if (data_arr.size() < 2 || data_arr.size() > 35) {
    ESP_LOGE(TAG, "aeha: 'data' array must have 2-35 elements (got %zu)", data_arr.size());
    return false;
  }

  remote_base::AEHAData data{};
  data.address = root["address"].as<uint16_t>();
  data.data.reserve(data_arr.size());
  for (JsonVariant item : data_arr) {
    data.data.push_back(item.as<uint8_t>());
  }

  // Optional carrier frequency (default 38kHz is set by protocol)
  if (root["carrier_frequency"].is<uint32_t>()) {
    dst->set_carrier_frequency(root["carrier_frequency"].as<uint32_t>());
  }

  remote_base::AEHAProtocol().encode(dst, data);
  return true;
}

static bool parse_beo4(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "source", "beo4") || !check_field<uint32_t>(root, "command", "beo4")) {
    return false;
  }

  remote_base::Beo4Data data{};
  data.source = root["source"].as<uint8_t>();
  data.command = root["command"].as<uint8_t>();
  data.repeats = root["repeats"].is<uint32_t>() ? root["repeats"].as<uint8_t>() : 0;

  remote_base::Beo4Protocol().encode(dst, data);
  return true;
}

static bool parse_byronsx(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "byronsx") || !check_field<uint32_t>(root, "command", "byronsx")) {
    return false;
  }

  remote_base::ByronSXData data{};
  data.address = root["address"].as<uint8_t>();
  data.command = root["command"].as<uint8_t>();

  remote_base::ByronSXProtocol().encode(dst, data);
  return true;
}

static bool parse_canalsat(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "device", "canalsat") || !check_field<uint32_t>(root, "command", "canalsat")) {
    return false;
  }

  remote_base::CanalSatData data{};
  data.device = root["device"].as<uint8_t>();
  data.address = root["address"].is<uint32_t>() ? root["address"].as<uint8_t>() : 0;
  data.command = root["command"].as<uint8_t>();

  remote_base::CanalSatProtocol().encode(dst, data);
  return true;
}

static bool parse_canalsatld(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "device", "canalsatld") || !check_field<uint32_t>(root, "command", "canalsatld")) {
    return false;
  }

  remote_base::CanalSatLDData data{};
  data.device = root["device"].as<uint8_t>();
  data.address = root["address"].is<uint32_t>() ? root["address"].as<uint8_t>() : 0;
  data.command = root["command"].as<uint8_t>();

  remote_base::CanalSatLDProtocol().encode(dst, data);
  return true;
}

static bool parse_coolix(JsonObject root, remote_base::RemoteTransmitData *dst) {
  remote_base::CoolixData data{};

  // Coolix can accept either a single value or first/second pair
  if (root["first"].is<uint32_t>()) {
    data.first = root["first"].as<uint32_t>();
    data.second = root["second"].is<uint32_t>() ? root["second"].as<uint32_t>() : 0;
  } else {
    ESP_LOGE(TAG, "coolix: Missing required field 'first'");
    return false;
  }

  remote_base::CoolixProtocol().encode(dst, data);
  return true;
}

static bool parse_dish(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "command", "dish")) {
    return false;
  }

  remote_base::DishData data{};
  data.address = root["address"].is<uint32_t>() ? root["address"].as<uint16_t>() : 1;
  data.command = root["command"].as<uint8_t>();

  remote_base::DishProtocol().encode(dst, data);
  return true;
}

static bool parse_dooya(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "id", "dooya") || !check_field<uint32_t>(root, "channel", "dooya") ||
      !check_field<uint32_t>(root, "button", "dooya") || !check_field<uint32_t>(root, "check", "dooya")) {
    return false;
  }

  remote_base::DooyaData data{};
  data.id = root["id"].as<uint32_t>();
  data.channel = root["channel"].as<uint8_t>();
  data.button = root["button"].as<uint8_t>();
  data.check = root["check"].as<uint8_t>();

  remote_base::DooyaProtocol().encode(dst, data);
  return true;
}

static bool parse_drayton(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "drayton") || !check_field<uint32_t>(root, "channel", "drayton") ||
      !check_field<uint32_t>(root, "command", "drayton")) {
    return false;
  }

  remote_base::DraytonData data{};
  data.address = root["address"].as<uint16_t>();
  data.channel = root["channel"].as<uint8_t>();
  data.command = root["command"].as<uint8_t>();

  remote_base::DraytonProtocol().encode(dst, data);
  return true;
}

static bool parse_dyson(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "code", "dyson")) {
    return false;
  }

  remote_base::DysonData data{};
  data.code = root["code"].as<uint16_t>();
  data.index = root["index"].is<uint32_t>() ? root["index"].as<uint8_t>() : 0xFF;

  remote_base::DysonProtocol().encode(dst, data);
  return true;
}

static bool parse_gobox(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<int32_t>(root, "code", "gobox")) {
    return false;
  }

  remote_base::GoboxData data{};
  data.code = root["code"].as<int32_t>();

  remote_base::GoboxProtocol().encode(dst, data);
  return true;
}

static bool parse_haier(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_array_field(root, "code", "haier", 13)) {
    return false;
  }

  remote_base::HaierData data{};
  JsonArray code_arr = root["code"].as<JsonArray>();
  data.data.reserve(code_arr.size());
  for (JsonVariant item : code_arr) {
    data.data.push_back(item.as<uint8_t>());
  }

  remote_base::HaierProtocol().encode(dst, data);
  return true;
}

static bool parse_jvc(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "data", "jvc")) {
    return false;
  }

  remote_base::JVCData data{};
  data.data = root["data"].as<uint32_t>();

  remote_base::JVCProtocol().encode(dst, data);
  return true;
}

static bool parse_keeloq(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "keeloq") || !check_field<uint32_t>(root, "encrypted", "keeloq")) {
    return false;
  }

  remote_base::KeeloqData data{};
  data.address = root["address"].as<uint32_t>();
  data.command = root["command"].is<uint32_t>() ? root["command"].as<uint8_t>() : 0x10;
  data.encrypted = root["encrypted"].as<uint32_t>();
  data.vlow = root["vlow"].is<bool>() ? root["vlow"].as<bool>() : false;
  data.repeat = false;  // Not exposed via JSON

  remote_base::KeeloqProtocol().encode(dst, data);
  return true;
}

static bool parse_lg(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "data", "lg")) {
    return false;
  }

  remote_base::LGData data{};
  data.data = root["data"].as<uint32_t>();
  data.nbits = root["nbits"].is<uint32_t>() ? root["nbits"].as<uint8_t>() : 28;

  remote_base::LGProtocol().encode(dst, data);
  return true;
}

static bool parse_magiquest(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "wand_id", "magiquest")) {
    return false;
  }

  remote_base::MagiQuestData data{};
  data.wand_id = root["wand_id"].as<uint32_t>();
  data.magnitude = root["magnitude"].is<uint32_t>() ? root["magnitude"].as<uint16_t>() : 0xFFFF;

  remote_base::MagiQuestProtocol().encode(dst, data);
  return true;
}

static bool parse_midea(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_array_field(root, "code", "midea")) {
    return false;
  }

  JsonArray code_arr = root["code"].as<JsonArray>();
  if (code_arr.size() != 6) {
    ESP_LOGE(TAG, "midea: 'code' array must have exactly 6 elements (got %zu)", code_arr.size());
    return false;
  }

  std::vector<uint8_t> code_vec;
  code_vec.reserve(code_arr.size());
  for (JsonVariant item : code_arr) {
    code_vec.push_back(item.as<uint8_t>());
  }

  remote_base::MideaData data(code_vec);
  data.finalize();

  remote_base::MideaProtocol().encode(dst, data);
  return true;
}

static bool parse_mirage(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_array_field(root, "code", "mirage")) {
    return false;
  }

  JsonArray code_arr = root["code"].as<JsonArray>();
  if (code_arr.size() != 14) {
    ESP_LOGE(TAG, "mirage: 'code' array must have exactly 14 elements (got %zu)", code_arr.size());
    return false;
  }

  remote_base::MirageData data{};
  data.data.reserve(code_arr.size());
  for (JsonVariant item : code_arr) {
    data.data.push_back(item.as<uint8_t>());
  }

  remote_base::MirageProtocol().encode(dst, data);
  return true;
}

static bool parse_nec(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "nec") || !check_field<uint32_t>(root, "command", "nec")) {
    return false;
  }

  remote_base::NECData data{};
  data.address = root["address"].as<uint16_t>();
  data.command = root["command"].as<uint16_t>();
  data.command_repeats = root["command_repeats"].is<uint32_t>() ? root["command_repeats"].as<uint16_t>() : 1;

  remote_base::NECProtocol().encode(dst, data);
  return true;
}

static bool parse_nexa(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "device", "nexa") || !check_field<uint32_t>(root, "group", "nexa") ||
      !check_field<uint32_t>(root, "state", "nexa") || !check_field<uint32_t>(root, "channel", "nexa") ||
      !check_field<uint32_t>(root, "level", "nexa")) {
    return false;
  }

  remote_base::NexaData data{};
  data.device = root["device"].as<uint32_t>();
  data.group = root["group"].as<uint8_t>();
  data.state = root["state"].as<uint8_t>();
  data.channel = root["channel"].as<uint8_t>();
  data.level = root["level"].as<uint8_t>();

  remote_base::NexaProtocol().encode(dst, data);
  return true;
}

static bool parse_panasonic(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "panasonic") || !check_field<uint32_t>(root, "command", "panasonic")) {
    return false;
  }

  remote_base::PanasonicData data{};
  data.address = root["address"].as<uint16_t>();
  data.command = root["command"].as<uint32_t>();

  remote_base::PanasonicProtocol().encode(dst, data);
  return true;
}

static bool parse_pioneer(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "rc_code_1", "pioneer")) {
    return false;
  }

  remote_base::PioneerData data{};
  data.rc_code_1 = root["rc_code_1"].as<uint16_t>();
  data.rc_code_2 = root["rc_code_2"].is<uint32_t>() ? root["rc_code_2"].as<uint16_t>() : 0;

  remote_base::PioneerProtocol().encode(dst, data);
  return true;
}

static bool parse_pronto(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<const char *>(root, "data", "pronto")) {
    return false;
  }

  remote_base::ProntoData data{};
  data.data = root["data"].as<std::string>();
  data.delta = root["delta"].is<int32_t>() ? root["delta"].as<int32_t>() : -1;

  remote_base::ProntoProtocol().encode(dst, data);
  return true;
}

static bool parse_raw(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_array_field(root, "code", "raw")) {
    return false;
  }

  JsonArray timings = root["code"].as<JsonArray>();
  if (timings.size() == 0) {
    ESP_LOGE(TAG, "raw: 'code' array cannot be empty");
    return false;
  }

  for (JsonVariant timing : timings) {
    if (!timing.is<int32_t>()) {
      ESP_LOGE(TAG, "raw: Invalid timing value (must be int32)");
      return false;
    }
    int32_t val = timing.as<int32_t>();
    if (val < 0) {
      dst->space(static_cast<uint32_t>(-val));
    } else {
      dst->mark(static_cast<uint32_t>(val));
    }
  }

  // Optional carrier frequency
  if (root["carrier_frequency"].is<uint32_t>()) {
    dst->set_carrier_frequency(root["carrier_frequency"].as<uint32_t>());
  }

  return true;
}

static bool parse_rc5(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "rc5") || !check_field<uint32_t>(root, "command", "rc5")) {
    return false;
  }

  remote_base::RC5Data data{};
  data.address = root["address"].as<uint8_t>();
  data.command = root["command"].as<uint8_t>();

  remote_base::RC5Protocol().encode(dst, data);
  return true;
}

static bool parse_rc6(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "rc6") || !check_field<uint32_t>(root, "command", "rc6")) {
    return false;
  }

  remote_base::RC6Data data{};
  data.address = root["address"].as<uint8_t>();
  data.command = root["command"].as<uint8_t>();

  remote_base::RC6Protocol().encode(dst, data);
  return true;
}

static bool parse_rc_switch_raw(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<const char *>(root, "code", "rc_switch_raw")) {
    return false;
  }

  std::string code = root["code"].as<std::string>();

  // Get protocol (default to protocol 1)
  uint8_t protocol_num = root["protocol"].is<uint32_t>() ? root["protocol"].as<uint8_t>() : 1;
  if (protocol_num < 1 || protocol_num > 8) {
    ESP_LOGE(TAG, "rc_switch_raw: protocol must be 1-8");
    return false;
  }

  remote_base::RCSwitchBase proto = remote_base::RC_SWITCH_PROTOCOLS[protocol_num - 1];
  uint64_t the_code = remote_base::decode_binary_string(code);
  uint8_t nbits = code.size();
  proto.transmit(dst, the_code, nbits);

  return true;
}

static bool parse_rc_switch_type_a(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<const char *>(root, "group", "rc_switch_type_a") ||
      !check_field<const char *>(root, "device", "rc_switch_type_a") ||
      !check_field<bool>(root, "state", "rc_switch_type_a")) {
    return false;
  }

  std::string group = root["group"].as<std::string>();
  std::string device = root["device"].as<std::string>();
  bool state = root["state"].as<bool>();

  // Validate group and device are 5 binary digits
  if (group.length() != 5 || device.length() != 5) {
    ESP_LOGE(TAG, "rc_switch_type_a: 'group' and 'device' must be exactly 5 binary digits");
    return false;
  }

  // Get protocol (default to protocol 1)
  uint8_t protocol_num = root["protocol"].is<uint32_t>() ? root["protocol"].as<uint8_t>() : 1;
  if (protocol_num < 1 || protocol_num > 8) {
    ESP_LOGE(TAG, "rc_switch_type_a: protocol must be 1-8");
    return false;
  }

  remote_base::RCSwitchBase proto = remote_base::RC_SWITCH_PROTOCOLS[protocol_num - 1];
  uint8_t u_group = remote_base::decode_binary_string(group);
  uint8_t u_device = remote_base::decode_binary_string(device);

  uint64_t code;
  uint8_t nbits;
  proto.type_a_code(u_group, u_device, state, &code, &nbits);
  proto.transmit(dst, code, nbits);

  return true;
}

static bool parse_rc_switch_type_b(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "rc_switch_type_b") ||
      !check_field<uint32_t>(root, "channel", "rc_switch_type_b") ||
      !check_field<bool>(root, "state", "rc_switch_type_b")) {
    return false;
  }

  uint8_t address = root["address"].as<uint8_t>();
  uint8_t channel = root["channel"].as<uint8_t>();
  bool state = root["state"].as<bool>();

  // Get protocol (default to protocol 1)
  uint8_t protocol_num = root["protocol"].is<uint32_t>() ? root["protocol"].as<uint8_t>() : 1;
  if (protocol_num < 1 || protocol_num > 8) {
    ESP_LOGE(TAG, "rc_switch_type_b: protocol must be 1-8");
    return false;
  }

  remote_base::RCSwitchBase proto = remote_base::RC_SWITCH_PROTOCOLS[protocol_num - 1];
  uint64_t code;
  uint8_t nbits;
  proto.type_b_code(address, channel, state, &code, &nbits);
  proto.transmit(dst, code, nbits);

  return true;
}

static bool parse_rc_switch_type_c(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<const char *>(root, "family", "rc_switch_type_c") ||
      !check_field<uint32_t>(root, "group", "rc_switch_type_c") ||
      !check_field<uint32_t>(root, "device", "rc_switch_type_c") ||
      !check_field<bool>(root, "state", "rc_switch_type_c")) {
    return false;
  }

  std::string family = root["family"].as<std::string>();
  if (family.length() != 1 || family[0] < 'a' || family[0] > 'p') {
    ESP_LOGE(TAG, "rc_switch_type_c: 'family' must be a single letter a-p");
    return false;
  }

  uint8_t group = root["group"].as<uint8_t>();
  uint8_t device = root["device"].as<uint8_t>();
  bool state = root["state"].as<bool>();

  // Get protocol (default to protocol 1)
  uint8_t protocol_num = root["protocol"].is<uint32_t>() ? root["protocol"].as<uint8_t>() : 1;
  if (protocol_num < 1 || protocol_num > 8) {
    ESP_LOGE(TAG, "rc_switch_type_c: protocol must be 1-8");
    return false;
  }

  remote_base::RCSwitchBase proto = remote_base::RC_SWITCH_PROTOCOLS[protocol_num - 1];
  uint64_t code;
  uint8_t nbits;
  proto.type_c_code(family[0], group, device, state, &code, &nbits);
  proto.transmit(dst, code, nbits);

  return true;
}

static bool parse_rc_switch_type_d(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<const char *>(root, "group", "rc_switch_type_d") ||
      !check_field<uint32_t>(root, "device", "rc_switch_type_d") ||
      !check_field<bool>(root, "state", "rc_switch_type_d")) {
    return false;
  }

  std::string group = root["group"].as<std::string>();
  if (group.length() != 1 || (group[0] != 'a' && group[0] != 'b' && group[0] != 'c' && group[0] != 'd')) {
    ESP_LOGE(TAG, "rc_switch_type_d: 'group' must be one of: a, b, c, d");
    return false;
  }

  uint8_t device = root["device"].as<uint8_t>();
  bool state = root["state"].as<bool>();

  // Get protocol (default to protocol 1)
  uint8_t protocol_num = root["protocol"].is<uint32_t>() ? root["protocol"].as<uint8_t>() : 1;
  if (protocol_num < 1 || protocol_num > 8) {
    ESP_LOGE(TAG, "rc_switch_type_d: protocol must be 1-8");
    return false;
  }

  remote_base::RCSwitchBase proto = remote_base::RC_SWITCH_PROTOCOLS[protocol_num - 1];
  uint64_t code;
  uint8_t nbits;
  proto.type_d_code(group[0], device, state, &code, &nbits);
  proto.transmit(dst, code, nbits);

  return true;
}

static bool parse_roomba(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "data", "roomba")) {
    return false;
  }

  remote_base::RoombaData data{};
  data.data = root["data"].as<uint8_t>();

  remote_base::RoombaProtocol().encode(dst, data);
  return true;
}

static bool parse_samsung(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "data", "samsung")) {
    return false;
  }

  remote_base::SamsungData data{};
  data.data = root["data"].as<uint64_t>();
  data.nbits = root["nbits"].is<uint32_t>() ? root["nbits"].as<uint8_t>() : 32;

  remote_base::SamsungProtocol().encode(dst, data);
  return true;
}

static bool parse_samsung36(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "address", "samsung36") || !check_field<uint32_t>(root, "command", "samsung36")) {
    return false;
  }

  remote_base::Samsung36Data data{};
  data.address = root["address"].as<uint16_t>();
  data.command = root["command"].as<uint32_t>();

  remote_base::Samsung36Protocol().encode(dst, data);
  return true;
}

static bool parse_sony(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "data", "sony")) {
    return false;
  }

  remote_base::SonyData data{};
  data.data = root["data"].as<uint32_t>();
  data.nbits = root["nbits"].is<uint32_t>() ? root["nbits"].as<uint8_t>() : 12;

  remote_base::SonyProtocol().encode(dst, data);
  return true;
}

static bool parse_symphony(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "data", "symphony") || !check_field<uint32_t>(root, "nbits", "symphony")) {
    return false;
  }

  remote_base::SymphonyData data{};
  data.data = root["data"].as<uint32_t>();
  data.nbits = root["nbits"].as<uint8_t>();
  data.repeats = root["repeats"].is<uint32_t>() ? root["repeats"].as<uint8_t>() : 1;

  remote_base::SymphonyProtocol().encode(dst, data);
  return true;
}

static bool parse_toshiba_ac(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "rc_code_1", "toshiba_ac")) {
    return false;
  }

  remote_base::ToshibaAcData data{};
  data.rc_code_1 = root["rc_code_1"].as<uint64_t>();
  data.rc_code_2 = root["rc_code_2"].is<uint32_t>() ? root["rc_code_2"].as<uint64_t>() : 0;

  remote_base::ToshibaAcProtocol().encode(dst, data);
  return true;
}

static bool parse_toto(JsonObject root, remote_base::RemoteTransmitData *dst) {
  if (!check_field<uint32_t>(root, "command", "toto")) {
    return false;
  }

  remote_base::TotoData data{};
  data.command = root["command"].as<uint8_t>();
  data.rc_code_1 = root["rc_code_1"].is<uint32_t>() ? root["rc_code_1"].as<uint8_t>() : 0;
  data.rc_code_2 = root["rc_code_2"].is<uint32_t>() ? root["rc_code_2"].as<uint8_t>() : 0;

  remote_base::TotoProtocol().encode(dst, data);
  return true;
}

// Protocol parser lookup table entry
struct ProtocolParser {
  const char *name;
  bool (*parser)(JsonObject, remote_base::RemoteTransmitData *);
};

// Protocol lookup table - sorted alphabetically for readability
static constexpr ProtocolParser PROTOCOL_PARSERS[] = {
    {"abbwelcome", parse_abbwelcome},
    {"aeha", parse_aeha},
    {"beo4", parse_beo4},
    {"byronsx", parse_byronsx},
    {"canalsat", parse_canalsat},
    {"canalsatld", parse_canalsatld},
    {"coolix", parse_coolix},
    {"dish", parse_dish},
    {"dooya", parse_dooya},
    {"drayton", parse_drayton},
    {"dyson", parse_dyson},
    {"gobox", parse_gobox},
    {"haier", parse_haier},
    {"jvc", parse_jvc},
    {"keeloq", parse_keeloq},
    {"lg", parse_lg},
    {"magiquest", parse_magiquest},
    {"midea", parse_midea},
    {"mirage", parse_mirage},
    {"nec", parse_nec},
    {"nexa", parse_nexa},
    {"panasonic", parse_panasonic},
    {"pioneer", parse_pioneer},
    {"pronto", parse_pronto},
    {"raw", parse_raw},
    {"rc5", parse_rc5},
    {"rc6", parse_rc6},
    {"rc_switch_raw", parse_rc_switch_raw},
    {"rc_switch_type_a", parse_rc_switch_type_a},
    {"rc_switch_type_b", parse_rc_switch_type_b},
    {"rc_switch_type_c", parse_rc_switch_type_c},
    {"rc_switch_type_d", parse_rc_switch_type_d},
    {"roomba", parse_roomba},
    {"samsung", parse_samsung},
    {"samsung36", parse_samsung36},
    {"sony", parse_sony},
    {"symphony", parse_symphony},
    {"toshiba_ac", parse_toshiba_ac},
    {"toto", parse_toto},
};

static constexpr size_t PROTOCOL_COUNT = sizeof(PROTOCOL_PARSERS) / sizeof(PROTOCOL_PARSERS[0]);

// Main encode function that dispatches to protocol-specific parsers
bool encode_from_json(const std::string &protocol_json, remote_base::RemoteTransmitData *transmit_data) {
  // Parse JSON
  bool success = json::parse_json(protocol_json, [transmit_data](JsonObject root) -> bool {
    // Extract protocol name
    if (!root["protocol"].is<const char *>()) {
      ESP_LOGE(TAG, "Missing required field 'protocol'\n"
                    "Example: {\"protocol\":\"nec\",\"address\":255,\"command\":187}");
      return false;
    }

    const char *protocol = root["protocol"].as<const char *>();

    // Look up protocol in table
    for (size_t i = 0; i < PROTOCOL_COUNT; i++) {
      if (strcmp(protocol, PROTOCOL_PARSERS[i].name) == 0) {
        return PROTOCOL_PARSERS[i].parser(root, transmit_data);
      }
    }

    // Protocol not found
    ESP_LOGE(TAG,
             "Unknown protocol: %s\n"
             "Supported protocols:",
             protocol);
    // This will generate a lot of logging calls but saves us from having to
    //  maintain a separate list of protocol names, reducing binary size.
    for (size_t i = 0; i < PROTOCOL_COUNT; i++) {
      ESP_LOGE(TAG, "  %s", PROTOCOL_PARSERS[i].name);
    }
    return false;
  });

  return success;
}

void write_supported_protocols_json(std::string &out) {
  // Build JSON array of protocol names directly into output string
  out.clear();
  out.reserve(512);  // Pre-allocate reasonable capacity to avoid reallocation during construction
  out += "[";
  for (size_t i = 0; i < PROTOCOL_COUNT; i++) {
    if (i > 0) {
      out += ",";
    }
    out += "\"";
    out += PROTOCOL_PARSERS[i].name;
    out += "\"";
  }
  out += "]";
}

}  // namespace esphome::infrared_proxy
