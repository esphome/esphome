from esphome import final_validate as fv
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import CONF_PARENT_ID, DOMAIN, MainControlUnitHub

CONF_TEMP_IN = "temp_in"
CONF_TEMP_OUT = "temp_out"
CONF_WATER_LEVEL = "water_level"


def _sensor_schema(
    unit_of_measurement: str = cv.UNDEFINED,
    accuracy_decimals: int = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
    state_class: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=unit_of_measurement,
        accuracy_decimals=accuracy_decimals,
        device_class=device_class,
        state_class=state_class,
        icon=icon,
    ).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(MainControlUnitHub),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TEMP_IN: _sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
        CONF_TEMP_OUT: _sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
        CONF_WATER_LEVEL: _sensor_schema(
            icon="mdi:water-percent",
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


# The hub keeps one pointer per type, so a second entry would take over the first one's updates
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="sensor",
    platform=DOMAIN,
    keys=(CONF_PARENT_ID, CONF_TYPE),
    message="Only one fendt_caravan sensor of type '{type}' is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    setter = {
        CONF_TEMP_IN: hub.set_temp_in_sensor,
        CONF_TEMP_OUT: hub.set_temp_out_sensor,
        CONF_WATER_LEVEL: hub.set_water_level_sensor,
    }[config[CONF_TYPE]]
    var = await sensor.new_sensor(config)
    cg.add(setter(var))
