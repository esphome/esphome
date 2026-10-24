#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "esphome/components/event/event.h"

namespace esphome::event::testing {

// A rotary dimmer: a press counts presses, a turn counts steps.
enum : uint8_t { NUMBER_OF_PRESSES, STEPS, SPEED, LONG_PRESS, ACTION };

static const char *const TYPES[] = {"press", "rotate_left"};
static const uint8_t PRESS_ONLY[] = {1, 0b01};  // 1 byte of bits; bit 0: event type 0, "press"
static const EventVariable VARIABLES[] = {
    {"number_of_presses", PRESS_ONLY, 0, EVENT_VARIABLE_TYPE_INT},
    {"steps", nullptr, 4, EVENT_VARIABLE_TYPE_INT},
    {"speed", nullptr, 8, EVENT_VARIABLE_TYPE_FLOAT},
    {"long_press", nullptr, 12, EVENT_VARIABLE_TYPE_BOOL},
    {"action", nullptr, 13, EVENT_VARIABLE_TYPE_STRING, 7},  // "start", "repeat"
};

struct Dimmer {
  Dimmer() {
    this->event.set_event_types_static(TYPES, 2);
    this->event.set_data(&this->block, VARIABLES, 5);
  }
  EventData data() const { return this->event.get_last_event_data(); }

  EventDataBlock<21> block{};  // two ints, a float, a bool and 7 + 1 bytes of text
  Event event;
};

TEST(EventData, KeepsTheValuesOfTheLastEvent) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{STEPS, 3}, {SPEED, 0.5f}, {LONG_PRESS, true}});
  EXPECT_EQ(dimmer.data().size(), 5);
  EXPECT_EQ(dimmer.data().get_int(STEPS), 3);
  EXPECT_EQ(dimmer.data().get_float(SPEED), 0.5f);
  EXPECT_TRUE(dimmer.data().get_bool(LONG_PRESS));
  EXPECT_STREQ(dimmer.data().variable(STEPS).name, "steps");
}

TEST(EventData, MissingValuesReadAsZeroNanFalseAndEmpty) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{STEPS, 3}, {SPEED, 0.5f}, {LONG_PRESS, true}, {ACTION, "start"}});
  dimmer.event.trigger("press");  // a new event without values
  EXPECT_FALSE(dimmer.data().has(STEPS));
  EXPECT_EQ(dimmer.data().get_int(STEPS), 0);
  EXPECT_TRUE(std::isnan(dimmer.data().get_float(SPEED)));
  EXPECT_FALSE(dimmer.data().get_bool(LONG_PRESS));
  EXPECT_TRUE(dimmer.data().get_string(ACTION).empty());
}

TEST(EventData, InvalidTypeKeepsTheLastValues) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{STEPS, 3}});
  dimmer.event.trigger("hold", {{STEPS, 5}});  // not one of the event types, so ignored
  EXPECT_EQ(dimmer.event.get_last_event_type(), "rotate_left");
  EXPECT_EQ(dimmer.data().get_int(STEPS), 3);
}

TEST(EventData, VariableOfAnotherEventTypeIsLeftOut) {
  Dimmer dimmer;
  // number_of_presses belongs to "press" only
  dimmer.event.trigger("rotate_left", {{NUMBER_OF_PRESSES, 2}, {STEPS, 3}});
  EXPECT_FALSE(dimmer.data().has(NUMBER_OF_PRESSES));
  EXPECT_EQ(dimmer.data().get_int(STEPS), 3);
  dimmer.event.trigger("press", {{NUMBER_OF_PRESSES, 2}});
  EXPECT_EQ(dimmer.data().get_int(NUMBER_OF_PRESSES), 2);
}

TEST(EventData, TypeAddedAtRunTimeCarriesNoRestrictedVariable) {
  // 1 byte of bits for the 2 configured types; the 0xff stands for whatever follows the mask in flash
  static constexpr uint8_t PRESS_THEN_MORE[] = {1, 0b01, 0xff};
  static constexpr EventVariable PRESSES[] = {{"number_of_presses", PRESS_THEN_MORE, 0, EVENT_VARIABLE_TYPE_INT}};
  EventDataBlock<4> block{};
  Event event;
  event.set_event_types_static(TYPES, 2);
  event.set_data(&block, PRESSES, 1);
  event.set_event_types({"press", "rotate_left", "a", "b", "c", "d", "e", "f", "hold"});
  event.trigger("hold", {{0, 1}});  // type 8, past the end of the bits
  EXPECT_FALSE(event.get_last_event_data().has(0));
}

TEST(EventData, ValueOfTheWrongTypeIsLeftOut) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{STEPS, 0.5f}, {LONG_PRESS, 1}});  // a float for an int, an int for a bool
  EXPECT_FALSE(dimmer.data().has(STEPS));
  EXPECT_FALSE(dimmer.data().has(LONG_PRESS));
}

TEST(EventData, FloatVariableTakesAnInteger) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{SPEED, 2}});
  EXPECT_EQ(dimmer.data().get_float(SPEED), 2.0f);
}

// A uint32_t or a 64-bit value could be cut to fit an int32_t, so it does not compile without a cast
static_assert(std::is_constructible_v<EventValue, uint16_t>);
static_assert(!std::is_constructible_v<EventValue, uint32_t>);
static_assert(!std::is_constructible_v<EventValue, int64_t>);
// The same for an enum: one of 8 bits compiles, one of 64 bits needs a cast
enum Small : uint8_t {};
enum Wide : int64_t {};
static_assert(std::is_constructible_v<EventValue, Small>);
static_assert(!std::is_constructible_v<EventValue, Wide>);

TEST(EventData, KeepsACopyOfTheText) {
  Dimmer dimmer;
  std::string action = "repeat";
  dimmer.event.trigger("rotate_left", {{ACTION, action}});
  action = "changed";
  EXPECT_EQ(dimmer.data().get_string(ACTION), "repeat");
}

TEST(EventData, TooLongTextIsLeftOut) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{ACTION, "released"}, {STEPS, 3}});  // 8 bytes, the variable takes 7
  EXPECT_FALSE(dimmer.data().has(ACTION));
  EXPECT_EQ(dimmer.data().get_int(STEPS), 3);
}

TEST(EventData, TextIsNotANumber) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{STEPS, "3"}, {ACTION, 3}});
  EXPECT_FALSE(dimmer.data().has(STEPS));
  EXPECT_FALSE(dimmer.data().has(ACTION));
}

TEST(EventData, UnknownVariableIsLeftOut) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{7, 1}, {STEPS, 3}});  // the dimmer has 5 variables
  EXPECT_EQ(dimmer.data().get_int(STEPS), 3);
}

TEST(EventData, ListenersReadTheValuesOfTheEventTheyGet) {
  Dimmer dimmer;
  int32_t seen = 0;
  dimmer.event.add_on_event_callback([&dimmer, &seen](StringRef) { seen = dimmer.data().get_int(STEPS); });
  dimmer.event.trigger("rotate_left", {{STEPS, -2}});
  EXPECT_EQ(seen, -2);
}

TEST(EventData, NewEventTypesClearTheValues) {
  Dimmer dimmer;
  dimmer.event.trigger("rotate_left", {{STEPS, 3}});
  dimmer.event.set_event_types({"press", "rotate_left"});
  EXPECT_FALSE(dimmer.data().has(STEPS));
}

TEST(EventData, RuntimeListOverTheLimitIsRejected) {
  Dimmer dimmer;
  FixedVector<const char *> types;
  types.init(EVENT_DATA_MAX_TYPES + 1);
  for (size_t i = 0; i < EVENT_DATA_MAX_TYPES + 1; i++)
    types.push_back("type");
  dimmer.event.set_event_types(types);
  EXPECT_EQ(dimmer.event.get_event_types().size(), 2u);  // the old list stays
}

TEST(EventData, EventWithoutVariablesHasNoData) {
  Event button;
  button.set_event_types_static(TYPES, 2);
  button.trigger("press", {{0, 1}});  // logged and ignored
  EXPECT_EQ(button.get_last_event_type(), "press");
  EXPECT_EQ(button.get_last_event_data().size(), 0);
  EXPECT_FALSE(button.get_last_event_data().has(0));
}

}  // namespace esphome::event::testing
