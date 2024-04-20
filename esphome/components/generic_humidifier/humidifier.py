import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import humidifier, sensor
from esphome.const import (
    CONF_HUMIDIFIER_LEVEL_1_ACTION,
    CONF_HUMIDIFIER_LEVEL_2_ACTION,
    CONF_HUMIDIFIER_LEVEL_3_ACTION,
    CONF_HUMIDIFIER_PRESET_ACTION,
    CONF_DEFAULT_TARGET_HUMIDITY,
    CONF_ID,
    CONF_SENSOR,
)

generic_humidifier_ns = cg.esphome_ns.namespace("generic_humidifier")
GenericHumidifier = generic_humidifier_ns.class_(
    "GenericHumidifier", humidifier.Humidifier, cg.Component
)
GenericHumidifierTargetHumidityConfig = generic_humidifier_ns.struct(
    "GenericHumidifierTargetHumidityConfig"
)

CONFIG_SCHEMA = cv.All(
    humidifier.HUMIDIFIER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GenericHumidifier),
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_DEFAULT_TARGET_HUMIDITY): cv.temperature,
            cv.Required(CONF_HUMIDIFIER_LEVEL_1_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HUMIDIFIER_LEVEL_2_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HUMIDIFIER_LEVEL_3_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HUMIDIFIER_PRESET_ACTION): automation.validate_automation(
                single=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(
        CONF_HUMIDIFIER_LEVEL_1_ACTION,
        CONF_HUMIDIFIER_LEVEL_2_ACTION,
        CONF_HUMIDIFIER_LEVEL_3_ACTION,
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await humidifier.register_humidifier(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    normal_config = GenericHumidifierTargetHumidityConfig(
        config[CONF_DEFAULT_TARGET_HUMIDITY],
    )
    cg.add(var.set_normal_config(normal_config))

    if level_1_action_config := config.get(CONF_HUMIDIFIER_LEVEL_1_ACTION):
        await automation.build_automation(
            var.get_level_1_trigger(), [], level_1_action_config
        )
        cg.add(var.set_supports_level_1(True))

    if level_2_action_config := config.get(CONF_HUMIDIFIER_LEVEL_2_ACTION):
        await automation.build_automation(
            var.get_level_2_trigger(), [], level_2_action_config
        )
        cg.add(var.set_supports_level_2(True))

    if level_3_action_config := config.get(CONF_HUMIDIFIER_LEVEL_3_ACTION):
        await automation.build_automation(
            var.get_level_3_trigger(), [], level_3_action_config
        )
        cg.add(var.set_supports_level_3(True))

    if preset_action_config := config.get(CONF_HUMIDIFIER_PRESET_ACTION):
        await automation.build_automation(
            var.get_preset_trigger(), [], preset_action_config
        )
        cg.add(var.set_supports_preset(True))
