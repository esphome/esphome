import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_NAME,
    CONF_SENSORS,
    CONF_BINARY_SENSORS,
)

AUTO_LOAD = ["socket"]
CODEOWNERS = ["@Links2004"]
DEPENDENCIES = ["network"]

CONF_HOST = "host"
CONF_PREFIX = "prefix"

statsd_component_ns = cg.esphome_ns.namespace("statsd")
StatsdComponent = statsd_component_ns.class_("StatsdComponent", cg.PollingComponent)

CONFIG_SENSORS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
        cv.Required(CONF_NAME): cv.string_strict,
    }
)

CONFIG_BINARY_SENSORS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_NAME): cv.string_strict,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StatsdComponent),
        cv.Required(CONF_HOST): cv.string_strict,
        cv.Optional(CONF_PORT, default=8125): cv.port,
        cv.Optional(CONF_PREFIX, default=""): cv.string_strict,
        cv.Optional(CONF_SENSORS): cv.ensure_list(CONFIG_SENSORS_SCHEMA),
        cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(CONFIG_BINARY_SENSORS_SCHEMA),
    }
).extend(cv.polling_component_schema("10s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(
        var.configure(
            config.get(CONF_HOST),
            config.get(CONF_PORT),
            config.get(CONF_PREFIX),
        )
    )

    for sensor_cfg in config.get(CONF_SENSORS, []):
        s = await cg.get_variable(sensor_cfg[CONF_ID])
        cg.add(var.register_sensor(sensor_cfg[CONF_NAME], s))

    for sensor_cfg in config.get(CONF_BINARY_SENSORS, []):
        s = await cg.get_variable(sensor_cfg[CONF_ID])
        cg.add(var.register_binary_sensor(sensor_cfg[CONF_NAME], s))
