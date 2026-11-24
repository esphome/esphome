from esphome.components import cstxxx
from esphome.types import ConfigType

from .. import cst9220_ns

AUTO_LOAD = ["cstxxx"]

CST9220Touchscreen = cst9220_ns.class_("CST9220Touchscreen", cstxxx.CSTTouchscreen)

CONFIG_SCHEMA = cstxxx.cst_touchscreen_schema(CST9220Touchscreen, 0x5A)


async def to_code(config: ConfigType) -> None:
    await cstxxx.new_cst_touchscreen(config)
