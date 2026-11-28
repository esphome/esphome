import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
import esphome.final_validate as fv

from .. import gree_ns
from ..climate import GreeClimate

CODEOWNERS = ["@realkibou"]

CONF_GREE_ID = "gree_id"

# Keep this order in sync with GREE_VERTICAL_SWING_POSITIONS in gree_vertical_swing_select.cpp.
VERTICAL_SWING_OPTIONS = [
    "Default",  # 0x00
    "Full Swing",  # 0x01
    "Fixed Upper",  # 0x02
    "Fixed Upper Middle",  # 0x03
    "Fixed Middle",  # 0x04
    "Fixed Lower Middle",  # 0x05
    "Fixed Lower",  # 0x06
    "Swing Lower",  # 0x07
    "Swing Middle",  # 0x09
    "Swing Upper",  # 0x0B
]

GreeVerticalSwingSelect = gree_ns.class_(
    "GreeVerticalSwingSelect", select.Select, cg.Component
)

CONFIG_SCHEMA = select.select_schema(
    GreeVerticalSwingSelect,
    icon="mdi:air-conditioner",
).extend(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
    }
)

# The climate keeps one vane select, so a second entry would take over the first.
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=select.DOMAIN,
    platform="gree",
    keys=(CONF_GREE_ID,),
    message="Only one gree select entry is allowed per climate ('{gree_id}')",
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GREE_ID])
    cg.add_define("USE_GREE_VERTICAL_SWING_SELECT")
    var = await select.new_select(config, parent, options=VERTICAL_SWING_OPTIONS)
    await cg.register_component(var, config)
