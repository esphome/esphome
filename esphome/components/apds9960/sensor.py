import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_TYPE, CONF_UNIT_OF_MEASUREMENT, CONF_ACCURACY_DECIMALS, CONF_ICON, \
    UNIT_PERCENT, ICON_LIGHTBULB
from . import APDS9960, CONF_APDS9960_ID

DEPENDENCIES = ['apds9960']

TYPES = {
    'CLEAR': 'set_clear_channel',
    'RED': 'set_red_channel',
    'GREEN': 'set_green_channel',
    'BLUE': 'set_blue_channel',
    'PROXIMITY': 'set_proximity',
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.Required(CONF_TYPE): cv.one_of(*TYPES, upper=True),
    cv.GenerateID(CONF_APDS9960_ID): cv.use_variable_id(APDS9960),

    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_PERCENT): sensor.unit_of_measurement,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=1): sensor.accuracy_decimals,
    cv.Optional(CONF_ICON, default=ICON_LIGHTBULB): sensor.icon,
}))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_APDS9960_ID])
    var = yield sensor.new_sensor(config)
    func = getattr(hub, TYPES[config[CONF_TYPE]])
    cg.add(func(var))
