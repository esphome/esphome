import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, variable, setup_component

MakeUptimeSensor = Application.struct('MakeUptimeSensor')
UptimeSensor = sensor.sensor_ns.class_('UptimeSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(UptimeSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeUptimeSensor),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_uptime_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    uptime = make.Puptime

    sensor.setup_sensor(uptime, make.Pmqtt, config)
    setup_component(uptime, config)


BUILD_FLAGS = '-DUSE_UPTIME_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
