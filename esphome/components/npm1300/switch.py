import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (
    CONF_BUCK1,
    CONF_BUCK2,
    CONF_CHARGER,
    CONF_LDO1,
    CONF_LDO2,
    CONF_NPM1300_ID,
    CONF_THERMISTOR_RESISTANCE,
    NPM1300_CHILD_SCHEMA,
    OUTPUTS,
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


def _final_validate(config: ConfigType) -> None:
    if CONF_CHARGER not in config:
        return
    hub = fv.get_declaring_config(config[CONF_NPM1300_ID])
    # Only the charger block turns off the NTC check, without which the charger never starts
    if CONF_CHARGER not in hub and CONF_THERMISTOR_RESISTANCE not in hub:
        raise cv.Invalid(
            f"The '{CONF_CHARGER}' switch needs a '{CONF_CHARGER}' block or "
            f"'{CONF_THERMISTOR_RESISTANCE}' on the npm1300 component",
            path=[CONF_CHARGER],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


def _output_apply(key: str) -> str:
    return f"set_output_enabled({OUTPUTS[key]}, state)"


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
        apply=_output_apply(CONF_BUCK1),
        publish="on_success",
    )
    await switches(
        CONF_BUCK2,
        hub.set_buck2_switch,
        apply=_output_apply(CONF_BUCK2),
        publish="on_success",
    )
    await switches(
        CONF_LDO1,
        hub.set_ldo1_switch,
        apply=_output_apply(CONF_LDO1),
        publish="on_success",
    )
    await switches(
        CONF_LDO2,
        hub.set_ldo2_switch,
        apply=_output_apply(CONF_LDO2),
        publish="on_success",
    )
