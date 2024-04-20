import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import humidifier, sensor
from esphome.const import (
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

CONF_NORMAL_ACTION = "normal_action"
CONF_ECO_ACTION = "eco_action"
CONF_AWAY_ACTION = "away_action"
CONF_BOOST_ACTION = "boost_action"
CONF_COMFORT_ACTION = "comfort_action"
CONF_HOME_ACTION = "home_action"
CONF_SLEEP_ACTION = "sleep_action"
CONF_AUTO_ACTION = "auto_action"
CONF_BABY_ACTION = "baby_action"

CONFIG_SCHEMA = cv.All(
    humidifier.HUMIDIFIER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GenericHumidifier),
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_DEFAULT_TARGET_HUMIDITY): cv.temperature,
            cv.Required(CONF_NORMAL_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Required(CONF_ECO_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_AWAY_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_BOOST_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_COMFORT_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HOME_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_SLEEP_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_AUTO_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_BABY_ACTION): automation.validate_automation(single=True),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(
        CONF_NORMAL_ACTION,
        CONF_ECO_ACTION,
        CONF_AWAY_ACTION,
        CONF_BOOST_ACTION,
        CONF_COMFORT_ACTION,
        CONF_HOME_ACTION,
        CONF_SLEEP_ACTION,
        CONF_AUTO_ACTION,
        CONF_BABY_ACTION,
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

    if normal_action_config := config.get(CONF_NORMAL_ACTION):
        await automation.build_automation(
            var.get_normal_trigger(), [], normal_action_config
        )
        cg.add(var.set_supports_normal(True))
    if eco_action_config := config.get(CONF_ECO_ACTION):
        await automation.build_automation(var.get_eco_trigger(), [], eco_action_config)
        cg.add(var.set_supports_eco(True))
    if away_action_config := config.get(CONF_AWAY_ACTION):
        await automation.build_automation(
            var.get_away_trigger(), [], away_action_config
        )
        cg.add(var.set_supports_away(True))
    if boost_action_config := config.get(CONF_BOOST_ACTION):
        await automation.build_automation(
            var.get_boost_trigger(), [], boost_action_config
        )
        cg.add(var.set_supports_boost(True))
    if comfort_action_config := config.get(CONF_COMFORT_ACTION):
        await automation.build_automation(
            var.get_comfort_trigger(), [], comfort_action_config
        )
        cg.add(var.set_supports_comfort(True))
    if home_action_config := config.get(CONF_HOME_ACTION):
        await automation.build_automation(
            var.get_home_trigger(), [], home_action_config
        )
        cg.add(var.set_supports_home(True))
    if sleep_action_config := config.get(CONF_SLEEP_ACTION):
        await automation.build_automation(
            var.get_sleep_trigger(), [], sleep_action_config
        )
        cg.add(var.set_supports_sleep(True))
    if auto_action_config := config.get(CONF_AUTO_ACTION):
        await automation.build_automation(
            var.get_auto_trigger(), [], auto_action_config
        )
        cg.add(var.set_supports_auto(True))
    if baby_action_config := config.get(CONF_BABY_ACTION):
        await automation.build_automation(
            var.get_baby_trigger(), [], baby_action_config
        )
        cg.add(var.set_supports_baby(True))
