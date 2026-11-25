#  YAML config keys
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TEMPERATURE, CONF_TYPE, ICON_THERMOMETER
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import CONF_ICM20689_ID, ICM20689Component

AUTO_LOAD = ["icm20689"]

CONFIG_SCHEMA = sensor.sensor_schema(
    kind=sensor.TEMPERATURE, icon=ICON_THERMOMETER, accuracy_decimals=2
).extend(
    {
        cv.Optional(CONF_TYPE): cv.one_of(CONF_TEMPERATURE),
        cv.GenerateID(CONF_ICM20689_ID): cv.use_id(ICM20689Component),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_ICM20689_ID])
    data = MockObj("data")
    value_lambda = await cg.process_lambda(
        var.publish_state(data),
        [(cg.float_, str(data))],
    )
    cg.add(parent.add_temperature_listener(value_lambda))
