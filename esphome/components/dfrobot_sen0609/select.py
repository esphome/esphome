import esphome.codegen as cg
from esphome.components import select
from esphome.components.const import CONF_OUT_PIN_LEVEL
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ICON_SCALE
from esphome.types import ConfigType

from .. import CONF_DFROBOT_SEN0609_ID, DFRobotSEN0609Component

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_OUT_PIN_LEVEL): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SCALE,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    selects = select.sub_selects(config, parent=hub)
    # The level the OUT pin has while a target is present; the index is the module's value
    await selects(
        CONF_OUT_PIN_LEVEL,
        hub.set_out_pin_level_select,
        options=["low", "high"],
        apply="control_out_pin_level(index)",
    )
