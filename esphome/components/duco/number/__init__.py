import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoComfortTemperature = duco_ns.class_(
    "DucoComfortTemperature", cg.PollingComponent, number.Number
)

CONFIG_SCHEMA = (
    number.number_schema(
        DucoComfortTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )
    .extend(DUCO_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    var = await number.new_number(config, min_value=10.0, max_value=25.5, step=0.1)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DUCO_ID])
