import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_UPDATE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    CONF_FACTORY_RESET,
)

from .. import CONF_MR60FDA2_ID, MR60FDA2Component, mr60fda2_ns

DEPENDENCIES = ["seeed_mr60fda2"]

GetRadarParametersButton = mr60fda2_ns.class_("GetRadarParametersButton", button.Button)
ResetRadarButton = mr60fda2_ns.class_("ResetRadarButton", button.Button)

CONF_GET_RADAR_PARAMETERS = "get_radar_parameters"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60FDA2_ID): cv.use_id(MR60FDA2Component),
    cv.Optional(CONF_GET_RADAR_PARAMETERS): button.button_schema(
        GetRadarParametersButton,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_NONE,
    ),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        ResetRadarButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    mr60fda2_component = await cg.get_variable(config[CONF_MR60FDA2_ID])
    if get_radar_parameters_config := config.get(CONF_GET_RADAR_PARAMETERS):
        b = await button.new_button(get_radar_parameters_config)
        await cg.register_parented(b, config[CONF_MR60FDA2_ID])
        cg.add(mr60fda2_component.set_get_radar_parameters_button(b))
    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset_config)
        await cg.register_parented(b, config[CONF_MR60FDA2_ID])
        cg.add(mr60fda2_component.set_factory_reset_button(b))
