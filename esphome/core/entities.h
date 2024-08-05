#pragma once

// This file is not used by the runtime, instead, a version is generated during
// compilation with only the relevant feature flags for the current build.
//
// This file is only used by static analyzers and IDEs.

namespace esphome {
namespace update {
class UpdateEntity;
}
namespace alarm_control_panel {
class AlarmControlPanel;
}
namespace number {
class Number;
}
namespace light {
class LightState;
}
namespace switch_ {
class Switch;
}
namespace climate {
class Climate;
}
namespace fan {
class Fan;
}
namespace datetime {
class DateTimeEntity;
}
namespace binary_sensor {
class BinarySensor;
}
namespace datetime {
class TimeEntity;
}
namespace text {
class Text;
}
namespace lock {
class Lock;
}
namespace event {
class Event;
}
namespace text_sensor {
class TextSensor;
}
namespace datetime {
class DateEntity;
}
namespace cover {
class Cover;
}
namespace sensor {
class Sensor;
}
namespace select {
class Select;
}
namespace valve {
class Valve;
}
namespace button {
class Button;
}
using entities_t =
    std::tuple<update::UpdateEntity *, alarm_control_panel::AlarmControlPanel *, number::Number *, light::LightState *,
               switch_::Switch *, climate::Climate *, fan::Fan *, datetime::DateTimeEntity *,
               binary_sensor::BinarySensor *, datetime::TimeEntity *, text::Text *, lock::Lock *, event::Event *,
               text_sensor::TextSensor *, datetime::DateEntity *, cover::Cover *, sensor::Sensor *, select::Select *,
               valve::Valve *, button::Button *>;
}  // namespace esphome
