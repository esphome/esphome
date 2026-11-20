from esphome.components import cstxxx
from esphome.types import ConfigType

from .. import cst3240_ns

AUTO_LOAD = ["cstxxx"]

CST3240Touchscreen = cst3240_ns.class_("CST3240Touchscreen", cstxxx.CSTTouchscreen)

CONFIG_SCHEMA = cstxxx.cst_touchscreen_schema(CST3240Touchscreen, 0x5A)


async def to_code(config: ConfigType) -> None:
    await cstxxx.new_cst_touchscreen(config)
