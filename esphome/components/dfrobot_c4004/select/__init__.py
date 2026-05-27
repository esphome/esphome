import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_C4004_ID, C4004Component, dfrobot_c4004_ns

CONF_INSTALL_MODE = "install_mode"
INSTALL_MODE_OPTIONS = ["Side", "Top"]

C4004InstallModeSelect = dfrobot_c4004_ns.class_(
    "C4004InstallModeSelect", select.Select, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_INSTALL_MODE): select.select_schema(
            C4004InstallModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:radar",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4004_ID])

    if install_mode_config := config.get(CONF_INSTALL_MODE):
        sel = await select.new_select(
            install_mode_config,
            options=INSTALL_MODE_OPTIONS,
        )
        await cg.register_parented(sel, config[CONF_C4004_ID])
        cg.add(parent.set_install_mode_select(sel))
