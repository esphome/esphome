import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS
from esphome.types import ConfigType

from .. import (
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    DucoDevice,
    duco_ns,
    register_duco_device,
)

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

CONF_COMFORT_TEMPERATURE = "comfort_temperature"

DucoComfortTemperature = duco_ns.class_(
    "DucoComfortTemperature", DucoDevice, number.Number
)

COMFORT_TEMPERATURE_SCHEMA = (
    number.number_schema(
        DucoComfortTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=0x12): cv.one_of(
                0x12, 0x13, 0x14, int=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)

CONFIG_SCHEMA = cv.Any(
    COMFORT_TEMPERATURE_SCHEMA.extend(DUCO_COMPONENT_SCHEMA),
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Required(CONF_COMFORT_TEMPERATURE): cv.ensure_list(
                COMFORT_TEMPERATURE_SCHEMA
            ),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    for entity_config in config.get(CONF_COMFORT_TEMPERATURE, [config]):
        var = await number.new_number(
            entity_config, min_value=10.0, max_value=25.5, step=0.1
        )
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
