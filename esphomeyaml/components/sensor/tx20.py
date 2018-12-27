import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core, pins
from esphomeyaml.components import sensor, text_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_UPDATE_INTERVAL, CONF_WIND_SPEED, \
    CONF_WIND_DIRECTION, CONF_WIND_DIRECTION_TEXT, CONF_PIN, CONF_NAME
from esphomeyaml.helpers import App, Application, variable, \
    setup_component, gpio_input_pin_expression

MakeTX20Sensor = Application.struct('MakeTX20Sensor')
TX20WindSpeedSensor = sensor.sensor_ns.class_('TX20WindSpeedSensor',
                                              sensor.EmptyPollingParentSensor)
TX20WindDirectionSensor = sensor.sensor_ns.class_('TX20WindDirectionSensor',
                                                  sensor.EmptyPollingParentSensor)
TX20WindDirectionTextSensor = text_sensor.text_sensor_ns.class_('TX20WindDirectionTextSensor',
                                                                text_sensor.MQTTTextSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTX20Sensor),
    vol.Required(CONF_WIND_SPEED): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(TX20WindSpeedSensor),
    })),
    vol.Required(CONF_WIND_DIRECTION): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(TX20WindDirectionSensor),
    })),
    vol.Required(CONF_WIND_DIRECTION_TEXT): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(TX20WindDirectionTextSensor),
    })),
    vol.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_tx20_sensor(config[CONF_WIND_SPEED][CONF_NAME],
                               config[CONF_WIND_DIRECTION][CONF_NAME],
                               config[CONF_WIND_DIRECTION_TEXT][CONF_NAME], pin,
                               config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    tx20 = make.Ptx20

    sensor.setup_sensor(tx20.Pget_wind_speed_sensor(), make.Pmqtt_wind_speed,
                        config[CONF_WIND_SPEED])
    sensor.setup_sensor(tx20.Pget_wind_direction_sensor(), make.Pmqtt_wind_direction,
                        config[CONF_WIND_DIRECTION])
    text_sensor.setup_text_sensor(tx20.Pget_wind_direction_text_sensor(), 
                                  make.Pmqtt_wind_direction_text,
                                  config[CONF_WIND_DIRECTION_TEXT])
    setup_component(tx20, config)


BUILD_FLAGS = '-DUSE_TX20 -DUSE_TEXT_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_WIND_SPEED]),
            sensor.core_to_hass_config(data, config[CONF_WIND_DIRECTION]),
            sensor.core_to_hass_config(data, config[CONF_WIND_DIRECTION_TEXT])]
