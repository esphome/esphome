#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#define LOG_BINARY_SENSOR_(name, sensor) LOG_BINARY_SENSOR("  ", name, sensor##_binary_sensor_)
#define PUBLISH_BINARY_SENSOR(sensor, new_state) \
  if ((sensor##_binary_sensor_) && (sensor##_binary_sensor_)->state != (new_state)) \
    (sensor##_binary_sensor_)->publish_state(new_state);
#else
#define LOG_BINARY_SENSOR_(name, sensor)
#define PUBLISH_BINARY_SENSOR(sensor, new_state)
#endif

#ifdef USE_SENSOR
#define LOG_SENSOR_(name, sensor) LOG_SENSOR("  ", name, sensor##_sensor_)
#define PUBLISH_SENSOR(sensor, new_state) \
  if ((sensor##_sensor_) && (sensor##_sensor_)->raw_state != (new_state)) \
    (sensor##_sensor_)->publish_state(new_state);
#else
#define LOG_SENSOR_(name, sensor)
#define PUBLISH_SENSOR(sensor, new_state)
#endif

#ifdef USE_TEXT_SENSOR
#define LOG_TEXT_SENSOR_(name, sensor) LOG_TEXT_SENSOR("  ", name, sensor##_text_sensor_)
#define PUBLISH_TEXT_SENSOR(sensor, new_state) \
  if ((sensor##_text_sensor_) && (sensor##_text_sensor_)->raw_state != (new_state)) \
    (sensor##_text_sensor_)->publish_state(new_state);
#else
#define LOG_TEXT_SENSOR_(name, sensor)
#define PUBLISH_TEXT_SENSOR(sensor, new_state)
#endif
