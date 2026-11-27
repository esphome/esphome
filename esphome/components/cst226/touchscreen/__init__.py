from esphome.components import cstxxx
from esphome.types import ConfigType

from .. import cst226_ns

AUTO_LOAD = ["cstxxx"]

CST226Touchscreen = cst226_ns.class_("CST226Touchscreen", cstxxx.CSTTouchscreen)

CST226ButtonListener = cst226_ns.class_("CST226ButtonListener")
CONFIG_SCHEMA = cstxxx.cst_touchscreen_schema(CST226Touchscreen, 0x5A)


async def to_code(config: ConfigType) -> None:
    await cstxxx.new_cst_touchscreen(config)
