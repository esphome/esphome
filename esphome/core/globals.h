#pragma once

namespace esphome {

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
}  // namespace esphome
