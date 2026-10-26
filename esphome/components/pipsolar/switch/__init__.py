import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ICON_POWER
from esphome.types import ConfigType

from .. import CONF_PIPSOLAR_ID, PIPSOLAR_COMPONENT_SCHEMA

DEPENDENCIES = ["uart"]

CONF_OUTPUT_SOURCE_PRIORITY_UTILITY = "output_source_priority_utility"
CONF_OUTPUT_SOURCE_PRIORITY_SOLAR = "output_source_priority_solar"
CONF_OUTPUT_SOURCE_PRIORITY_BATTERY = "output_source_priority_battery"
CONF_OUTPUT_SOURCE_PRIORITY_HYBRID = "output_source_priority_hybrid"
CONF_INPUT_VOLTAGE_RANGE = "input_voltage_range"
CONF_PV_OK_CONDITION_FOR_PARALLEL = "pv_ok_condition_for_parallel"
CONF_PV_POWER_BALANCE = "pv_power_balance"

# Each switch's hub setter and its on and off commands; None means the inverter has no off command
TYPES = {
    CONF_OUTPUT_SOURCE_PRIORITY_UTILITY: (
        lambda hub: hub.set_output_source_priority_utility_switch,
        "POP00",
        None,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY_SOLAR: (
        lambda hub: hub.set_output_source_priority_solar_switch,
        "POP01",
        None,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY_BATTERY: (
        lambda hub: hub.set_output_source_priority_battery_switch,
        "POP02",
        None,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY_HYBRID: (
        lambda hub: hub.set_output_source_priority_hybrid_switch,
        "POP03",
        None,
    ),
    CONF_INPUT_VOLTAGE_RANGE: (
        lambda hub: hub.set_input_voltage_range_switch,
        "PGR01",
        "PGR00",
    ),
    CONF_PV_OK_CONDITION_FOR_PARALLEL: (
        lambda hub: hub.set_pv_ok_condition_for_parallel_switch,
        "PPVOKC1",
        "PPVOKC0",
    ),
    CONF_PV_POWER_BALANCE: (
        lambda hub: hub.set_pv_power_balance_switch,
        "PSPB1",
        "PSPB0",
    ),
}

PIPSWITCH_SCHEMA = switch.switch_schema(
    switch.ApplySwitch, icon=ICON_POWER, block_inverted=True
)

CONFIG_SCHEMA = PIPSOLAR_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): PIPSWITCH_SCHEMA for type in TYPES}
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PIPSOLAR_ID])
    switches = switch.sub_switches(config, parent=hub)
    # The hub polls the inverter and publishes each switch's state
    for key, (setter, on, off) in TYPES.items():
        off_arg = "nullptr" if off is None else f'"{off}"'
        await switches(
            key, setter(hub), apply=f'write_switch(state, "{on}", {off_arg})'
        )
