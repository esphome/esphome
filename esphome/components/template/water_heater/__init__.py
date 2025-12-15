from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import water_heater
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_OPTIMISTIC,
    CONF_RESTORE_MODE,
    CONF_SET_ACTION,
    CONF_TARGET_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_MAX_TEMPERATURE,
)

from .. import template_ns

CONF_CURRENT_TEMPERATURE = "current_temperature"

TemplateWaterHeater = template_ns.class_(
    "TemplateWaterHeater", water_heater.WaterHeater, cg.Component
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

CONFIG_SCHEMA = water_heater.WATER_HEATER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TemplateWaterHeater),
        cv.Optional(CONF_OPTIMISTIC, default=True): cv.boolean,
        cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_RESTORE_MODE, default="NO_RESTORE"): cv.enum(
            RESTORE_MODES, upper=True
        ),
        cv.Optional(CONF_CURRENT_TEMPERATURE): cv.templatable(cv.temperature),
        cv.Optional(CONF_MODE): cv.templatable(water_heater.validate_water_heater_mode),
        # Hier voegen we de min/max opties toe die in je vorige error ontbraken
        cv.Optional(CONF_MIN_TEMPERATURE, default=10.0): cv.temperature,
        cv.Optional(CONF_MAX_TEMPERATURE, default=60.0): cv.temperature,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await water_heater.register_water_heater(var, config)

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_min_temperature(config[CONF_MIN_TEMPERATURE]))
    cg.add(var.set_max_temperature(config[CONF_MAX_TEMPERATURE]))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [], config[CONF_SET_ACTION]
        )

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_CURRENT_TEMPERATURE in config:
        conf = config[CONF_CURRENT_TEMPERATURE]
        if not isinstance(conf, cv.Lambda):
            conf = cv.Lambda(f"return {conf};")

        template_ = await cg.process_lambda(
            conf,
            [],
            return_type=cg.optional.template(cg.float_),
        )
        cg.add(var.set_current_temperature_lambda(template_))

    if CONF_MODE in config:
        conf = config[CONF_MODE]
        if not isinstance(conf, cv.Lambda):
            enum_value_str = str(conf).split("::")[-1]
            conf = cv.Lambda(f"return water_heater::{enum_value_str};")

        template_ = await cg.process_lambda(
            conf,
            [],
            return_type=cg.optional.template(water_heater.WaterHeaterMode),
        )
        cg.add(var.set_mode_lambda(template_))


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
async def water_heater_template_publish_to_code(config, action_id, template_arg, args):
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
