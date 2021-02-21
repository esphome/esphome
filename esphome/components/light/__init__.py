import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.automation as auto
from esphome.components import mqtt, power_supply
from esphome.const import CONF_COLOR_CORRECT, \
    CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_GAMMA_CORRECT, CONF_ID, \
    CONF_INTERNAL, CONF_NAME, CONF_MQTT_ID, CONF_POWER_SUPPLY, CONF_RESTORE_MODE, \
    CONF_ON_TURN_OFF, CONF_ON_TURN_ON, CONF_TRIGGER_ID
from esphome.core import coroutine, coroutine_with_priority
from .automation import light_control_to_code  # noqa
from .effects import validate_effects, BINARY_EFFECTS, \
    MONOCHROMATIC_EFFECTS, RGB_EFFECTS, ADDRESSABLE_EFFECTS, EFFECTS_REGISTRY
from .types import (  # noqa
    LightState, AddressableLightState, light_ns, LightOutput, AddressableLight, \
    LightTurnOnTrigger, LightTurnOffTrigger)

CODEOWNERS = ['@esphome/core']
IS_PLATFORM_COMPONENT = True

LightRestoreMode = light_ns.enum('LightRestoreMode')
RESTORE_MODES = {
    'RESTORE_DEFAULT_OFF': LightRestoreMode.LIGHT_RESTORE_DEFAULT_OFF,
    'RESTORE_DEFAULT_ON': LightRestoreMode.LIGHT_RESTORE_DEFAULT_ON,
    'ALWAYS_OFF': LightRestoreMode.LIGHT_ALWAYS_OFF,
    'ALWAYS_ON': LightRestoreMode.LIGHT_ALWAYS_ON,
}

LIGHT_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(LightState),
    cv.OnlyWith(CONF_MQTT_ID, 'mqtt'): cv.declare_id(mqtt.MQTTJSONLightComponent),
    cv.Optional(CONF_RESTORE_MODE, default='restore_default_off'):
        cv.enum(RESTORE_MODES, upper=True, space='_'),
    cv.Optional(CONF_ON_TURN_ON): auto.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightTurnOnTrigger),
    }),
    cv.Optional(CONF_ON_TURN_OFF): auto.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightTurnOffTrigger),
    }),
})

BINARY_LIGHT_SCHEMA = LIGHT_SCHEMA.extend({
    cv.Optional(CONF_EFFECTS): validate_effects(BINARY_EFFECTS),
})

BRIGHTNESS_ONLY_LIGHT_SCHEMA = LIGHT_SCHEMA.extend({
    cv.Optional(CONF_GAMMA_CORRECT, default=2.8): cv.positive_float,
    cv.Optional(CONF_DEFAULT_TRANSITION_LENGTH, default='1s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_EFFECTS): validate_effects(MONOCHROMATIC_EFFECTS),
})

RGB_LIGHT_SCHEMA = BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend({
    cv.Optional(CONF_EFFECTS): validate_effects(RGB_EFFECTS),
})

ADDRESSABLE_LIGHT_SCHEMA = RGB_LIGHT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(AddressableLightState),
    cv.Optional(CONF_EFFECTS): validate_effects(ADDRESSABLE_EFFECTS),
    cv.Optional(CONF_COLOR_CORRECT): cv.All([cv.percentage], cv.Length(min=3, max=4)),
    cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
})


@coroutine
def setup_light_core_(light_var, output_var, config):
    cg.add(light_var.set_restore_mode(config[CONF_RESTORE_MODE]))
    if CONF_INTERNAL in config:
        cg.add(light_var.set_internal(config[CONF_INTERNAL]))
    if CONF_DEFAULT_TRANSITION_LENGTH in config:
        cg.add(light_var.set_default_transition_length(config[CONF_DEFAULT_TRANSITION_LENGTH]))
    if CONF_GAMMA_CORRECT in config:
        cg.add(light_var.set_gamma_correct(config[CONF_GAMMA_CORRECT]))
    effects = yield cg.build_registry_list(EFFECTS_REGISTRY, config.get(CONF_EFFECTS, []))
    cg.add(light_var.add_effects(effects))

    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        yield auto.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        yield auto.build_automation(trigger, [], conf)

    if CONF_COLOR_CORRECT in config:
        cg.add(output_var.set_correction(*config[CONF_COLOR_CORRECT]))

    if CONF_POWER_SUPPLY in config:
        var_ = yield cg.get_variable(config[CONF_POWER_SUPPLY])
        cg.add(output_var.set_power_supply(var_))

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], light_var)
        yield mqtt.register_mqtt_component(mqtt_, config)


@coroutine
def register_light(output_var, config):
    light_var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], output_var)
    cg.add(cg.App.register_light(light_var))
    yield cg.register_component(light_var, config)
    yield setup_light_core_(light_var, output_var, config)


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_define('USE_LIGHT')
    cg.add_global(light_ns.using)
