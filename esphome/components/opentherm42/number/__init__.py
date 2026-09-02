import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_VALUE, CONF_RESTORE_VALUE

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT,
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2,
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY,
)

CONF_OPENTHERM42_ID = "opentherm42_id"

OpenTherm42Number = opentherm42_ns.class_(
    "OpenTherm42Number", number.Number, cg.Component
)


def _degrees_c_schema() -> cv.Schema:
    return number.number_schema(OpenTherm42Number, unit_of_measurement="°C").extend(
        {
            cv.Optional(CONF_INITIAL_VALUE, default=0.0): cv.float_range(
                min=0, max=100
            ),
            cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
        }
    )


# §5.1: the master decides the actual min/max/step of the value it sends -- the spec only defines
# the wire range (0..100 for these three, see notes under the ID 1 table entry).
TYPES: dict[str, tuple[cv.Schema, dict]] = {
    # §5.3.1 Class 1, ID 1: Control Setpoint, i.e. CH water temperature setpoint (degrees C, 0..100).
    # The CHenable bit (see switch platform) has priority: the boiler must ignore this value while
    # CH is disabled.
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT: (
        _degrees_c_schema(),
        {"min_value": 0, "max_value": 100, "step": 0.1},
    ),
    # §5.3.1 Class 1, ID 8: Control Setpoint 2 (TsetCH2), i.e. setpoint for the 2nd CH circuit
    # (degrees C, 0..100).
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2: (
        _degrees_c_schema(),
        {"min_value": 0, "max_value": 100, "step": 0.1},
    ),
    # §5.3.1 Class 1, ID 71 LB: Control Setpoint ventilation/heat-recovery. Relative ventilation
    # position (0-100%): 0% is the minimum set ventilation, 100% is the maximum set ventilation.
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY: (
        number.number_schema(OpenTherm42Number, unit_of_measurement="%").extend(
            {
                cv.Optional(CONF_INITIAL_VALUE, default=0): cv.int_range(
                    min=0, max=100
                ),
                cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
            }
        ),
        {"min_value": 0, "max_value": 100, "step": 1},
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{
            cv.Optional(marker): schema.extend(cv.COMPONENT_SCHEMA)
            for marker, (schema, _traits) in TYPES.items()
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker, (_schema, traits) in TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, **traits)
            await cg.register_component(var, marker_config)
            cg.add(var.set_initial_value(marker_config[CONF_INITIAL_VALUE]))
            cg.add(var.set_restore_value(marker_config[CONF_RESTORE_VALUE]))
            cg.add(getattr(hub, f"set_{marker}_number")(var))
