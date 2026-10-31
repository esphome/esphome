from collections.abc import Callable
from typing import NamedTuple

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ICON_POWER
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import CONF_PIPSOLAR_ID, PIPSOLAR_COMPONENT_SCHEMA

DEPENDENCIES = ["uart"]

CONF_OUTPUT_SOURCE_PRIORITY_UTILITY = "output_source_priority_utility"
CONF_OUTPUT_SOURCE_PRIORITY_SOLAR = "output_source_priority_solar"
CONF_OUTPUT_SOURCE_PRIORITY_BATTERY = "output_source_priority_battery"
CONF_OUTPUT_SOURCE_PRIORITY_HYBRID = "output_source_priority_hybrid"
CONF_INPUT_VOLTAGE_RANGE = "input_voltage_range"
CONF_PV_OK_CONDITION_FOR_PARALLEL = "pv_ok_condition_for_parallel"
CONF_PV_POWER_BALANCE = "pv_power_balance"


class _PipSwitch(NamedTuple):
    setter: Callable[[MockObj], MockObj]
    on: str
    off: str | None  # None when the inverter has no off command


# Each switch's hub setter and its on and off commands
TYPES = {
    CONF_OUTPUT_SOURCE_PRIORITY_UTILITY: _PipSwitch(
        lambda hub: hub.set_output_source_priority_utility_switch,
        "POP00",
        None,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY_SOLAR: _PipSwitch(
        lambda hub: hub.set_output_source_priority_solar_switch,
        "POP01",
        None,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY_BATTERY: _PipSwitch(
        lambda hub: hub.set_output_source_priority_battery_switch,
        "POP02",
        None,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY_HYBRID: _PipSwitch(
        lambda hub: hub.set_output_source_priority_hybrid_switch,
        "POP03",
        None,
    ),
    CONF_INPUT_VOLTAGE_RANGE: _PipSwitch(
        lambda hub: hub.set_input_voltage_range_switch,
        "PGR01",
        "PGR00",
    ),
    CONF_PV_OK_CONDITION_FOR_PARALLEL: _PipSwitch(
        lambda hub: hub.set_pv_ok_condition_for_parallel_switch,
        "PPVOKC1",
        "PPVOKC0",
    ),
    CONF_PV_POWER_BALANCE: _PipSwitch(
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
    for key, sw in TYPES.items():
        off_arg = "nullptr" if sw.off is None else f'"{sw.off}"'
        await switches(
            key, sw.setter(hub), apply=f'write_switch(state, "{sw.on}", {off_arg})'
        )
