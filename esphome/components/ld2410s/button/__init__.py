import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_FACTORY_RESET,
    DEVICE_CLASS_UPDATE,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_LD2410S_ID, LD2410S, ld2410s_ns

LD2410SStartCalibrationButton = ld2410s_ns.class_(
    "LD2410SStartCalibrationButton", button.Button
)
LD2410SFactoryResetButton = ld2410s_ns.class_(
    "LD2410SFactoryResetButton", button.Button
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Optional(CONF_CALIBRATION): button.button_schema(
        LD2410SStartCalibrationButton,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:refresh-auto",
    ),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        LD2410SFactoryResetButton,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:factory",
    ),
}


async def to_code(config):
    ld2410s_component = await cg.get_variable(config[CONF_LD2410S_ID])

    if calibration := config.get(CONF_CALIBRATION):
        b = await button.new_button(calibration)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_calibration_button(b))

    if factory_reset := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_factory_reset_button(b))
