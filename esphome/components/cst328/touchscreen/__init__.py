from esphome.components import cstxxx
from esphome.types import ConfigType

from .. import cst328_ns

AUTO_LOAD = ["cstxxx"]

CST328Touchscreen = cst328_ns.class_("CST328Touchscreen", cstxxx.CSTTouchscreen)

CST328ButtonListener = cst328_ns.class_("CST328ButtonListener")
CONFIG_SCHEMA = cstxxx.cst_touchscreen_schema(CST328Touchscreen, 0x1A)


async def to_code(config: ConfigType) -> None:
    await cstxxx.new_cst_touchscreen(config)
