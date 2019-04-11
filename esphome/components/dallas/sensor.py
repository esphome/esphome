import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import CONF_ADDRESS, CONF_DALLAS_ID, CONF_INDEX, CONF_NAME, \
    CONF_RESOLUTION, CONF_UNIT_OF_MEASUREMENT, UNIT_CELSIUS, CONF_ICON, ICON_THERMOMETER, \
    CONF_ACCURACY_DECIMALS, CONF_ID
from . import DallasComponent, dallas_ns

DallasTemperatureSensor = dallas_ns.class_('DallasTemperatureSensor', sensor.Sensor)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DallasTemperatureSensor),
    cv.GenerateID(CONF_DALLAS_ID): cv.use_variable_id(DallasComponent),

    cv.Optional(CONF_ADDRESS): cv.hex_int,
    cv.Optional(CONF_INDEX): cv.positive_int,
    cv.Optional(CONF_RESOLUTION, default=12): cv.All(cv.int_, cv.Range(min=9, max=12)),

    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): sensor.unit_of_measurement,
    cv.Optional(CONF_ICON, default=ICON_THERMOMETER): sensor.icon,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=1): sensor.accuracy_decimals,
}), cv.has_exactly_one_key(CONF_ADDRESS, CONF_INDEX))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_DALLAS_ID])
    if CONF_ADDRESS in config:
        address = cg.HexIntLiteral(config[CONF_ADDRESS])
        rhs = hub.Pget_sensor_by_address(config[CONF_NAME], address, config.get(CONF_RESOLUTION))
    else:
        rhs = hub.Pget_sensor_by_index(config[CONF_NAME], config[CONF_INDEX],
                                       config.get(CONF_RESOLUTION))
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield sensor.register_sensor(var, config)

