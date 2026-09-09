import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM
from esphome.types import ConfigType

from .sensor import CONF_THRESHOLD_HIGH, CONF_THRESHOLD_LOW, OPT4048Component

CODEOWNERS = ["@JS3910"]
DEPENDENCIES = ["i2c"]

CONF_OPT4048_ID = "opt4048_id"
CONF_CONVERSION_READY = "conversion_ready"
CONF_OVERLOAD = "overload"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPT4048_ID): cv.use_id(OPT4048Component),
        cv.Optional(CONF_CONVERSION_READY): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_OVERLOAD): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_THRESHOLD_LOW): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_THRESHOLD_HIGH): binary_sensor.binary_sensor_schema(),
    }
)

BINARY_SENSORS = {
    CONF_CONVERSION_READY: "set_conversion_ready_binary_sensor",
    CONF_OVERLOAD: "set_overload_binary_sensor",
    CONF_THRESHOLD_LOW: "set_threshold_low_binary_sensor",
    CONF_THRESHOLD_HIGH: "set_threshold_high_binary_sensor",
}


async def to_code(config: ConfigType) -> None:
    var = await cg.get_variable(config[CONF_OPT4048_ID])
    for conf_id, setter in BINARY_SENSORS.items():
        if sensor_config := config.get(conf_id):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(var, setter)(sens))
