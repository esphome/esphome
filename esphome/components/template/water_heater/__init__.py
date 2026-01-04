from esphome import automation
import esphome.codegen as cg
from esphome.components import water_heater
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_OPTIMISTIC,
    CONF_RESTORE_MODE,
    CONF_SET_ACTION,
    CONF_SUPPORTED_MODES,
    CONF_TARGET_TEMPERATURE,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

from .. import template_ns

CONF_CURRENT_TEMPERATURE = "current_temperature"

TemplateWaterHeater = template_ns.class_(
    "TemplateWaterHeater", water_heater.WaterHeater
)

TemplateWaterHeaterPublishAction = template_ns.class_(
    "TemplateWaterHeaterPublishAction",
    automation.Action,
    cg.Parented.template(TemplateWaterHeater),
)

TemplateWaterHeaterRestoreMode = template_ns.enum("TemplateWaterHeaterRestoreMode")
RESTORE_MODES = {
    "NO_RESTORE": TemplateWaterHeaterRestoreMode.WATER_HEATER_NO_RESTORE,
    "RESTORE": TemplateWaterHeaterRestoreMode.WATER_HEATER_RESTORE,
    "RESTORE_AND_CALL": TemplateWaterHeaterRestoreMode.WATER_HEATER_RESTORE_AND_CALL,
}

CONFIG_SCHEMA = water_heater.water_heater_schema(TemplateWaterHeater).extend(
    {
        cv.Optional(CONF_OPTIMISTIC, default=True): cv.boolean,
        cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_RESTORE_MODE, default="NO_RESTORE"): cv.enum(
            RESTORE_MODES, upper=True
        ),
        cv.Optional(CONF_CURRENT_TEMPERATURE): cv.returning_lambda,
        cv.Optional(CONF_MODE): cv.returning_lambda,
        cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
            water_heater.validate_water_heater_mode
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await water_heater.register_water_heater(var, config)

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [], config[CONF_SET_ACTION]
        )

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_CURRENT_TEMPERATURE in config:
        template_ = await cg.process_lambda(
            config[CONF_CURRENT_TEMPERATURE],
            [],
            return_type=cg.optional.template(cg.float_),
        )
        cg.add(var.set_current_temperature_lambda(template_))

    if CONF_MODE in config:
        template_ = await cg.process_lambda(
            config[CONF_MODE],
            [],
            return_type=cg.optional.template(water_heater.WaterHeaterMode),
        )
        cg.add(var.set_mode_lambda(template_))

    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))


@automation.register_action(
    "water_heater.template.publish",
    TemplateWaterHeaterPublishAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TemplateWaterHeater),
            cv.Optional(CONF_CURRENT_TEMPERATURE): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_TEMPERATURE): cv.templatable(cv.temperature),
            cv.Optional(CONF_MODE): cv.templatable(
                water_heater.validate_water_heater_mode
            ),
        }
    ),
)
async def water_heater_template_publish_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    if current_temp := config.get(CONF_CURRENT_TEMPERATURE):
        template_ = await cg.templatable(current_temp, args, float)
        cg.add(var.set_current_temperature(template_))

    if target_temp := config.get(CONF_TARGET_TEMPERATURE):
        template_ = await cg.templatable(target_temp, args, float)
        cg.add(var.set_target_temperature(template_))

    if mode := config.get(CONF_MODE):
        template_ = await cg.templatable(mode, args, water_heater.WaterHeaterMode)
        cg.add(var.set_mode(template_))

    return var
