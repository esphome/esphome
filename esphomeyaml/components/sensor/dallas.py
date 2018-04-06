import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.dallas import DALLAS_COMPONENT_CLASS
from esphomeyaml.const import CONF_ADDRESS, CONF_DALLAS_ID, CONF_INDEX, CONF_RESOLUTION, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import HexIntLiteral, get_variable

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    vol.Exclusive(CONF_ADDRESS, 'dallas'): cv.hex_int,
    vol.Exclusive(CONF_INDEX, 'dallas'): cv.positive_int,
    vol.Optional(CONF_DALLAS_ID): cv.variable_id,
    vol.Optional(CONF_RESOLUTION): vol.All(vol.Coerce(int), vol.Range(min=8, max=12)),
}).extend(sensor.MQTT_SENSOR_ID_SCHEMA.schema)


def to_code(config):
    hub = get_variable(config.get(CONF_DALLAS_ID), DALLAS_COMPONENT_CLASS)
    update_interval = config.get(CONF_UPDATE_INTERVAL)
    if CONF_RESOLUTION in config and update_interval is None:
        update_interval = 10000

    if CONF_ADDRESS in config:
        address = HexIntLiteral(config[CONF_ADDRESS])
        sensor_ = hub.Pget_sensor_by_address(address, update_interval,
                                             config.get(CONF_RESOLUTION))
    else:
        sensor_ = hub.Pget_sensor_by_index(config[CONF_INDEX], update_interval,
                                           config.get(CONF_RESOLUTION))
    sensor.make_mqtt_sensor_for(sensor_, config)
