#include "gtest/gtest.h"
#include "esphome/components/espectre/text/target_mac_text.h"

#ifdef ESPECTRE_SDK_TEST_DOUBLE

#include "esphome/components/logger/logger.h"

namespace esphome::espectre::testing {

class ESPectreTargetMacTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Each test starts without the target an earlier test saved.
    if (global_preferences != nullptr)
      global_preferences->reset();
    add_text_(component_, text_);
  }

  static void add_text_(ESPectreComponent &component, TargetMacText &text) {
    text.traits.set_min_length(4);
    text.traits.set_max_length(17);
    component.set_target_mac_text(&text);
  }

  const std::string &target_() const { return sdk_->config_.traffic_generator_target_mac; }

  ESPectreComponent component_;
  ::espectre::RuntimeFrontendController *sdk_{::espectre::RuntimeFrontendController::instance};
  TargetMacText text_{&component_};
};

TEST_F(ESPectreTargetMacTest, RequestAppliesInLoop) {
  component_.setup();
  EXPECT_EQ(text_.state, "auto");
  text_.make_call().set_value("AA:BB:CC:DD:EE:02").perform();
  EXPECT_EQ(target_(), "auto");
  EXPECT_EQ(text_.state, "auto");
  component_.loop();
  // The runtime stores lower-case hex digits, and the text shows what the runtime stored.
  EXPECT_EQ(target_(), "aa:bb:cc:dd:ee:02");
  EXPECT_EQ(text_.state, "aa:bb:cc:dd:ee:02");

  component_.request_traffic_generator_target_mac("auto");
  component_.loop();
  EXPECT_EQ(target_(), "auto");
  EXPECT_EQ(text_.state, "auto");
}

TEST_F(ESPectreTargetMacTest, RejectedRequestKeepsTheTarget) {
  component_.setup();
  component_.request_traffic_generator_target_mac("aa:bb:cc:dd:ee:02");
  component_.loop();
  int publishes = 0;
  text_.add_on_state_callback([&publishes](const std::string &) { publishes++; });
  // Multicast, all zero, not a MAC address, and too long to be one.
  for (const char *target :
       {"01:00:5e:00:00:01", "00:00:00:00:00:00", "automatic", "aa:bb:cc:dd:ee:02:03", "aa:bb:cc:dd:ee:0203"}) {
    sdk_->rejected_target_mac = target;
    component_.request_traffic_generator_target_mac(target);
    component_.loop();
    EXPECT_EQ(target_(), "aa:bb:cc:dd:ee:02") << target;
    EXPECT_EQ(text_.state, "aa:bb:cc:dd:ee:02") << target;
  }
  // Each rejection republishes the active target, so the entity drops the value it was asked for.
  EXPECT_EQ(publishes, 5);

  component_.loop();
  EXPECT_EQ(publishes, 5);
}

TEST_F(ESPectreTargetMacTest, EmbeddedNulKeepsActiveAndSavedTarget) {
  constexpr char target[] = "aa:bb:cc:dd:ee:02";
  constexpr char malformed[] = "auto\0suffix";
  component_.setup();
  text_.make_call().set_value(target).perform();
  component_.loop();
  ASSERT_EQ(target_(), target);

  text_.make_call().set_value(malformed, sizeof(malformed) - 1).perform();
  component_.loop();
  EXPECT_NE(sdk_->last_target_mac, "auto");
  EXPECT_EQ(target_(), target);
  EXPECT_EQ(text_.state, target);

  ESPectreComponent restarted;
  TargetMacText restored_text{&restarted};
  add_text_(restarted, restored_text);
  restarted.setup();
  EXPECT_EQ(restored_text.state, target);
}

TEST_F(ESPectreTargetMacTest, RequestWithoutText) {
  ESPectreComponent component;
  component.setup();
  component.request_traffic_generator_target_mac("aa:bb:cc:dd:ee:02");
  component.loop();
  EXPECT_EQ(::espectre::RuntimeFrontendController::instance->config_.traffic_generator_target_mac, "aa:bb:cc:dd:ee:02");
}

TEST_F(ESPectreTargetMacTest, RequestDuringApplyWaitsForNextLoop) {
  component_.setup();
  constexpr char first[] = "aa:bb:cc:dd:ee:02";
  // A log automation can issue another command from inside the SDK setter.
  sdk_->target_mac_hook = [this]() { text_.make_call().set_value("auto").perform(); };
  text_.make_call().set_value(first).perform();
  component_.loop();
  EXPECT_EQ(sdk_->active_target_mac_, first);
  EXPECT_EQ(text_.state, first);
  TargetMacPreference saved{};
  ASSERT_TRUE(text_.make_entity_preference<TargetMacPreference>().load(&saved));
  EXPECT_STREQ(saved.target.data(), first);

  sdk_->target_mac_hook = nullptr;
  component_.loop();
  EXPECT_EQ(sdk_->active_target_mac_, "auto");
  EXPECT_EQ(text_.state, "auto");
}

TEST_F(ESPectreTargetMacTest, PublishesActiveTargetWhenAnotherTargetIsStaged) {
  component_.setup();
  component_.set_traffic_generator_target_mac("aa:bb:cc:dd:ee:04");
  text_.make_call().set_value("AA:BB:CC:DD:EE:02").perform();
  component_.loop();
  EXPECT_EQ(sdk_->active_target_mac_, "aa:bb:cc:dd:ee:02");
  EXPECT_EQ(target_(), "aa:bb:cc:dd:ee:04");
  EXPECT_EQ(text_.state, sdk_->active_target_mac_);

  sdk_->rejected_target_mac = "01:00:5e:00:00:01";
  text_.make_call().set_value(sdk_->rejected_target_mac).perform();
  component_.loop();
  EXPECT_EQ(text_.state, "aa:bb:cc:dd:ee:02");
}

TEST_F(ESPectreTargetMacTest, SavedTargetRestoresUntilYamlChanges) {
  component_.setup();
  component_.request_traffic_generator_target_mac("AA:BB:CC:DD:EE:02");
  component_.loop();

  ESPectreComponent same_yaml;
  add_text_(same_yaml, text_);
  same_yaml.setup();
  EXPECT_EQ(::espectre::RuntimeFrontendController::instance->config_at_setup_.traffic_generator_target_mac,
            "aa:bb:cc:dd:ee:02");
  EXPECT_EQ(text_.state, "aa:bb:cc:dd:ee:02");

  // A rejected request does not replace the saved target.
  auto *same_sdk = ::espectre::RuntimeFrontendController::instance;
  same_sdk->rejected_target_mac = "01:00:5e:00:00:01";
  same_yaml.request_traffic_generator_target_mac(same_sdk->rejected_target_mac);
  same_yaml.loop();
  ESPectreComponent after_rejection;
  add_text_(after_rejection, text_);
  after_rejection.setup();
  EXPECT_EQ(::espectre::RuntimeFrontendController::instance->config_at_setup_.traffic_generator_target_mac,
            "aa:bb:cc:dd:ee:02");

  ESPectreComponent changed_yaml;
  changed_yaml.set_traffic_generator_target_mac("aa:bb:cc:dd:ee:04");
  add_text_(changed_yaml, text_);
  changed_yaml.setup();
  EXPECT_EQ(::espectre::RuntimeFrontendController::instance->config_at_setup_.traffic_generator_target_mac,
            "aa:bb:cc:dd:ee:04");
  EXPECT_EQ(text_.state, "aa:bb:cc:dd:ee:04");

  ESPectreComponent original_yaml;
  add_text_(original_yaml, text_);
  original_yaml.setup();
  EXPECT_EQ(text_.state, "auto");
}

TEST_F(ESPectreTargetMacTest, RejectedSavedTargetWarnsAndRestoresYaml) {
  constexpr char yaml_target[] = "aa:bb:cc:dd:ee:04";
  const TargetMacPreference invalid{fnv1a_hash(yaml_target), {"01:00:5e:00:00:01"}};
  sdk_->rejected_target_mac = invalid.target.data();
  auto pref = text_.make_entity_preference<TargetMacPreference>();
  ASSERT_TRUE(pref.save(&invalid));
  component_.set_traffic_generator_target_mac(yaml_target);

  const auto baud_rate = logger::global_logger->get_baud_rate();
  logger::global_logger->set_baud_rate(115200);
  ::testing::internal::CaptureStdout();
  component_.setup();
  const std::string log = ::testing::internal::GetCapturedStdout();
  logger::global_logger->set_baud_rate(baud_rate);

  EXPECT_NE(log.find("Invalid saved target MAC"), std::string::npos) << log;
  EXPECT_EQ(text_.state, yaml_target);
  TargetMacPreference repaired{};
  ASSERT_TRUE(pref.load(&repaired));
  EXPECT_EQ(repaired.yaml_hash, invalid.yaml_hash);
  EXPECT_STREQ(repaired.target.data(), yaml_target);
}

}  // namespace esphome::espectre::testing

#endif  // ESPECTRE_SDK_TEST_DOUBLE
