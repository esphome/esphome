import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import (
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    DucoDevice,
    duco_ns,
    register_duco_device,
)

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx", "@w1ll14m"]

CONF_COMFORT_TEMPERATURE = "comfort_temperature"

DucoComfortTemperature = duco_ns.class_(
    "DucoComfortTemperature", DucoDevice, number.Number
)

DucoSetpoint = duco_ns.class_("DucoSetpoint", DucoDevice, number.Number)

CONF_CO2_SETPOINT = "co2_setpoint"
CONF_RH_SETPOINT = "rh_setpoint"

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


# key: (item, unit, min, max, step) of a node setting read and written with FUNCTION_NODE_SETTING
SETPOINTS = {
    CONF_CO2_SETPOINT: (0x03, UNIT_PARTS_PER_MILLION, 0, 2000, 10),
    CONF_RH_SETPOINT: (0x04, UNIT_PERCENT, 0, 90, 5),
}


def _setpoint_schema(unit: str) -> cv.Schema:
    return (
        number.number_schema(
            DucoSetpoint,
            unit_of_measurement=unit,
            entity_category=ENTITY_CATEGORY_CONFIG,
        )
        .extend({cv.Required(CONF_ADDRESS): cv.int_range(min=2, max=68)})
        .extend(cv.polling_component_schema("60s"))
    )


CONFIG_SCHEMA = cv.Any(
    COMFORT_TEMPERATURE_SCHEMA.extend(DUCO_COMPONENT_SCHEMA),
    cv.All(
        DUCO_COMPONENT_SCHEMA.extend(
            {
                cv.Optional(CONF_COMFORT_TEMPERATURE): cv.ensure_list(
                    COMFORT_TEMPERATURE_SCHEMA
                ),
                **{
                    cv.Optional(key): cv.ensure_list(_setpoint_schema(unit))
                    for key, (_, unit, _, _, _) in SETPOINTS.items()
                },
            }
        ),
        cv.has_at_least_one_key(CONF_COMFORT_TEMPERATURE, *SETPOINTS),
    ),
)


async def to_code(config: ConfigType) -> None:
    # The single-entity form is one comfort temperature with its address at the top level
    comfort_entities = config.get(
        CONF_COMFORT_TEMPERATURE, [config] if CONF_ADDRESS in config else []
    )
    for entity_config in comfort_entities:
        var = await number.new_number(
            entity_config, min_value=10.0, max_value=25.5, step=0.1
        )
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
    for key, (item, _, minimum, maximum, step) in SETPOINTS.items():
        for entity in config.get(key, []):
            var = await number.new_number(
                entity,
                entity[CONF_ADDRESS],
                item,
                min_value=minimum,
                max_value=maximum,
                step=step,
            )
            await register_duco_device(var, entity, config[CONF_DUCO_ID])
