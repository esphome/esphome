import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

CONF_MAX_LIFETIME = "max_lifetime"
CONF_IS_ON = "is_on"
CONF_CURRENT_SPEED = "current_speed"

filter_lifetime_ns = cg.esphome_ns.namespace("filter_lifetime")
FilterLifetime = filter_lifetime_ns.class_(
    "FilterLifetime", sensor.Sensor, cg.Component
)


CONFIG_SCHEMA = (
    sensor.sensor_schema(FilterLifetime)
    .extend(
        {
            cv.Required(CONF_MAX_LIFETIME): cv.positive_int,
            cv.Required(CONF_IS_ON): cv.returning_lambda,
            cv.Required(CONF_CURRENT_SPEED): cv.returning_lambda,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_max_lifetime(config[CONF_MAX_LIFETIME]))
    is_on_template = await cg.templatable(config[CONF_IS_ON], [], bool)
    cg.add(var.set_is_on(is_on_template))
    current_speed_template = await cg.templatable(config[CONF_CURRENT_SPEED], [], float)
    cg.add(var.set_current_speed(current_speed_template))
    if sensor.CONF_FILTERS in config:
        filters = await sensor.build_filters(config[sensor.CONF_FILTERS])
        cg.add(var.set_filters(filters))
