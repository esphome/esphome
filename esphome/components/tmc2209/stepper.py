from esphome import automation
from esphome.components import tmc22xx
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import TMC2209Stepper

AUTO_LOAD = ["tmc22xx"]
DEPENDENCIES = ["uart"]

CONF_ON_STALL = "on_stall"


def _validate_stall_needs_diag(config: ConfigType) -> ConfigType:
    if CONF_ON_STALL in config and tmc22xx.CONF_DIAG_PIN not in config:
        raise cv.Invalid(
            f"'{CONF_ON_STALL}' requires '{tmc22xx.CONF_DIAG_PIN}', stalls are only reported on DIAG"
        )
    return config


CONFIG_SCHEMA = cv.All(
    tmc22xx.tmc22xx_schema(
        TMC2209Stepper,
        max_address=3,
        extra={cv.Optional(CONF_ON_STALL): automation.validate_automation({})},
    ),
    _validate_stall_needs_diag,
)
FINAL_VALIDATE_SCHEMA = tmc22xx.final_validate


async def to_code(config: ConfigType) -> None:
    var = await tmc22xx.new_tmc22xx(config)
    for conf in config.get(CONF_ON_STALL, []):
        await automation.build_callback_automation(
            var, "add_on_stall_callback", [], conf
        )
