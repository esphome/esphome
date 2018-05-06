import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_COUNT_MODE, CONF_FALLING_EDGE, CONF_ID, CONF_INTERNAL_FILTER, \
    CONF_NAME, CONF_PIN, CONF_PULL_MODE, CONF_RISING_EDGE, CONF_UPDATE_INTERVAL, \
    ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, RawExpression, add, variable

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

GPIO_PULL_MODES = {
    'PULLUP': 'GPIO_PULLUP_ONLY',
    'PULLDOWN': 'GPIO_PULLDOWN_ONLY',
    'PULLUP_PULLDOWN': 'GPIO_PULLUP_PULLDOWN',
    'FLOATING': 'GPIO_FLOATING',
}

GPIO_PULL_MODE_SCHEMA = vol.All(vol.Upper, vol.Any(*list(GPIO_PULL_MODES.keys())))

COUNT_MODES = {
    'DISABLE': 'PCNT_COUNT_DIS',
    'INCREMENT': 'PCNT_COUNT_INC',
    'DECREMENT': 'PCNT_COUNT_DEC',
}

COUNT_MODE_SCHEMA = vol.All(vol.Upper, vol.Any(*list(COUNT_MODES.keys())))

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('pulse_counter'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.input_pin,
    vol.Optional(CONF_PULL_MODE): GPIO_PULL_MODE_SCHEMA,
    vol.Optional(CONF_COUNT_MODE): vol.Schema({
        vol.Required(CONF_RISING_EDGE): COUNT_MODE_SCHEMA,
        vol.Required(CONF_FALLING_EDGE): COUNT_MODE_SCHEMA,
    }),
    vol.Optional(CONF_INTERNAL_FILTER): vol.All(vol.Coerce(int), vol.Range(min=0, max=1023)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
}).extend(sensor.MQTT_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_pulse_counter_sensor(config[CONF_NAME], config[CONF_PIN],
                                        config.get(CONF_UPDATE_INTERVAL))
    make = variable('Application::MakePulseCounterSensor', config[CONF_ID], rhs)
    pcnt = make.Ppcnt
    if CONF_PULL_MODE in config:
        pull_mode = GPIO_PULL_MODES[config[CONF_PULL_MODE]]
        add(pcnt.set_pull_mode(RawExpression(pull_mode)))
    if CONF_COUNT_MODE in config:
        count_mode = config[CONF_COUNT_MODE]
        rising_edge = COUNT_MODES[count_mode[CONF_RISING_EDGE]]
        falling_edge = COUNT_MODES[count_mode[CONF_FALLING_EDGE]]
        add(pcnt.set_edge_mode(RawExpression(rising_edge), RawExpression(falling_edge)))
    if CONF_INTERNAL_FILTER in config:
        add(pcnt.set_filter(config[CONF_INTERNAL_FILTER]))
    sensor.setup_sensor(pcnt, config)
    sensor.setup_mqtt_sensor_component(make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_PULSE_COUNTER_SENSOR'
