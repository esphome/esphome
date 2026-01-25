import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ENTITY_CATEGORY_DIAGNOSTIC

from .. import LD6002BComponent, ld6002b_ns
from ..const import (
    CONF_APPLY_AREA,
    CONF_AUTO_INTERFERENCE,
    CONF_CLEAR_INTERFERENCE,
    CONF_GET_AREAS,
    CONF_GET_DELAY,
    CONF_GET_INSTALLATION,
    CONF_GET_LOW_POWER_MODE,
    CONF_GET_LOW_POWER_SLEEP_TIME,
    CONF_GET_SENSITIVITY,
    CONF_GET_TRIGGER_SPEED,
    CONF_GET_Z_RANGE,
    CONF_LD6002B_ID,
    CONF_RESET_DETECTION_AREA,
    CONF_RESET_UNATTENDED,
    CONF_WAKE,
)

DEPENDENCIES = ["ld6002b"]

LD6002BButton = ld6002b_ns.class_("LD6002BButton", button.Button)
ButtonType = ld6002b_ns.enum("ButtonType", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_APPLY_AREA): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_AUTO_INTERFERENCE): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_GET_AREAS): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_CLEAR_INTERFERENCE): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_RESET_DETECTION_AREA): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_GET_DELAY): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_SENSITIVITY): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_TRIGGER_SPEED): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_Z_RANGE): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_INSTALLATION): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_LOW_POWER_MODE): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_LOW_POWER_SLEEP_TIME): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_RESET_UNATTENDED): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_WAKE): button.button_schema(
            LD6002BButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)

BUTTON_MAP = {
    CONF_APPLY_AREA: ButtonType.APPLY_AREA,
    CONF_AUTO_INTERFERENCE: ButtonType.AUTO_INTERFERENCE,
    CONF_GET_AREAS: ButtonType.GET_AREAS,
    CONF_CLEAR_INTERFERENCE: ButtonType.CLEAR_INTERFERENCE,
    CONF_RESET_DETECTION_AREA: ButtonType.RESET_DETECTION_AREA,
    CONF_GET_DELAY: ButtonType.GET_DELAY,
    CONF_GET_SENSITIVITY: ButtonType.GET_SENSITIVITY,
    CONF_GET_TRIGGER_SPEED: ButtonType.GET_TRIGGER_SPEED,
    CONF_GET_Z_RANGE: ButtonType.GET_Z_RANGE,
    CONF_GET_INSTALLATION: ButtonType.GET_INSTALLATION,
    CONF_GET_LOW_POWER_MODE: ButtonType.GET_LOW_POWER_MODE,
    CONF_GET_LOW_POWER_SLEEP_TIME: ButtonType.GET_LOW_POWER_SLEEP_TIME,
    CONF_RESET_UNATTENDED: ButtonType.RESET_UNATTENDED,
    CONF_WAKE: ButtonType.WAKE,
}


async def to_code(config):
    for key, button_type in BUTTON_MAP.items():
        if button_config := config.get(key):
            b = cg.new_Pvariable(button_config[CONF_ID], button_type)
            await button.register_button(b, button_config)
            await cg.register_parented(b, config[CONF_LD6002B_ID])
