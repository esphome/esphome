from esphome.components import tmc22xx
from esphome.types import ConfigType

from . import TMC2209Stepper

AUTO_LOAD = ["tmc22xx"]
DEPENDENCIES = ["uart"]

CONFIG_SCHEMA = tmc22xx.tmc22xx_schema(TMC2209Stepper, max_address=3)
FINAL_VALIDATE_SCHEMA = tmc22xx.final_validate


async def to_code(config: ConfigType) -> None:
    await tmc22xx.new_tmc22xx(config)
