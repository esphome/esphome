#include <gtest/gtest.h>

#include "esphome/components/event/event.h"

namespace esphome::event::testing {

static const char *const TYPES[] = {"pressed", "held"};

TEST(EventTypes, StaticTableIsViewed) {
  Event event;
  event.set_event_types_static(TYPES, 2);
  EXPECT_EQ(event.get_event_types().data(), TYPES);
  EXPECT_EQ(event.get_event_types().size(), 2u);
}

TEST(EventTypes, RuntimeListIsCopied) {
  Event event;
  event.set_event_types_static(TYPES, 2);
  event.set_event_types({"a", "b", "c"});
  EXPECT_NE(event.get_event_types().data(), TYPES);
  EXPECT_EQ(event.get_event_types().size(), 3u);
  EXPECT_STREQ(event.get_event_types()[2], "c");
}

TEST(EventTypes, CopiesAnotherEventAndResetsLastType) {
  Event source;
  source.set_event_types({"x", "y"});
  Event copy;
  copy.set_event_types_static(TYPES, 2);
  copy.trigger("held");
  ASSERT_TRUE(copy.has_event());
  copy.set_event_types(source.get_event_types());
  EXPECT_FALSE(copy.has_event());
  EXPECT_NE(copy.get_event_types().data(), source.get_event_types().data());
  EXPECT_STREQ(copy.get_event_types()[1], "y");
}

}  // namespace esphome::event::testing
