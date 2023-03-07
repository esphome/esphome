#pragma once

namespace esphome {

#ifdef UNIT_TESTS
namespace test {
class Switch;
class Sensor;
class CustomEntity;
}  // namespace test

using entities_t = std::tuple<
#ifdef USE_SENSOR
    test::Sensor *,
#endif
#ifdef USE_SWITCH
    test::Switch *,
#endif
    test::CustomEntity *>;
#else

namespace binary_sensor {
class BinarySensor;
}
namespace sensor {
class Sensor;
}
namespace switch_ {
class Switch;
}
namespace button {
class Button;
}
namespace text_sensor {
class TextSensor;
}
namespace fan {
class Fan;
}
namespace climate {
class Climate;
}
namespace light {
class LightState;
}
namespace cover {
class Cover;
}
namespace number {
class Number;
}
namespace select {
class Select;
}
namespace lock {
class Lock;
}
namespace media_player {
class MediaPlayer;
}

using entities_t =
    std::tuple<binary_sensor::BinarySensor *, sensor::Sensor *, switch_::Switch *, button::Button *,
               text_sensor::TextSensor *, fan::Fan *, climate::Climate *, light::LightState *, cover::Cover *,
               number::Number *, select::Select *, lock::Lock *, media_player::MediaPlayer *>;
#endif
}  // namespace esphome
