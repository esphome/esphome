#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_wizard.h"
#include "esphome/components/api/proto.h"
#include "esphome/components/host/preferences.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/preferences.h"

namespace esphome::api {

// Entity with a settable key. Entities get their key from codegen in setup(), which a unit test does not run.
class WizardTestEntity : public EntityBase {
 public:
  void set_key(uint32_t key) { this->object_id_hash_ = key; }
};

static WizardTestEntity wizard_switch;

// RAM buffers of the inputs, as codegen defines them
static char wizard_input_weather[WIZARD_ENTITY_ID_BUFFER_SIZE] = "sensor.default";
static char wizard_input_other[WIZARD_ENTITY_ID_BUFFER_SIZE] = "";

static constexpr uint32_t WEATHER_KEY = 0x0a0b0c0d;
static constexpr uint32_t OTHER_KEY = 0x11223344;

// The same shapes the generated code emits.
static const char *const WIZARD_DOMAINS[] = {"weather", "sensor"};
static const char *const WIZARD_DEVICE_CLASSES[] = {"temperature"};
static const char *const WIZARD_FEATURES[] = {"weather.WeatherEntityFeature.FORECAST_DAILY"};
static const WizardFilterRow WIZARD_FILTERS[] = {
    {"met", {WIZARD_DOMAINS, 2}, {WIZARD_DEVICE_CLASSES, 1}, {WIZARD_FEATURES, 1}},
    {nullptr, {WIZARD_DOMAINS, 1}, {nullptr, 0}, {nullptr, 0}},
};
static const WizardEntityRow WIZARD_ENTITIES[] = {
    {[]() -> EntityBase * { return &wizard_switch; }, "Enable"},
    {[]() -> EntityBase * { return &wizard_switch; }, nullptr},
};
static const WizardInputRow WIZARD_INPUTS[] = {
    {WEATHER_KEY, wizard_input_weather, "Weather", {WIZARD_FILTERS, 2}},
    {OTHER_KEY, wizard_input_other, nullptr, {nullptr, 0}},
};
static const WizardPageRow WIZARD_PAGES[] = {
    {"Setup", "Pick", {WIZARD_ENTITIES, 2}, {nullptr, 0}},
    {nullptr, nullptr, {nullptr, 0}, {WIZARD_INPUTS, 2}},
};
const WizardView<WizardPage, WizardPageRow> API_WIZARD_PAGES = {WIZARD_PAGES, 2};

using Bytes = std::vector<uint8_t>;

static Bytes cat(std::initializer_list<Bytes> parts) {
  Bytes out;
  for (const auto &part : parts)
    out.insert(out.end(), part.begin(), part.end());
  return out;
}

static Bytes length_prefixed(uint8_t number, const Bytes &body) {
  Bytes out{static_cast<uint8_t>(number << 3 | 2)};
  size_t length = body.size();
  while (length >= 0x80) {
    out.push_back(static_cast<uint8_t>(length | 0x80));
    length >>= 7;
  }
  out.push_back(static_cast<uint8_t>(length));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

static Bytes str_field(uint8_t number, const std::string &value) {
  return length_prefixed(number, Bytes(value.begin(), value.end()));
}

static Bytes msg_field(uint8_t number, const Bytes &body) { return length_prefixed(number, body); }

static Bytes key_field(uint32_t key) {
  return {0x0d, static_cast<uint8_t>(key), static_cast<uint8_t>(key >> 8), static_cast<uint8_t>(key >> 16),
          static_cast<uint8_t>(key >> 24)};
}

static Bytes encode(const ProtoMessage &msg, uint32_t (*calc)(const void *),
                    uint8_t *(*enc)(const void *, ProtoWriteBuffer &PROTO_ENCODE_DEBUG_PARAM)) {
  APIBuffer buffer;
  uint32_t size = calc(&msg);
  EXPECT_TRUE(buffer.resize(size));
  ProtoWriteBuffer writer(&buffer, 0);
#ifdef ESPHOME_DEBUG_API
  uint8_t *proto_debug_end_ = buffer.data() + buffer.size();
#endif
  uint8_t *end = enc(&msg, writer PROTO_ENCODE_DEBUG_ARG);
  EXPECT_EQ(static_cast<size_t>(end - buffer.data()), size);
  return Bytes(buffer.data(), buffer.data() + size);
}

TEST(DeviceWizard, EncodesEveryPageFromFlashTables) {
  wizard_switch.set_key(0x01020304);

  DeviceWizardResponse resp;
  resp.pages = &API_WIZARD_PAGES;

  Bytes filter_met = cat({str_field(1, "met"), str_field(2, "weather"), str_field(2, "sensor"),
                          str_field(3, "temperature"), str_field(4, "weather.WeatherEntityFeature.FORECAST_DAILY")});
  Bytes filter_domain = str_field(2, "weather");
  Bytes page_one =
      cat({str_field(1, "Setup"), str_field(2, "Pick"),
           msg_field(3, cat({key_field(0x01020304), str_field(3, "Enable")})), msg_field(3, key_field(0x01020304))});
  Bytes page_two = cat({msg_field(4, cat({key_field(WEATHER_KEY), str_field(2, "Weather"), msg_field(3, filter_met),
                                          msg_field(3, filter_domain), str_field(4, "sensor.default")})),
                        msg_field(4, key_field(OTHER_KEY))});
  Bytes expected = cat({msg_field(1, page_one), msg_field(1, page_two)});

  EXPECT_EQ(encode(resp, &DeviceWizardResponse::calc_size_msg, &DeviceWizardResponse::encode_msg), expected);
}

TEST(DeviceWizard, CopiesFlashStringsAndTruncatesToTheBuffer) {
  char buffer[8];
  EXPECT_EQ(wizard_copy_flash_string("abc", buffer, sizeof(buffer)), 3u);
  EXPECT_STREQ(buffer, "abc");
  EXPECT_EQ(wizard_copy_flash_string("abcdefghij", buffer, sizeof(buffer)), 7u);
  EXPECT_STREQ(buffer, "abcdefg");
  EXPECT_EQ(wizard_copy_flash_string(nullptr, buffer, sizeof(buffer)), 0u);
  EXPECT_STREQ(buffer, "");
}

// A request for the input with the given key and entity id
static const char *set_input(uint32_t key, const char *entity_id) {
  WizardInputSetRequest request;
  request.key = key;
  request.entity_id = StringRef(entity_id);
  return wizard_set_input(request);
}

// Saved entity ids are kept by the host preferences, which also write a file, so every test starts and ends clean
class DeviceWizardPreferences : public ::testing::Test {
 protected:
  void SetUp() override {
    host::setup_preferences();
    // Reading makes the host load its file; resetting and syncing then empties both the memory and the file
    uint8_t unused;
    host::get_preferences()->load(0, &unused, 1);
    this->clear_();
  }
  void TearDown() override { this->clear_(); }
  void clear_() {
    host::get_preferences()->reset();
    host::get_preferences()->sync();
  }
};

TEST_F(DeviceWizardPreferences, SetInputStoresTheEntityIdInTheBuffer) {
  EXPECT_EQ(set_input(OTHER_KEY, "sensor.outdoor"), wizard_input_other);
  EXPECT_STREQ(wizard_input_other, "sensor.outdoor");
  // A shorter id replaces a longer one completely
  EXPECT_EQ(set_input(OTHER_KEY, "light.a"), wizard_input_other);
  EXPECT_STREQ(wizard_input_other, "light.a");
  // The longest valid id fills the buffer
  std::string longest = "sensor." + std::string(WIZARD_ENTITY_ID_BUFFER_SIZE - 1 - 7, 'x');
  EXPECT_EQ(set_input(OTHER_KEY, longest.c_str()), wizard_input_other);
  EXPECT_EQ(std::string(wizard_input_other), longest);
}

TEST_F(DeviceWizardPreferences, SetInputIgnoresWhatIsNotAnEntityId) {
  std::string before = wizard_input_weather;
  EXPECT_EQ(set_input(WEATHER_KEY, ""), nullptr);
  EXPECT_EQ(set_input(WEATHER_KEY, "nodot"), nullptr);
  std::string too_long = "sensor." + std::string(WIZARD_ENTITY_ID_BUFFER_SIZE, 'x');
  EXPECT_EQ(set_input(WEATHER_KEY, too_long.c_str()), nullptr);
  EXPECT_EQ(set_input(0xdeadbeef, "sensor.a"), nullptr);
  EXPECT_EQ(std::string(wizard_input_weather), before);
}

TEST_F(DeviceWizardPreferences, SetupLoadsSavedEntityIdsOverTheDefaults) {
  wizard_setup();
  EXPECT_STREQ(wizard_input_weather, "sensor.default");

  ASSERT_NE(set_input(WEATHER_KEY, "sensor.chosen"), nullptr);
  strcpy(wizard_input_weather, "sensor.default");
  wizard_setup();
  EXPECT_STREQ(wizard_input_weather, "sensor.chosen");

  // A saved value that is no entity id is ignored
  const char *garbage = "no dot here";
  uint8_t saved[WIZARD_ENTITY_ID_BUFFER_SIZE - 1] = {};
  memcpy(saved, garbage, strlen(garbage));
  ASSERT_TRUE(global_preferences->save(WEATHER_KEY ^ 0x57495A44, saved, sizeof(saved)));
  strcpy(wizard_input_weather, "sensor.default");
  wizard_setup();
  EXPECT_STREQ(wizard_input_weather, "sensor.default");
}

TEST_F(DeviceWizardPreferences, StandaloneInputReadsTheBufferTheWizardWrites) {
  strcpy(wizard_input_other, "");
  WizardInput input(wizard_input_other);
  EXPECT_FALSE(input.has_entity_id());
  EXPECT_TRUE(input.entity_id().empty());

  ASSERT_EQ(set_input(OTHER_KEY, "weather.home"), wizard_input_other);
  EXPECT_TRUE(input.has_entity_id());
  EXPECT_EQ(input.entity_id(), "weather.home");

  // A saved id comes back through the object after a restart
  strcpy(wizard_input_other, "");
  wizard_setup();
  EXPECT_EQ(input.entity_id(), "weather.home");
}

TEST(DeviceWizard, CapabilitiesAnnounceTheWizard) {
  DeviceCapabilitiesResponse resp;
  EXPECT_TRUE(
      encode(resp, &DeviceCapabilitiesResponse::calc_size_msg, &DeviceCapabilitiesResponse::encode_msg).empty());
  resp.wizard.configured = true;
  EXPECT_EQ(encode(resp, &DeviceCapabilitiesResponse::calc_size_msg, &DeviceCapabilitiesResponse::encode_msg),
            msg_field(5, Bytes{0x08, 0x01}));
}

TEST(DeviceWizard, EmptyViewEncodesNothing) {
  static const WizardView<WizardPage, WizardPageRow> empty = {nullptr, 0};
  DeviceWizardResponse resp;
  resp.pages = &empty;
  EXPECT_EQ(resp.calculate_size(), 0u);
  EXPECT_TRUE(encode(resp, &DeviceWizardResponse::calc_size_msg, &DeviceWizardResponse::encode_msg).empty());
}

}  // namespace esphome::api
