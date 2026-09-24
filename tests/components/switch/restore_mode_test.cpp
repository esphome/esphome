#ifdef USE_HOST
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "esphome/components/host/preferences.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::switch_::testing {
namespace fs = std::filesystem;

/// RAII helper to save and restore an environment variable. Copied from tests/components/host/preferences_test.cpp
/// into this component's namespace so the two copies can diverge without becoming an ODR violation.
class ScopedEnvVar {
 public:
  explicit ScopedEnvVar(const char *name) : name_(name) {
    const char *val = getenv(name);
    if (val != nullptr) {
      this->saved_value_ = val;
      this->was_set_ = true;
    }
  }
  ~ScopedEnvVar() {
    if (this->was_set_) {
      setenv(this->name_.c_str(), this->saved_value_.c_str(), 1);
    } else {
      unsetenv(this->name_.c_str());
    }
  }
  ScopedEnvVar(const ScopedEnvVar &) = delete;
  ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

 private:
  std::string name_;
  std::string saved_value_;
  bool was_set_{false};
};

/// Switch is abstract; this is the minimum needed to instantiate one.
class TestSwitch : public Switch {
 public:
  void write_state(bool state) override { this->last_written = state; }
  /// The preference key is derived from object_id_hash_, so setting it directly is both
  /// sufficient and simpler than constructing a name with the right StringRef lifetime.
  /// Two instances sharing a value share stored state, which the persistence test relies on.
  void set_test_id(uint32_t hash) { this->object_id_hash_ = hash; }
  bool persistent() { return this->is_persistent_(); }
  /// True once get_initial_state() has bound rtc_: save() on an unbound preference returns false.
  bool rtc_bound() {
    bool probe{false};
    return this->rtc_.save(&probe);
  }
  bool last_written{false};
};

class SwitchRestoreModeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_switch_restore_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_switch_restore", strlen("test_switch_restore"), "", 0);
    host::setup_preferences();
    // Host preferences also keep an in-memory store, which would leak between tests and GTEST_REPEAT iterations.
    host::host_preferences->reset();
  }

  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  static void set_cause(const char *cause) {
    if (cause == nullptr) {
      unsetenv("ESPHOME_RESET_CAUSE");
    } else {
      setenv("ESPHOME_RESET_CAUSE", cause, 1);
    }
  }

  fs::path temp_dir_;
};

// Interleaved (cause, mode) pairs, exactly as the code generator emits them.
static const uint8_t SOFTWARE_TO_RESTORE_OFF[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_SOFTWARE),
    static_cast<uint8_t>(SWITCH_RESTORE_DEFAULT_OFF),
};
// Two entries, with the cause under test deliberately SECOND, so that the interleaved stride
// arithmetic in effective_restore_mode_() and is_persistent_() is actually exercised. With a
// single-entry table only index 0 is ever read and a wrong-byte read would go unnoticed.
static const uint8_t WATCHDOG_THEN_SOFTWARE[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_WATCHDOG),
    static_cast<uint8_t>(SWITCH_ALWAYS_OFF),
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_SOFTWARE),
    static_cast<uint8_t>(SWITCH_RESTORE_DEFAULT_OFF),
};
static const uint8_t SOFTWARE_TO_ALWAYS_OFF[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_SOFTWARE),
    static_cast<uint8_t>(SWITCH_ALWAYS_OFF),
};
static const uint8_t SOFTWARE_TO_INVERTED_OFF[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_SOFTWARE),
    static_cast<uint8_t>(SWITCH_RESTORE_INVERTED_DEFAULT_OFF),
};
// PANIC (4) and BROWNOUT (5) lack RESTORE_MODE_PERSISTENT_MASK (0x02), and BROWNOUT has the ON bit.
// SOFTWARE (2) and WATCHDOG (3) happen to carry 0x02 -- SOFTWARE is even numerically equal to
// SWITCH_RESTORE_DEFAULT_OFF -- so tables built only from them cannot tell a cause byte from a mode
// byte. These tables are chosen so that every wrong-byte read changes the answer.
static const uint8_t PANIC_OFF_THEN_BROWNOUT_RESTORE[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_PANIC),
    static_cast<uint8_t>(SWITCH_ALWAYS_OFF),
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_BROWNOUT),
    static_cast<uint8_t>(SWITCH_RESTORE_DEFAULT_OFF),
};
static const uint8_t SOFTWARE_OFF_THEN_WATCHDOG_ON[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_SOFTWARE),
    static_cast<uint8_t>(SWITCH_ALWAYS_OFF),
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_WATCHDOG),
    static_cast<uint8_t>(SWITCH_ALWAYS_ON),
};
static const uint8_t WATCHDOG_OFF_THEN_BROWNOUT_OFF[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_WATCHDOG),
    static_cast<uint8_t>(SWITCH_ALWAYS_OFF),
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_BROWNOUT),
    static_cast<uint8_t>(SWITCH_ALWAYS_OFF),
};
static const uint8_t SOFTWARE_TO_DISABLED[] = {
    static_cast<uint8_t>(ResetCause::RESET_CAUSE_SOFTWARE),
    static_cast<uint8_t>(SWITCH_RESTORE_DISABLED),
};

TEST_F(SwitchRestoreModeTest, HostReportsTheCauseNamedInTheEnvironment) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  auto cause_for = [](const char *name) {
    set_cause(name);
    return arch_get_reset_cause();
  };
  EXPECT_EQ(cause_for(nullptr), ResetCause::RESET_CAUSE_UNKNOWN);
  EXPECT_EQ(cause_for("power_on"), ResetCause::RESET_CAUSE_POWER_ON);
  EXPECT_EQ(cause_for("software"), ResetCause::RESET_CAUSE_SOFTWARE);
  EXPECT_EQ(cause_for("watchdog"), ResetCause::RESET_CAUSE_WATCHDOG);
  EXPECT_EQ(cause_for("panic"), ResetCause::RESET_CAUSE_PANIC);
  EXPECT_EQ(cause_for("brownout"), ResetCause::RESET_CAUSE_BROWNOUT);
  EXPECT_EQ(cause_for("external"), ResetCause::RESET_CAUSE_EXTERNAL);
  EXPECT_EQ(cause_for("sleep_wake"), ResetCause::RESET_CAUSE_SLEEP_WAKE);
  EXPECT_EQ(cause_for("not-a-cause"), ResetCause::RESET_CAUSE_UNKNOWN);
}

// Existing ESP8266/rp2 preference slot layouts depend on a switch with no persistent mode allocating nothing.
TEST_F(SwitchRestoreModeTest, PreferenceIsAllocatedOnlyForAPersistentConfiguration) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("power_on");
  const SwitchRestoreMode non_persistent[] = {SWITCH_ALWAYS_OFF, SWITCH_ALWAYS_ON, SWITCH_RESTORE_DISABLED};
  uint32_t id = 0x4000;
  for (const SwitchRestoreMode mode : non_persistent) {
    TestSwitch sw;
    sw.set_test_id(id++);
    sw.set_restore_mode(mode);
    (void) sw.get_initial_state_with_restore_mode();
    EXPECT_FALSE(sw.rtc_bound()) << "restore mode " << static_cast<int>(mode) << " allocated a preference";
  }
  {
    TestSwitch sw;  // overrides that are all non-persistent
    sw.set_test_id(id++);
    sw.set_restore_mode(SWITCH_ALWAYS_ON);
    sw.set_restore_mode_on_reset(SOFTWARE_OFF_THEN_WATCHDOG_ON, 2);
    (void) sw.get_initial_state_with_restore_mode();
    EXPECT_FALSE(sw.rtc_bound()) << "non-persistent overrides allocated a preference";
  }
  TestSwitch sw;  // a persistent override on a boot it does not match
  sw.set_test_id(id++);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  (void) sw.get_initial_state_with_restore_mode();
  EXPECT_TRUE(sw.rtc_bound()) << "a persistent override did not allocate a preference";
}

// With no override configured, behaviour must be bit-for-bit what it was before.
TEST_F(SwitchRestoreModeTest, NoOverrideLeavesBehaviourUnchanged) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("software");
  TestSwitch sw;
  sw.set_test_id(0x1000);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value());
}

TEST_F(SwitchRestoreModeTest, OverrideAppliesForTheMatchingCause) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("software");
  TestSwitch sw;
  sw.set_test_id(0x1001);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  // RESTORE_DEFAULT_OFF with nothing stored yet defaults to OFF, so ALWAYS_ON is overridden.
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state.value());
}

TEST_F(SwitchRestoreModeTest, OverrideIgnoredForAnUnlistedCause) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("power_on");
  TestSwitch sw;
  sw.set_test_id(0x1002);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value());
}

// The safety property: a boot the platform cannot classify must behave as if the feature
// were not configured at all.
TEST_F(SwitchRestoreModeTest, UnknownCauseNeverSelectsAnOverride) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause(nullptr);
  TestSwitch sw;
  sw.set_test_id(0x1003);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value());
}

TEST_F(SwitchRestoreModeTest, DisabledAsOverrideDefersToTheComponent) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");

  {
    // Effective mode DISABLED on this boot: the component decides, so no initial state...
    set_cause("software");
    TestSwitch sw;
    sw.set_test_id(0x1004);
    sw.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_DISABLED, 1);
    EXPECT_FALSE(sw.get_initial_state_with_restore_mode().has_value());
    // ...but the preference must still have been bound before the DISABLED early return, so a
    // state published on this boot is saved for the next one.
    sw.publish_state(true);
    global_preferences->sync();
  }

  set_cause("power_on");  // not in the table: the persistent base applies
  TestSwitch sw2;
  sw2.set_test_id(0x1004);
  sw2.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_DISABLED, 1);
  auto restored = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(restored.has_value());
  EXPECT_TRUE(restored.value()) << "state published on a DISABLED boot was not saved: rtc_ was not bound "
                                   "before the DISABLED early return";
}

// An ALWAYS_ON base with a persistent override must still save and restore the state.
TEST_F(SwitchRestoreModeTest, PersistenceFollowsTheOverrideNotJustTheBaseMode) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("software");

  {
    TestSwitch sw;
    sw.set_test_id(0xA5A5A5A5);
    sw.set_restore_mode(SWITCH_ALWAYS_ON);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
    // Binds rtc_ first, exactly as Component::setup() does, then records a state.
    auto first = sw.get_initial_state_with_restore_mode();
    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE(first.value());
    sw.publish_state(true);
    global_preferences->sync();
  }

  TestSwitch sw2;
  sw2.set_test_id(0xA5A5A5A5);
  sw2.set_restore_mode(SWITCH_ALWAYS_ON);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  auto restored = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(restored.has_value());
  EXPECT_TRUE(restored.value()) << "state saved under a persistent override was not restored";
}

// rtc_ binding must depend only on the configuration: a state saved on a boot the override does not match must
// survive to the next boot that it does match.
TEST_F(SwitchRestoreModeTest, StateSavedOnANonMatchingBootIsStillPersisted) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");

  {
    // Cause deliberately NOT in the table: the effective mode is the non-persistent ALWAYS_ON.
    set_cause("power_on");
    TestSwitch sw;
    sw.set_test_id(0xB6B6B6B6);
    sw.set_restore_mode(SWITCH_ALWAYS_ON);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
    auto first = sw.get_initial_state_with_restore_mode();
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(first.value()) << "ALWAYS_ON should apply when no override matches";
    sw.publish_state(false);
    sw.publish_state(true);
    global_preferences->sync();
  }

  // Now a boot whose cause DOES match, so the persistent override applies and must see the
  // state written during the previous, non-matching boot.
  set_cause("software");
  TestSwitch sw2;
  sw2.set_test_id(0xB6B6B6B6);
  sw2.set_restore_mode(SWITCH_ALWAYS_ON);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  auto restored = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(restored.has_value());
  EXPECT_TRUE(restored.value()) << "state saved on a non-matching boot was dropped: rtc_ was "
                                   "bound on the effective mode instead of the configuration";
}

TEST_F(SwitchRestoreModeTest, SecondTableEntryIsFoundAndStrideIsCorrect) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("software");
  TestSwitch sw;
  sw.set_test_id(0x2001);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(WATCHDOG_THEN_SOFTWARE, 2);
  // Must find entry 1 (software -> RESTORE_DEFAULT_OFF), not entry 0, and must not read a
  // cause byte where a mode byte lives.
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state.value()) << "second table entry was not matched, or the stride is wrong";
}

TEST_F(SwitchRestoreModeTest, FirstTableEntryStillMatchesWithMultipleEntries) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("watchdog");
  TestSwitch sw;
  sw.set_test_id(0x2002);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(WATCHDOG_THEN_SOFTWARE, 2);
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state.value()) << "ALWAYS_OFF override for the first entry did not apply";
}

// A persistent base with an override table where no entry matches this boot: the base mode alone
// decides, the first boot defaults OFF with nothing stored, and the next boot restores the stored ON.
TEST_F(SwitchRestoreModeTest, PersistentBaseModeStillRestoresWhenNoOverrideMatches) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");

  {
    set_cause("power_on");  // not in the table
    TestSwitch sw;
    sw.set_test_id(0x3001);
    sw.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_ALWAYS_OFF, 1);
    auto first = sw.get_initial_state_with_restore_mode();
    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE(first.value()) << "persistent base with nothing stored should default OFF";
    sw.publish_state(true);
    global_preferences->sync();
  }

  set_cause("power_on");
  TestSwitch sw2;
  sw2.set_test_id(0x3001);
  sw2.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_ALWAYS_OFF, 1);
  auto restored = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(restored.has_value());
  EXPECT_TRUE(restored.value()) << "persistent base did not restore the stored state";
}

// The override is applied through the same masks as restore_mode, so an inverted override must
// invert the *stored* value. This is the only test that executes the INVERTED branch.
TEST_F(SwitchRestoreModeTest, InvertedOverrideInvertsTheStoredValue) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");

  {
    set_cause("software");
    TestSwitch sw;
    sw.set_test_id(0x3002);
    sw.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_INVERTED_OFF, 1);
    (void) sw.get_initial_state_with_restore_mode();
    sw.publish_state(true);
    global_preferences->sync();
  }

  set_cause("software");
  TestSwitch sw2;
  sw2.set_test_id(0x3002);
  sw2.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_INVERTED_OFF, 1);
  auto restored = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(restored.has_value());
  EXPECT_FALSE(restored.value()) << "inverted override did not invert the stored ON state";
}

// (1) is_persistent_() must read MODE bytes. Entry 0 is non-persistent and entry 1 is persistent,
// and neither cause byte carries 0x02, so reading causes, reading the wrong stride, or reading only
// entry 0 all give false.
TEST_F(SwitchRestoreModeTest, IsPersistentReadsModeBytesAcrossAllEntries) {
  TestSwitch sw;
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(PANIC_OFF_THEN_BROWNOUT_RESTORE, 2);
  EXPECT_TRUE(sw.persistent()) << "entry 1's persistent mode was not seen";
}

// ...and the converse: both modes non-persistent, both causes carry 0x02, so reading cause bytes
// instead of mode bytes would wrongly report persistent.
TEST_F(SwitchRestoreModeTest, IsPersistentIgnoresCauseBytes) {
  TestSwitch sw;
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(SOFTWARE_OFF_THEN_WATCHDOG_ON, 2);
  EXPECT_FALSE(sw.persistent()) << "a cause byte was read as a mode byte";
}

// (2) effective_restore_mode_() must return entry 1's MODE byte. The adjacent byte is BROWNOUT (5),
// which has the ON bit, so reading it instead of ALWAYS_OFF would switch the load on.
TEST_F(SwitchRestoreModeTest, EffectiveModeReturnsTheModeByteOfTheMatchingEntry) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  set_cause("brownout");
  TestSwitch sw;
  sw.set_test_id(0x2003);
  sw.set_restore_mode(SWITCH_ALWAYS_ON);
  sw.set_restore_mode_on_reset(WATCHDOG_OFF_THEN_BROWNOUT_OFF, 2);
  auto state = sw.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state.value()) << "a byte other than entry 1's mode byte was returned";
}

// (3) Because get_initial_state() is gated on the configuration, a stored value can exist on a boot
// whose effective mode is NOT persistent. Such a mode must ignore it.
TEST_F(SwitchRestoreModeTest, NonPersistentBaseIgnoresAStoredState) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  {
    set_cause("software");  // persistent override applies: store OFF
    TestSwitch sw;
    sw.set_test_id(0x3003);
    sw.set_restore_mode(SWITCH_ALWAYS_ON);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
    (void) sw.get_initial_state_with_restore_mode();
    sw.publish_state(true);
    sw.publish_state(false);
    global_preferences->sync();
  }
  set_cause("power_on");  // not in the table: ALWAYS_ON, which must not consult the stored OFF
  TestSwitch sw2;
  sw2.set_test_id(0x3003);
  sw2.set_restore_mode(SWITCH_ALWAYS_ON);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_RESTORE_OFF, 1);
  auto state = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value()) << "ALWAYS_ON restored a stored state";
}

TEST_F(SwitchRestoreModeTest, NonPersistentOverrideIgnoresAStoredState) {
  ScopedEnvVar guard("ESPHOME_RESET_CAUSE");
  {
    set_cause("power_on");  // not in the table: persistent base applies, store ON
    TestSwitch sw;
    sw.set_test_id(0x3004);
    sw.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
    sw.set_restore_mode_on_reset(SOFTWARE_TO_ALWAYS_OFF, 1);
    (void) sw.get_initial_state_with_restore_mode();
    sw.publish_state(true);
    global_preferences->sync();
  }
  set_cause("software");  // ALWAYS_OFF override, which must not consult the stored ON
  TestSwitch sw2;
  sw2.set_test_id(0x3004);
  sw2.set_restore_mode(SWITCH_RESTORE_DEFAULT_OFF);
  sw2.set_restore_mode_on_reset(SOFTWARE_TO_ALWAYS_OFF, 1);
  auto state = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state.value()) << "an ALWAYS_OFF override restored a stored state";
}

}  // namespace esphome::switch_::testing
#endif  // USE_HOST
