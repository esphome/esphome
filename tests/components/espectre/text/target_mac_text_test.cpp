#include "gtest/gtest.h"
#include "esphome/components/espectre/espectre.h"
#include "esphome/components/text/text.h"

#ifdef ESPECTRE_SDK_TEST_DOUBLE

namespace esphome::espectre::testing {

// What text.ApplyText with apply="request_traffic_generator_target_mac(value)" generates
class TargetMacText final : public text::Text {
 public:
  explicit TargetMacText(ESPectreComponent *parent) : parent_(parent) {}

 protected:
  void control(const std::string &value) override { this->parent_->request_traffic_generator_target_mac(value); }
  ESPectreComponent *parent_;
};

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
  text_.make_call().set_value("aA:Bb:cC:Dd:eE:02").perform();
  EXPECT_EQ(target_(), "auto");
  EXPECT_EQ(text_.state, "auto");
  component_.loop();
  EXPECT_EQ(target_(), "aa:bb:cc:dd:ee:02");
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02");

  component_.request_traffic_generator_target_mac("auto");
  component_.loop();
  EXPECT_EQ(target_(), "auto");
  EXPECT_EQ(text_.state, "auto");
}

TEST_F(ESPectreTargetMacTest, RejectedRequestKeepsTheTarget) {
  component_.setup();
  component_.request_traffic_generator_target_mac("aa:bb:cc:dd:ee:02");
  component_.loop();
  size_t publishes = 0;
  text_.add_on_state_callback([&publishes](const std::string &) { publishes++; });
  sdk_->last_target_mac.clear();
  const std::array<std::string, 8> invalid_targets = {
      "01:00:5e:00:00:01",   "00:00:00:00:00:00", "automatic",         "aa:bb:cc:dd:ee:02:03",
      "aa:bb:cc:dd:ee:0203", "aa-bb-cc-dd-ee-02", "aa:bb:cc:dd:ee:0g", std::string("auto\0", 5)};
  for (const auto &target : invalid_targets) {
    component_.request_traffic_generator_target_mac(target);
    component_.loop();
    EXPECT_TRUE(sdk_->last_target_mac.empty()) << target;
    EXPECT_EQ(target_(), "aa:bb:cc:dd:ee:02") << target;
    EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02") << target;
  }
  // Each rejection republishes the active target, so the entity drops the value it was asked for.
  EXPECT_EQ(publishes, invalid_targets.size());

  component_.loop();
  EXPECT_EQ(publishes, invalid_targets.size());
}

TEST_F(ESPectreTargetMacTest, RequestDuringApplyWaitsForNextLoop) {
  component_.setup();
  constexpr char first[] = "aa:bb:cc:dd:ee:02";
  // A log automation can issue another command from inside the SDK setter.
  sdk_->target_mac_hook = [this]() { text_.make_call().set_value("auto").perform(); };
  text_.make_call().set_value(first).perform();
  component_.loop();
  EXPECT_EQ(sdk_->active_target_mac_, first);
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02");
  TargetMac saved{};
  ASSERT_TRUE(text_.make_entity_preference<TargetMac>(fnv1a_hash("auto")).load(&saved));
  EXPECT_EQ(saved, (TargetMac{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02}));

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
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02");

  sdk_->rejected_target_mac = "02:00:00:00:00:01";
  text_.make_call().set_value(sdk_->rejected_target_mac).perform();
  component_.loop();
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02");
}

TEST_F(ESPectreTargetMacTest, SavedTargetIsKeyedByYamlTarget) {
  component_.setup();
  component_.request_traffic_generator_target_mac("AA:BB:CC:DD:EE:02");
  component_.loop();

  ESPectreComponent same_yaml;
  add_text_(same_yaml, text_);
  same_yaml.setup();
  EXPECT_EQ(::espectre::RuntimeFrontendController::instance->config_at_setup_.traffic_generator_target_mac,
            "aa:bb:cc:dd:ee:02");
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02");

  // A rejected request does not replace the saved target.
  auto *same_sdk = ::espectre::RuntimeFrontendController::instance;
  same_sdk->rejected_target_mac = "02:00:00:00:00:01";
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
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:04");

  // Like the other controls, returning to the original YAML brings back what was saved for it.
  ESPectreComponent original_yaml;
  add_text_(original_yaml, text_);
  original_yaml.setup();
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:02");
}

TEST_F(ESPectreTargetMacTest, RejectedSavedTargetUsesYaml) {
  constexpr char yaml_target[] = "aa:bb:cc:dd:ee:04";
  const TargetMac invalid{0x01, 0x00, 0x5e, 0x00, 0x00, 0x01};
  sdk_->rejected_target_mac = "01:00:5e:00:00:01";
  ASSERT_TRUE(text_.make_entity_preference<TargetMac>(fnv1a_hash(yaml_target)).save(&invalid));
  component_.set_traffic_generator_target_mac(yaml_target);
  component_.setup();
  EXPECT_EQ(text_.state, "AA:BB:CC:DD:EE:04");
}

}  // namespace esphome::espectre::testing

#endif  // ESPECTRE_SDK_TEST_DOUBLE
