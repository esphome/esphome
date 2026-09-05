import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    ICON_RESTART,
    PLATFORM_NRF52,
)
from esphome.types import ConfigType

from . import (  # noqa: F401  pylint: disable=unused-import
    CONF_DEBUG_ID,
    FILTER_SOURCE_FILES,
    DebugComponent,
)

DEPENDENCIES = ["debug"]


CONF_RESET_REASON = "reset_reason"
CONF_STACK_USAGE = "stack_usage"
CONF_UICR = "uicr"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEBUG_ID): cv.use_id(DebugComponent),
        cv.Optional(CONF_DEVICE): text_sensor.text_sensor_schema(
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_RESET_REASON): text_sensor.text_sensor_schema(
            icon=ICON_RESTART,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_STACK_USAGE): cv.All(
            cv.only_on(PLATFORM_NRF52),
            text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        ),
        cv.Optional(CONF_UICR): cv.All(
            cv.only_on(PLATFORM_NRF52),
            text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    debug_component = await cg.get_variable(config[CONF_DEBUG_ID])

    if CONF_DEVICE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE])
        cg.add(debug_component.set_device_info_sensor(sens))
    if CONF_RESET_REASON in config:
        sens = await text_sensor.new_text_sensor(config[CONF_RESET_REASON])
        cg.add(debug_component.set_reset_reason_sensor(sens))
    if CONF_STACK_USAGE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STACK_USAGE])
        cg.add(debug_component.set_stack_usage_sensor(sens))
        # stack high water
        zephyr_add_prj_conf("INIT_STACKS", True)
    if CONF_UICR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_UICR])
        cg.add(debug_component.set_uicr_sensor(sens))
