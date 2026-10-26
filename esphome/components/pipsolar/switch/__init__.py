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

TYPES = (
    CONF_OUTPUT_SOURCE_PRIORITY_UTILITY,
    CONF_OUTPUT_SOURCE_PRIORITY_SOLAR,
    CONF_OUTPUT_SOURCE_PRIORITY_BATTERY,
    CONF_OUTPUT_SOURCE_PRIORITY_HYBRID,
    CONF_INPUT_VOLTAGE_RANGE,
    CONF_PV_OK_CONDITION_FOR_PARALLEL,
    CONF_PV_POWER_BALANCE,
)

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
    await switches(
        CONF_OUTPUT_SOURCE_PRIORITY_UTILITY,
        hub.set_output_source_priority_utility_switch,
        apply='queue_command(state ? "POP00" : nullptr)',
    )
    await switches(
        CONF_OUTPUT_SOURCE_PRIORITY_SOLAR,
        hub.set_output_source_priority_solar_switch,
        apply='queue_command(state ? "POP01" : nullptr)',
    )
    await switches(
        CONF_OUTPUT_SOURCE_PRIORITY_BATTERY,
        hub.set_output_source_priority_battery_switch,
        apply='queue_command(state ? "POP02" : nullptr)',
    )
    await switches(
        CONF_OUTPUT_SOURCE_PRIORITY_HYBRID,
        hub.set_output_source_priority_hybrid_switch,
        apply='queue_command(state ? "POP03" : nullptr)',
    )
    await switches(
        CONF_INPUT_VOLTAGE_RANGE,
        hub.set_input_voltage_range_switch,
        apply='queue_command(state ? "PGR01" : "PGR00")',
    )
    await switches(
        CONF_PV_OK_CONDITION_FOR_PARALLEL,
        hub.set_pv_ok_condition_for_parallel_switch,
        apply='queue_command(state ? "PPVOKC1" : "PPVOKC0")',
    )
    await switches(
        CONF_PV_POWER_BALANCE,
        hub.set_pv_power_balance_switch,
        apply='queue_command(state ? "PSPB1" : "PSPB0")',
    )
