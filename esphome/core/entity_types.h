// X-macro include file for entity type declarations.
// This file is included multiple times with different macro definitions.
//
// Both macros must be defined before including this file:
//
//   ENTITY_TYPE_(type, singular, plural, count, upper)
//     — entities without controller callbacks (button, infrared)
//
//   ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback)
//     — entities with controller callbacks
//
// Excluded from this list (handled manually):
//   - devices/areas: not entities
//   - serial_proxy: custom register logic, no by-key lookup

#ifndef ENTITY_TYPE_
#error "ENTITY_TYPE_(type, singular, plural, count, upper) must be defined before including entity_types.h"
#endif
#ifndef ENTITY_CONTROLLER_TYPE_
#error \
    "ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) must be defined before including entity_types.h"
#endif

#ifdef USE_BINARY_SENSOR
ENTITY_CONTROLLER_TYPE_(binary_sensor::BinarySensor, binary_sensor, binary_sensors, ESPHOME_ENTITY_BINARY_SENSOR_COUNT,
                        BINARY_SENSOR, binary_sensor_update)
#endif
#ifdef USE_COVER
ENTITY_CONTROLLER_TYPE_(cover::Cover, cover, covers, ESPHOME_ENTITY_COVER_COUNT, COVER, cover_update)
#endif
#ifdef USE_FAN
ENTITY_CONTROLLER_TYPE_(fan::Fan, fan, fans, ESPHOME_ENTITY_FAN_COUNT, FAN, fan_update)
#endif
#ifdef USE_LIGHT
ENTITY_CONTROLLER_TYPE_(light::LightState, light, lights, ESPHOME_ENTITY_LIGHT_COUNT, LIGHT, light_update)
#endif
#ifdef USE_SENSOR
ENTITY_CONTROLLER_TYPE_(sensor::Sensor, sensor, sensors, ESPHOME_ENTITY_SENSOR_COUNT, SENSOR, sensor_update)
#endif
#ifdef USE_SWITCH
ENTITY_CONTROLLER_TYPE_(switch_::Switch, switch, switches, ESPHOME_ENTITY_SWITCH_COUNT, SWITCH, switch_update)
#endif
#ifdef USE_BUTTON
ENTITY_TYPE_(button::Button, button, buttons, ESPHOME_ENTITY_BUTTON_COUNT, BUTTON)
#endif
#ifdef USE_TEXT_SENSOR
ENTITY_CONTROLLER_TYPE_(text_sensor::TextSensor, text_sensor, text_sensors, ESPHOME_ENTITY_TEXT_SENSOR_COUNT,
                        TEXT_SENSOR, text_sensor_update)
#endif
#ifdef USE_CLIMATE
ENTITY_CONTROLLER_TYPE_(climate::Climate, climate, climates, ESPHOME_ENTITY_CLIMATE_COUNT, CLIMATE, climate_update)
#endif
#ifdef USE_NUMBER
ENTITY_CONTROLLER_TYPE_(number::Number, number, numbers, ESPHOME_ENTITY_NUMBER_COUNT, NUMBER, number_update)
#endif
#ifdef USE_DATETIME_DATE
ENTITY_CONTROLLER_TYPE_(datetime::DateEntity, date, dates, ESPHOME_ENTITY_DATE_COUNT, DATETIME_DATE, date_update)
#endif
#ifdef USE_DATETIME_TIME
ENTITY_CONTROLLER_TYPE_(datetime::TimeEntity, time, times, ESPHOME_ENTITY_TIME_COUNT, DATETIME_TIME, time_update)
#endif
#ifdef USE_DATETIME_DATETIME
ENTITY_CONTROLLER_TYPE_(datetime::DateTimeEntity, datetime, datetimes, ESPHOME_ENTITY_DATETIME_COUNT, DATETIME_DATETIME,
                        datetime_update)
#endif
#ifdef USE_TEXT
ENTITY_CONTROLLER_TYPE_(text::Text, text, texts, ESPHOME_ENTITY_TEXT_COUNT, TEXT, text_update)
#endif
#ifdef USE_SELECT
ENTITY_CONTROLLER_TYPE_(select::Select, select, selects, ESPHOME_ENTITY_SELECT_COUNT, SELECT, select_update)
#endif
#ifdef USE_LOCK
ENTITY_CONTROLLER_TYPE_(lock::Lock, lock, locks, ESPHOME_ENTITY_LOCK_COUNT, LOCK, lock_update)
#endif
#ifdef USE_VALVE
ENTITY_CONTROLLER_TYPE_(valve::Valve, valve, valves, ESPHOME_ENTITY_VALVE_COUNT, VALVE, valve_update)
#endif
#ifdef USE_MEDIA_PLAYER
ENTITY_CONTROLLER_TYPE_(media_player::MediaPlayer, media_player, media_players, ESPHOME_ENTITY_MEDIA_PLAYER_COUNT,
                        MEDIA_PLAYER, media_player_update)
#endif
#ifdef USE_ALARM_CONTROL_PANEL
ENTITY_CONTROLLER_TYPE_(alarm_control_panel::AlarmControlPanel, alarm_control_panel, alarm_control_panels,
                        ESPHOME_ENTITY_ALARM_CONTROL_PANEL_COUNT, ALARM_CONTROL_PANEL, alarm_control_panel_update)
#endif
#ifdef USE_WATER_HEATER
ENTITY_CONTROLLER_TYPE_(water_heater::WaterHeater, water_heater, water_heaters, ESPHOME_ENTITY_WATER_HEATER_COUNT,
                        WATER_HEATER, water_heater_update)
#endif
#ifdef USE_INFRARED
ENTITY_TYPE_(infrared::Infrared, infrared, infrareds, ESPHOME_ENTITY_INFRARED_COUNT, INFRARED)
#endif
#ifdef USE_RADIO_FREQUENCY
ENTITY_TYPE_(radio_frequency::RadioFrequency, radio_frequency, radio_frequencies, ESPHOME_ENTITY_RADIO_FREQUENCY_COUNT,
             RADIO_FREQUENCY)
#endif
#ifdef USE_EVENT
ENTITY_CONTROLLER_TYPE_(event::Event, event, events, ESPHOME_ENTITY_EVENT_COUNT, EVENT, event)
#endif
#ifdef USE_UPDATE
ENTITY_CONTROLLER_TYPE_(update::UpdateEntity, update, updates, ESPHOME_ENTITY_UPDATE_COUNT, UPDATE, update)
#endif
