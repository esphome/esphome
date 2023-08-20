#pragma once

// This file is not used by the runtime, instead, a version is generated during
// compilation with only the relevant feature flags for the current build.
//
// This file is only used by static analyzers and IDEs.

namespace esphome {

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
namespace alarm_control_panel {
class AlarmControlPanel;
}

using entities_t = std::tuple<binary_sensor::BinarySensor *, sensor::Sensor *, switch_::Switch *, button::Button *,
                              text_sensor::TextSensor *, fan::Fan *, climate::Climate *, light::LightState *,
                              cover::Cover *, number::Number *, select::Select *, lock::Lock *,
                              media_player::MediaPlayer *, alarm_control_panel::AlarmControlPanel *>;
}  // namespace esphome
