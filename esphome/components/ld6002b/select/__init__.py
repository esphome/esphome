import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_AREA_ID, CONF_ID, CONF_SENSITIVITY, ENTITY_CATEGORY_CONFIG

from .. import LD6002BComponent, ld6002b_ns
from ..const import CONF_INSTALLATION_MODE, CONF_LD6002B_ID, CONF_TRIGGER_SPEED

DEPENDENCIES = ["ld6002b"]

LD6002BSelect = ld6002b_ns.class_("LD6002BSelect", select.Select)
SelectType = ld6002b_ns.enum("SelectType", is_class=True)

AREA_ID_OPTIONS = [
    "interference_0",
    "interference_1",
    "interference_2",
    "interference_3",
    "detection_0",
    "detection_1",
    "detection_2",
    "detection_3",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_SENSITIVITY): select.select_schema(
            LD6002BSelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_TRIGGER_SPEED): select.select_schema(
            LD6002BSelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_INSTALLATION_MODE): select.select_schema(
            LD6002BSelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_AREA_ID): select.select_schema(
            LD6002BSelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    if sensitivity_config := config.get(CONF_SENSITIVITY):
        s = cg.new_Pvariable(sensitivity_config[CONF_ID], SelectType.SENSITIVITY)
        await select.register_select(
            s, sensitivity_config, options=["low", "medium", "high"]
        )
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_sensitivity_select(s))

    if trigger_config := config.get(CONF_TRIGGER_SPEED):
        s = cg.new_Pvariable(trigger_config[CONF_ID], SelectType.TRIGGER_SPEED)
        await select.register_select(
            s, trigger_config, options=["slow", "medium", "fast"]
        )
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_trigger_speed_select(s))

    if install_config := config.get(CONF_INSTALLATION_MODE):
        s = cg.new_Pvariable(install_config[CONF_ID], SelectType.INSTALLATION_MODE)
        await select.register_select(s, install_config, options=["top", "side"])
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_installation_select(s))

    if area_id_config := config.get(CONF_AREA_ID):
        s = cg.new_Pvariable(area_id_config[CONF_ID], SelectType.AREA_ID)
        await select.register_select(s, area_id_config, options=AREA_ID_OPTIONS)
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_area_id_select(s))
