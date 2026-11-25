#include <gtest/gtest.h>

#include <string>
#include <type_traits>

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

TEST(InlineCallbackManager, EmptyManagerDispatchesNothing) {
  InlineCallbackManager<void(int)> callbacks;
  EXPECT_TRUE(callbacks.empty());
  EXPECT_EQ(callbacks.size(), 0u);
  callbacks.call(1);
  callbacks(2);
  EXPECT_TRUE(callbacks.empty());
}

// The inline list is only for MQTT builds, which always fill it; other builds keep the lazy pointer.
#ifdef USE_MQTT
static_assert(std::is_same_v<EntityCallbackManager<void(float)>, InlineCallbackManager<void(float)>>);
#else
static_assert(std::is_same_v<EntityCallbackManager<void(float)>, LazyCallbackManager<void(float)>>);
static_assert(sizeof(EntityCallbackManager<void(float)>) == sizeof(void *));
#endif

}  // namespace esphome::testing
