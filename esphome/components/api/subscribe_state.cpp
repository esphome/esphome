#include "subscribe_state.h"
#ifdef USE_API
#include "api_connection.h"
#include "esphome/core/log.h"

namespace esphome::api {

// Generate entity handler implementations using macros
#ifdef USE_BINARY_SENSOR
INITIAL_STATE_HANDLER(binary_sensor, binary_sensor::BinarySensor)
#endif
#ifdef USE_COVER
INITIAL_STATE_HANDLER(cover, cover::Cover)
#endif
#ifdef USE_FAN
INITIAL_STATE_HANDLER(fan, fan::Fan)
#endif
#ifdef USE_LIGHT
INITIAL_STATE_HANDLER(light, light::LightState)
#endif
#ifdef USE_SENSOR
INITIAL_STATE_HANDLER(sensor, sensor::Sensor)
#endif
#ifdef USE_SWITCH
INITIAL_STATE_HANDLER(switch, switch_::Switch)
#endif
#ifdef USE_TEXT_SENSOR
INITIAL_STATE_HANDLER(text_sensor, text_sensor::TextSensor)
#endif
#ifdef USE_CLIMATE
INITIAL_STATE_HANDLER(climate, climate::Climate)
#endif
#ifdef USE_NUMBER
INITIAL_STATE_HANDLER(number, number::Number)
#endif
#ifdef USE_DATETIME_DATE
INITIAL_STATE_HANDLER(date, datetime::DateEntity)
#endif
#ifdef USE_DATETIME_TIME
INITIAL_STATE_HANDLER(time, datetime::TimeEntity)
#endif
#ifdef USE_DATETIME_DATETIME
INITIAL_STATE_HANDLER(datetime, datetime::DateTimeEntity)
#endif
#ifdef USE_TEXT
INITIAL_STATE_HANDLER(text, text::Text)
#endif
#ifdef USE_SELECT
INITIAL_STATE_HANDLER(select, select::Select)
#endif
#ifdef USE_LOCK
INITIAL_STATE_HANDLER(lock, lock::Lock)
#endif
#ifdef USE_VALVE
INITIAL_STATE_HANDLER(valve, valve::Valve)
#endif
#ifdef USE_MEDIA_PLAYER
INITIAL_STATE_HANDLER(media_player, media_player::MediaPlayer)
#endif
#ifdef USE_ALARM_CONTROL_PANEL
INITIAL_STATE_HANDLER(alarm_control_panel, alarm_control_panel::AlarmControlPanel)
#endif
#ifdef USE_UPDATE
INITIAL_STATE_HANDLER(update, update::UpdateEntity)
#endif

// Special cases (button and event) are already defined inline in subscribe_state.h

InitialStateIterator::InitialStateIterator(APIConnection *client) : client_(client) {}

}  // namespace esphome::api
#endif
