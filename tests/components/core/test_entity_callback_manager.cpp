#include <gtest/gtest.h>

#include <string>

#include "esphome/core/entity_base.h"

namespace esphome::testing {

// Entity lists on MQTT builds. A listener that publishes again changes the member; later listeners
// must still see the value that was published to them, as with LazyCallbackManager.
TEST(InlineCallbackManager, LaterListenerSeesPublishedValue) {
  float state = 1.0f;
  float seen = 0.0f;
  InlineCallbackManager<void(float)> callbacks;
  callbacks.add([&state](float) { state = 2.0f; });
  callbacks.add([&seen](float value) { seen = value; });
  callbacks.call(state);
  EXPECT_EQ(seen, 1.0f);

  state = 1.0f;
  callbacks(state);
  EXPECT_EQ(seen, 1.0f);
}

TEST(InlineCallbackManager, ReferenceArgumentsPassThrough) {
  std::string state = "a";
  const std::string *seen = nullptr;
  InlineCallbackManager<void(const std::string &)> callbacks;
  callbacks.add([&seen](const std::string &value) { seen = &value; });
  callbacks.call(state);
  EXPECT_EQ(seen, &state);
  EXPECT_FALSE(callbacks.empty());
  EXPECT_EQ(callbacks.size(), 1u);
}

}  // namespace esphome::testing
