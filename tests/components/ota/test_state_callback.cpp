#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/ota/ota_backend.h"

namespace esphome::ota::testing {

struct Event {
  char source;
  OTAState state;
  float progress;
  uint8_t error;
};

class TestOTAComponent : public OTAComponent {
 public:
  using OTAComponent::notify_state_;
  void fail() { this->mark_failed(); }
};

class RecordingListener : public OTAStateListener {
 public:
  void on_ota_state(OTAState state, float progress, uint8_t error) override;
};

// The callback list is shared by every OTA component and cannot be cleared, so the
// callbacks are registered once, each filtering on its component like the generated ones.
static TestOTAComponent first_ota;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static TestOTAComponent second_ota;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static TestOTAComponent failed_ota;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<Event> events;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static RecordingListener listener;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void RecordingListener::on_ota_state(OTAState state, float progress, uint8_t error) {
  events.push_back({'l', state, progress, error});
}

class OTAStateCallback : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    OTAComponent::add_state_callback([](OTAState state, float progress, uint8_t error, OTAComponent *component) {
      if (component == &first_ota)
        events.push_back({'a', state, progress, error});
    });
    OTAComponent::add_state_callback([](OTAState state, float progress, uint8_t error, OTAComponent *component) {
      if (component == &first_ota)
        events.push_back({'b', state, progress, error});
    });
    OTAComponent::add_state_callback([](OTAState state, float progress, uint8_t error, OTAComponent *component) {
      if (component == &second_ota)
        events.push_back({'s', state, progress, error});
    });
    OTAComponent::add_state_callback([](OTAState state, float progress, uint8_t error, OTAComponent *component) {
      if (component == &failed_ota)
        events.push_back({'f', state, progress, error});
    });
    first_ota.add_state_listener(&listener);
    failed_ota.add_state_listener(&listener);
    failed_ota.fail();
  }
  void SetUp() override { events.clear(); }
};

TEST_F(OTAStateCallback, ForwardsStateProgressAndError) {
  second_ota.notify_state_(OTA_IN_PROGRESS, 42.5f, 0);
  second_ota.notify_state_(OTA_ERROR, 0.0f, OTA_RESPONSE_ERROR_UPDATE_END);
  ASSERT_EQ(events.size(), 2U);
  EXPECT_EQ(events[0].source, 's');
  EXPECT_EQ(events[0].state, OTA_IN_PROGRESS);
  EXPECT_FLOAT_EQ(events[0].progress, 42.5f);
  EXPECT_EQ(events[1].state, OTA_ERROR);
  EXPECT_EQ(events[1].error, OTA_RESPONSE_ERROR_UPDATE_END);
}

TEST_F(OTAStateCallback, EveryCallbackRunsBeforeTheListeners) {
  first_ota.notify_state_(OTA_STARTED, 0.0f, 0);
  ASSERT_EQ(events.size(), 3U);
  EXPECT_EQ(events[0].source, 'a');
  EXPECT_EQ(events[1].source, 'b');
  EXPECT_EQ(events[2].source, 'l');
}

TEST_F(OTAStateCallback, SkippedWhileFailedButListenersStillRun) {
  failed_ota.notify_state_(OTA_STARTED, 0.0f, 0);
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].source, 'l');
}

}  // namespace esphome::ota::testing
