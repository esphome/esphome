#include <gtest/gtest.h>
#include "esphome/components/remote_base/raw_protocol.h"
#include "esphome/core/base_automation.h"

namespace esphome::remote_base::testing {

namespace {

// Records x from every firing of an on_raw trigger.
struct Recorder {
  explicit Recorder(RawTrigger *trigger) : automation(trigger) { this->automation.add_action(&this->action); }
  std::vector<RawTimings> frames;
  LambdaAction<RawTimings> action{[this](const RawTimings &x) { this->frames.push_back(x); }};
  Automation<RawTimings> automation;
};

}  // namespace

TEST(RawTriggerTest, CopiesFramesFromAReceiverBuffer) {
  RawTrigger trigger;
  Recorder recorder(&trigger);
  RemoteReceiverListener *listener = &trigger;

  static const int32_t LONG_FRAME[] = {100, -200, 300, -400};
  static const int32_t SHORT_FRAME[] = {500, -600};
  listener->on_receive(RemoteReceiveData(RawTimingsView(LONG_FRAME), 25, TOLERANCE_MODE_PERCENTAGE));
  listener->on_receive(RemoteReceiveData(RawTimingsView(SHORT_FRAME), 25, TOLERANCE_MODE_PERCENTAGE));

  ASSERT_EQ(recorder.frames.size(), 2u);
  EXPECT_EQ(recorder.frames[0], (RawTimings{100, -200, 300, -400}));
  EXPECT_EQ(recorder.frames[1], (RawTimings{500, -600}));
}

}  // namespace esphome::remote_base::testing
