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

DucoComfortTemperature = duco_ns.class_(
    "DucoComfortTemperature", DucoDevice, number.Number
)

CONF_COMFORT_TEMPERATURE = "comfort_temperature"

COMFORT_TEMPERATURE_SCHEMA = (
    number.number_schema(
        DucoComfortTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=0x12): cv.int_range(min=0, max=0xFF),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)

LEGACY_SCHEMA = COMFORT_TEMPERATURE_SCHEMA.extend(DUCO_COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.Any(
    LEGACY_SCHEMA,
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Optional(CONF_COMFORT_TEMPERATURE): cv.ensure_list(COMFORT_TEMPERATURE_SCHEMA),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    if CONF_COMFORT_TEMPERATURE not in config:
        var = await number.new_number(config, min_value=10.0, max_value=25.5, step=0.1)
        cg.add(var.set_address(config[CONF_ADDRESS]))
        await register_duco_device(var, config, config[CONF_DUCO_ID])
        return

    for entity_config in config[CONF_COMFORT_TEMPERATURE]:
        var = await number.new_number(entity_config, min_value=10.0, max_value=25.5, step=0.1)
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
