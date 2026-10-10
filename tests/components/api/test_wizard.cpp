#ifdef USE_HOST
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_wizard.h"
#include "esphome/components/api/proto.h"

namespace esphome::api {

// RAM buffers of the inputs, as codegen defines them
static char *wizard_input_weather() {
  static char buffer[WIZARD_ENTITY_ID_BUFFER_SIZE] = "sensor.default";
  return buffer;
}
static char *wizard_input_other() {
  static char buffer[WIZARD_ENTITY_ID_BUFFER_SIZE] = "";
  return buffer;
}

static constexpr uint32_t WEATHER_KEY = 0x0a0b0c0d;
static constexpr uint32_t OTHER_KEY = 0x11223344;

// The same shapes the generated code emits. The data is 200 bytes (API_WIZARD_DATA_SIZE), so its length takes two
// bytes as a varint.
const uint8_t API_WIZARD_DATA[API_WIZARD_DATA_SIZE] = {
    0x03, 0x0a, 0x11, 0x18, 0x1f, 0x26, 0x2d, 0x34, 0x3b, 0x42, 0x49, 0x50, 0x57, 0x5e, 0x65, 0x6c, 0x73, 0x7a, 0x81,
    0x88, 0x8f, 0x96, 0x9d, 0xa4, 0xab, 0xb2, 0xb9, 0xc0, 0xc7, 0xce, 0xd5, 0xdc, 0xe3, 0xea, 0xf1, 0xf8, 0xff, 0x06,
    0x0d, 0x14, 0x1b, 0x22, 0x29, 0x30, 0x37, 0x3e, 0x45, 0x4c, 0x53, 0x5a, 0x61, 0x68, 0x6f, 0x76, 0x7d, 0x84, 0x8b,
    0x92, 0x99, 0xa0, 0xa7, 0xae, 0xb5, 0xbc, 0xc3, 0xca, 0xd1, 0xd8, 0xdf, 0xe6, 0xed, 0xf4, 0xfb, 0x02, 0x09, 0x10,
    0x17, 0x1e, 0x25, 0x2c, 0x33, 0x3a, 0x41, 0x48, 0x4f, 0x56, 0x5d, 0x64, 0x6b, 0x72, 0x79, 0x80, 0x87, 0x8e, 0x95,
    0x9c, 0xa3, 0xaa, 0xb1, 0xb8, 0xbf, 0xc6, 0xcd, 0xd4, 0xdb, 0xe2, 0xe9, 0xf0, 0xf7, 0xfe, 0x05, 0x0c, 0x13, 0x1a,
    0x21, 0x28, 0x2f, 0x36, 0x3d, 0x44, 0x4b, 0x52, 0x59, 0x60, 0x67, 0x6e, 0x75, 0x7c, 0x83, 0x8a, 0x91, 0x98, 0x9f,
    0xa6, 0xad, 0xb4, 0xbb, 0xc2, 0xc9, 0xd0, 0xd7, 0xde, 0xe5, 0xec, 0xf3, 0xfa, 0x01, 0x08, 0x0f, 0x16, 0x1d, 0x24,
    0x2b, 0x32, 0x39, 0x40, 0x47, 0x4e, 0x55, 0x5c, 0x63, 0x6a, 0x71, 0x78, 0x7f, 0x86, 0x8d, 0x94, 0x9b, 0xa2, 0xa9,
    0xb0, 0xb7, 0xbe, 0xc5, 0xcc, 0xd3, 0xda, 0xe1, 0xe8, 0xef, 0xf6, 0xfd, 0x04, 0x0b, 0x12, 0x19, 0x20, 0x27, 0x2e,
    0x35, 0x3c, 0x43, 0x4a, 0x51, 0x58, 0x5f, 0x66, 0x6d, 0x74};
const WizardInputEntry API_WIZARD_INPUTS[API_WIZARD_INPUT_COUNT] = {
    {WEATHER_KEY, wizard_input_weather()},
    {OTHER_KEY, wizard_input_other()},
};

using Bytes = std::vector<uint8_t>;

static Bytes encode(const ProtoMessage &msg, uint32_t (*calc)(const void *), ProtoEncodeFn enc) {
  APIBuffer buffer;
  uint32_t size = calc(&msg);
  EXPECT_TRUE(buffer.resize(size));
#ifdef ESPHOME_DEBUG_API
  uint8_t *proto_debug_end_ = buffer.data() + buffer.size();
#endif
  uint8_t *end = enc(&msg, buffer.data() PROTO_ENCODE_DEBUG_ARG);
  EXPECT_EQ(static_cast<size_t>(end - buffer.data()), size);
  return Bytes(buffer.data(), buffer.data() + size);
}

TEST(DeviceWizard, ResponseSendsTheDataUnchanged) {
  DeviceWizardResponse resp;
  resp.data = API_WIZARD_DATA;
  resp.data_len = API_WIZARD_DATA_SIZE;

  // Field 1, length delimited, then the 200 byte length as a two byte varint, then the data
  Bytes expected{0x0a, 0xc8, 0x01};
  expected.insert(expected.end(), API_WIZARD_DATA, API_WIZARD_DATA + API_WIZARD_DATA_SIZE);

  EXPECT_EQ(encode(resp, &DeviceWizardResponse::calc_size_msg, &DeviceWizardResponse::encode_msg), expected);
}

TEST(DeviceWizard, ShortDataHasAOneByteLength) {
  DeviceWizardResponse resp;
  resp.data = API_WIZARD_DATA;
  resp.data_len = 3;

  Bytes expected{0x0a, 0x03, API_WIZARD_DATA[0], API_WIZARD_DATA[1], API_WIZARD_DATA[2]};
  EXPECT_EQ(encode(resp, &DeviceWizardResponse::calc_size_msg, &DeviceWizardResponse::encode_msg), expected);
}

TEST(DeviceWizard, NoDataEncodesNothing) {
  DeviceWizardResponse resp;
  EXPECT_EQ(resp.calculate_size(), 0u);
  EXPECT_TRUE(encode(resp, &DeviceWizardResponse::calc_size_msg, &DeviceWizardResponse::encode_msg).empty());
}

TEST(DeviceWizard, CapabilitiesAnnounceTheWizard) {
  DeviceCapabilitiesResponse resp;
  EXPECT_TRUE(
      encode(resp, &DeviceCapabilitiesResponse::calc_size_msg, &DeviceCapabilitiesResponse::encode_msg).empty());
  resp.wizard.configured = true;
  // Field 5 (the wizard), length delimited, holding field 1 (configured) set to 1
  EXPECT_EQ(encode(resp, &DeviceCapabilitiesResponse::calc_size_msg, &DeviceCapabilitiesResponse::encode_msg),
            (Bytes{0x2a, 0x02, 0x08, 0x01}));
}

// A request for the input with the given key and entity id
static const char *set_input(uint32_t key, const char *entity_id) {
  WizardInputSetRequest request;
  request.key = key;
  request.entity_id = StringRef(entity_id);
  return wizard_set_input(request);
}

TEST(DeviceWizard, SetInputStoresTheEntityIdInTheBuffer) {
  EXPECT_EQ(set_input(OTHER_KEY, "sensor.outdoor"), wizard_input_other());
  EXPECT_STREQ(wizard_input_other(), "sensor.outdoor");
  // A shorter id replaces a longer one completely
  EXPECT_EQ(set_input(OTHER_KEY, "light.a"), wizard_input_other());
  EXPECT_STREQ(wizard_input_other(), "light.a");
  // The longest valid id fills the buffer
  std::string longest = "sensor." + std::string(WIZARD_ENTITY_ID_BUFFER_SIZE - 1 - 7, 'x');
  EXPECT_EQ(set_input(OTHER_KEY, longest.c_str()), wizard_input_other());
  EXPECT_EQ(std::string(wizard_input_other()), longest);
}

TEST(DeviceWizard, SetInputFindsTheInputByItsKey) {
  EXPECT_EQ(set_input(WEATHER_KEY, "weather.home"), wizard_input_weather());
  EXPECT_STREQ(wizard_input_weather(), "weather.home");
  EXPECT_EQ(set_input(OTHER_KEY, "weather.away"), wizard_input_other());
  EXPECT_STREQ(wizard_input_weather(), "weather.home");
}

TEST(DeviceWizard, SetInputIgnoresWhatIsNotAnEntityId) {
  std::string before = wizard_input_weather();
  EXPECT_EQ(set_input(WEATHER_KEY, ""), nullptr);
  EXPECT_EQ(set_input(WEATHER_KEY, "nodot"), nullptr);
  std::string too_long = "sensor." + std::string(WIZARD_ENTITY_ID_BUFFER_SIZE, 'x');
  EXPECT_EQ(set_input(WEATHER_KEY, too_long.c_str()), nullptr);
  EXPECT_EQ(set_input(0xdeadbeef, "sensor.a"), nullptr);
  EXPECT_EQ(std::string(wizard_input_weather()), before);
}

TEST(DeviceWizard, StandaloneInputReadsTheBufferTheWizardWrites) {
  strcpy(wizard_input_other(), "");
  WizardInput input(wizard_input_other());
  EXPECT_FALSE(input.has_entity_id());
  EXPECT_TRUE(input.entity_id().empty());

  ASSERT_EQ(set_input(OTHER_KEY, "weather.home"), wizard_input_other());
  EXPECT_TRUE(input.has_entity_id());
  EXPECT_EQ(input.entity_id(), "weather.home");
}

}  // namespace esphome::api
#endif  // USE_HOST
