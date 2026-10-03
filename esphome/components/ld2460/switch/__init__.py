import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from esphome.types import ConfigType

from .. import CONF_LD2460_ID, LD2460Component, ld2460_ns

ReportingSwitch = ld2460_ns.class_("ReportingSwitch", switch.Switch)

CONF_REPORTING = "reporting"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
    cv.Optional(CONF_REPORTING): switch.switch_schema(
        ReportingSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_PULSE,
    ),
}


async def to_code(config: ConfigType) -> None:
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])
    if reporting_config := config.get(CONF_REPORTING):
        s = await switch.new_switch(reporting_config)
        await cg.register_parented(s, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_reporting_switch(s))
