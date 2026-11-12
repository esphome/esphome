import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import (
    CONF_BUCK1,
    CONF_BUCK2,
    CONF_CHARGER,
    CONF_LDO1,
    CONF_LDO2,
    CONF_NPM1300_ID,
    NPM1300_CHILD_SCHEMA,
)

DEPENDENCIES = ["npm1300"]


def _switch_schema() -> cv.Schema:
    # The switches show the state of the chip, so nothing is restored at boot
    return switch.switch_schema(
        switch.ApplySwitch,
        default_restore_mode="DISABLED",
        entity_category=ENTITY_CATEGORY_CONFIG,
    )


CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_CHARGER): _switch_schema(),
        cv.Optional(CONF_BUCK1): _switch_schema(),
        cv.Optional(CONF_BUCK2): _switch_schema(),
        cv.Optional(CONF_LDO1): _switch_schema(),
        cv.Optional(CONF_LDO2): _switch_schema(),
    }
)


def _output_apply(name: str) -> str:
    return f"set_output_enabled(npm1300::NPM1300Output::NPM1300_OUTPUT_{name}, state)"


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_NPM1300_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_CHARGER,
        hub.set_charger_switch,
        apply="set_charger_enabled(state)",
        publish="on_success",
    )
    await switches(
        CONF_BUCK1,
        hub.set_buck1_switch,
        apply=_output_apply("BUCK1"),
        publish="on_success",
    )
    await switches(
        CONF_BUCK2,
        hub.set_buck2_switch,
        apply=_output_apply("BUCK2"),
        publish="on_success",
    )
    await switches(
        CONF_LDO1,
        hub.set_ldo1_switch,
        apply=_output_apply("LDO1"),
        publish="on_success",
    )
    await switches(
        CONF_LDO2,
        hub.set_ldo2_switch,
        apply=_output_apply("LDO2"),
        publish="on_success",
    )
