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

TEST(EventTypes, CopiesAnotherEventAndResetsLastType) {
  Event source;
  source.set_event_types({"x", "y"});
  Event copy;
  copy.set_event_types_static(TYPES, 2);
  copy.trigger("held");
  ASSERT_TRUE(copy.has_event());
  copy.set_event_types(source.get_event_types());
  EXPECT_FALSE(copy.has_event());
  EXPECT_NE(copy.get_event_types().data(), TYPES);
  EXPECT_NE(copy.get_event_types().data(), source.get_event_types().data());
  EXPECT_EQ(copy.get_event_types().size(), 2u);
  EXPECT_STREQ(copy.get_event_types()[1], "y");
}

TEST(EventTypes, CopiesItsOwnList) {
  Event event;
  event.set_event_types({"a", "b"});
  event.set_event_types(event.get_event_types());  // copies before freeing the old list
  EXPECT_EQ(event.get_event_types().size(), 2u);
  EXPECT_STREQ(event.get_event_types()[1], "b");
}

TEST(EventTypes, CopiesFromFixedVector) {
  FixedVector<const char *> types;
  types.init(2);
  types.push_back("on");
  types.push_back("off");
  Event event;
  event.set_event_types(types);
  EXPECT_NE(event.get_event_types().data(), types.begin());
  EXPECT_EQ(event.get_event_types().size(), 2u);
  EXPECT_STREQ(event.get_event_types()[0], "on");
}

}  // namespace esphome::event::testing
