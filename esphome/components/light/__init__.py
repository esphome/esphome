import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.automation import ACTION_REGISTRY, maybe_simple_id
from esphome.components import mqtt
from esphome.const import CONF_ALPHA, CONF_BLUE, CONF_BRIGHTNESS, CONF_COLORS, CONF_COLOR_CORRECT, \
    CONF_COLOR_TEMPERATURE, CONF_DEFAULT_TRANSITION_LENGTH, CONF_DURATION, CONF_EFFECT, \
    CONF_EFFECTS, CONF_FLASH_LENGTH, CONF_GAMMA_CORRECT, CONF_GREEN, CONF_ID, \
    CONF_INTERNAL, CONF_LAMBDA, CONF_NAME, CONF_NUM_LEDS, CONF_RANDOM, CONF_RED, \
    CONF_SPEED, CONF_STATE, CONF_TRANSITION_LENGTH, CONF_UPDATE_INTERVAL, CONF_WHITE, CONF_WIDTH, \
    CONF_MQTT_ID
from esphome.core import coroutine
from esphome.util import ServiceRegistry

IS_PLATFORM_COMPONENT = True

# Base
light_ns = cg.esphome_ns.namespace('light')
LightState = light_ns.class_('LightState', cg.Nameable, cg.Component)
# Fake class for addressable lights
AddressableLightState = light_ns.class_('LightState', LightState)
LightOutput = light_ns.class_('LightOutput')
AddressableLight = light_ns.class_('AddressableLight')
AddressableLightRef = AddressableLight.operator('ref')

# Actions
ToggleAction = light_ns.class_('ToggleAction', cg.Action)
LightControlAction = light_ns.class_('LightControlAction', cg.Action)

LightColorValues = light_ns.class_('LightColorValues')

# Effects
LightEffect = light_ns.class_('LightEffect')
RandomLightEffect = light_ns.class_('RandomLightEffect', LightEffect)
LambdaLightEffect = light_ns.class_('LambdaLightEffect', LightEffect)
StrobeLightEffect = light_ns.class_('StrobeLightEffect', LightEffect)
StrobeLightEffectColor = light_ns.class_('StrobeLightEffectColor', LightEffect)
FlickerLightEffect = light_ns.class_('FlickerLightEffect', LightEffect)
AddressableLightEffect = light_ns.class_('AddressableLightEffect', LightEffect)
AddressableLambdaLightEffect = light_ns.class_('AddressableLambdaLightEffect',
                                               AddressableLightEffect)
AddressableRainbowLightEffect = light_ns.class_('AddressableRainbowLightEffect',
                                                AddressableLightEffect)
AddressableColorWipeEffect = light_ns.class_('AddressableColorWipeEffect', AddressableLightEffect)
AddressableColorWipeEffectColor = light_ns.struct('AddressableColorWipeEffectColor')
AddressableScanEffect = light_ns.class_('AddressableScanEffect', AddressableLightEffect)
AddressableTwinkleEffect = light_ns.class_('AddressableTwinkleEffect', AddressableLightEffect)
AddressableRandomTwinkleEffect = light_ns.class_('AddressableRandomTwinkleEffect',
                                                 AddressableLightEffect)
AddressableFireworksEffect = light_ns.class_('AddressableFireworksEffect', AddressableLightEffect)
AddressableFlickerEffect = light_ns.class_('AddressableFlickerEffect', AddressableLightEffect)

CONF_STROBE = 'strobe'
CONF_FLICKER = 'flicker'
CONF_ADDRESSABLE_LAMBDA = 'addressable_lambda'
CONF_ADDRESSABLE_RAINBOW = 'addressable_rainbow'
CONF_ADDRESSABLE_COLOR_WIPE = 'addressable_color_wipe'
CONF_ADDRESSABLE_SCAN = 'addressable_scan'
CONF_ADDRESSABLE_TWINKLE = 'addressable_twinkle'
CONF_ADDRESSABLE_RANDOM_TWINKLE = 'addressable_random_twinkle'
CONF_ADDRESSABLE_FIREWORKS = 'addressable_fireworks'
CONF_ADDRESSABLE_FLICKER = 'addressable_flicker'

CONF_ADD_LED_INTERVAL = 'add_led_interval'
CONF_REVERSE = 'reverse'
CONF_MOVE_INTERVAL = 'move_interval'
CONF_TWINKLE_PROBABILITY = 'twinkle_probability'
CONF_PROGRESS_INTERVAL = 'progress_interval'
CONF_SPARK_PROBABILITY = 'spark_probability'
CONF_USE_RANDOM_COLOR = 'use_random_color'
CONF_FADE_OUT_RATE = 'fade_out_rate'
CONF_INTENSITY = 'intensity'

BINARY_EFFECTS = [CONF_LAMBDA, CONF_STROBE]
MONOCHROMATIC_EFFECTS = BINARY_EFFECTS + [CONF_FLICKER]
RGB_EFFECTS = MONOCHROMATIC_EFFECTS + [CONF_RANDOM]
ADDRESSABLE_EFFECTS = RGB_EFFECTS + [CONF_ADDRESSABLE_LAMBDA, CONF_ADDRESSABLE_RAINBOW,
                                     CONF_ADDRESSABLE_COLOR_WIPE, CONF_ADDRESSABLE_SCAN,
                                     CONF_ADDRESSABLE_TWINKLE, CONF_ADDRESSABLE_RANDOM_TWINKLE,
                                     CONF_ADDRESSABLE_FIREWORKS, CONF_ADDRESSABLE_FLICKER]

EFFECTS_REGISTRY = ServiceRegistry()


def register_effect(name, effect_type, default_name, schema, *extra_validators):
    schema = cv.Schema(schema).extend({
        cv.GenerateID(): cv.declare_variable_id(effect_type),
        cv.Optional(CONF_NAME, default=default_name): cv.string_strict,
    })
    validator = cv.All(schema, *extra_validators)
    register = EFFECTS_REGISTRY.register(name, validator)

    return register


@register_effect('lambda', LambdaLightEffect, "Lambda", {
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Optional(CONF_UPDATE_INTERVAL, default='0ms'): cv.update_interval,
})
def lambda_effect_to_code(config):
    lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [], return_type=cg.void)
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], lambda_,
                           config[CONF_UPDATE_INTERVAL])


@register_effect('random', RandomLightEffect, "Random", {
    cv.Optional(CONF_TRANSITION_LENGTH, default='7.5s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_UPDATE_INTERVAL, default='10s'): cv.positive_time_period_milliseconds,
})
def random_effect_to_code(config):
    effect = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(effect.set_transition_length(config[CONF_TRANSITION_LENGTH]))
    cg.add(effect.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    yield effect


@register_effect('strobe', StrobeLightEffect, "Strobe", {
    cv.Optional(CONF_COLORS, default=[
        {CONF_STATE: True, CONF_DURATION: '0.5s'},
        {CONF_STATE: False, CONF_DURATION: '0.5s'},
    ]): cv.All(cv.ensure_list(cv.Schema({
        cv.Optional(CONF_STATE, default=True): cv.boolean,
        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
        cv.Optional(CONF_RED, default=1.0): cv.percentage,
        cv.Optional(CONF_GREEN, default=1.0): cv.percentage,
        cv.Optional(CONF_BLUE, default=1.0): cv.percentage,
        cv.Optional(CONF_WHITE, default=1.0): cv.percentage,
        cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
    }), cv.has_at_least_one_key(CONF_STATE, CONF_BRIGHTNESS, CONF_RED, CONF_GREEN, CONF_BLUE,
                                CONF_WHITE)), cv.Length(min=2)),
})
def strobe_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    colors = []
    for color in config.get(CONF_COLORS, []):
        colors.append(cg.StructInitializer(
            StrobeLightEffectColor,
            ('color', LightColorValues(color[CONF_STATE], color[CONF_BRIGHTNESS],
                                       color[CONF_RED], color[CONF_GREEN], color[CONF_BLUE],
                                       color[CONF_WHITE])),
            ('duration', color[CONF_DURATION]),
        ))
    cg.add(var.set_colors(colors))
    yield var


@register_effect('flicker', FlickerLightEffect, "Flicker", {
    cv.Optional(CONF_ALPHA, default=0.95): cv.percentage,
    cv.Optional(CONF_INTENSITY, default=0.015): cv.percentage,
})
def flicker_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_alpha(config[CONF_ALPHA]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    yield var


@register_effect('addressable_lambda', AddressableLambdaLightEffect, "Addressable Lambda", {
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Optional(CONF_UPDATE_INTERVAL, default='0ms'): cv.positive_time_period_milliseconds,
})
def addressable_lambda_effect_to_code(config):
    args = [(AddressableLightRef, 'it')]
    lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], args, return_type=cg.void)
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], lambda_,
                           config[CONF_UPDATE_INTERVAL])
    yield var


@register_effect('addressable_rainbow', AddressableRainbowLightEffect, "Rainbow", {
    cv.Optional(CONF_SPEED, default=10): cv.uint32_t,
    cv.Optional(CONF_WIDTH, default=50): cv.uint32_t,
})
def addressable_rainbow_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_speed(config[CONF_SPEED]))
    cg.add(var.set_width(config[CONF_WIDTH]))
    yield var


@register_effect('addressable_color_wipe', AddressableColorWipeEffect, "Color Wipe", {
    cv.Optional(CONF_COLORS, default=[{CONF_NUM_LEDS: 1, CONF_RANDOM: True}]): cv.ensure_list({
        cv.Optional(CONF_RED, default=1.0): cv.percentage,
        cv.Optional(CONF_GREEN, default=1.0): cv.percentage,
        cv.Optional(CONF_BLUE, default=1.0): cv.percentage,
        cv.Optional(CONF_WHITE, default=1.0): cv.percentage,
        cv.Optional(CONF_RANDOM, default=False): cv.boolean,
        cv.Required(CONF_NUM_LEDS): cv.All(cv.uint32_t, cv.Range(min=1)),
    }),
    cv.Optional(CONF_ADD_LED_INTERVAL, default='0.1s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_REVERSE, default=False): cv.boolean,
})
def addressable_color_wipe_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_add_led_interval(config[CONF_ADD_LED_INTERVAL]))
    cg.add(var.set_reverse(config[CONF_REVERSE]))
    colors = []
    for color in config.get(CONF_COLORS, []):
        colors.append(cg.StructInitializer(
            AddressableColorWipeEffectColor,
            ('r', int(round(color[CONF_RED] * 255))),
            ('g', int(round(color[CONF_GREEN] * 255))),
            ('b', int(round(color[CONF_BLUE] * 255))),
            ('w', int(round(color[CONF_WHITE] * 255))),
            ('random', color[CONF_RANDOM]),
            ('num_leds', color[CONF_NUM_LEDS]),
        ))
    cg.add(var.set_colors(colors))
    yield var


@register_effect('addressable_scan', AddressableScanEffect, "Scan", {
    cv.Optional(CONF_MOVE_INTERVAL, default='0.1s'): cv.positive_time_period_milliseconds,
})
def addressable_scan_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_move_interval(config[CONF_MOVE_INTERVAL]))
    yield var


@register_effect('addressable_twinkle', AddressableTwinkleEffect, "Twinkle", {
    cv.Optional(CONF_TWINKLE_PROBABILITY, default='5%'): cv.percentage,
    cv.Optional(CONF_PROGRESS_INTERVAL, default='4ms'): cv.positive_time_period_milliseconds,
})
def addressable_twinkle_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_twinkle_probability(config[CONF_TWINKLE_PROBABILITY]))
    cg.add(var.set_progress_interval(config[CONF_PROGRESS_INTERVAL]))
    yield var


@register_effect('addressable_random_twinkle', AddressableRandomTwinkleEffect, "Random Twinkle", {
    cv.Optional(CONF_TWINKLE_PROBABILITY, default='5%'): cv.percentage,
    cv.Optional(CONF_PROGRESS_INTERVAL, default='32ms'): cv.positive_time_period_milliseconds,
})
def addressable_random_twinkle_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_twinkle_probability(config[CONF_TWINKLE_PROBABILITY]))
    cg.add(var.set_progress_interval(config[CONF_PROGRESS_INTERVAL]))
    yield var


@register_effect('addressable_fireworks', AddressableFireworksEffect, "Fireworks", {
    cv.Optional(CONF_UPDATE_INTERVAL, default='32ms'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_SPARK_PROBABILITY, default='10%'): cv.percentage,
    cv.Optional(CONF_USE_RANDOM_COLOR, default=False): cv.boolean,
    cv.Optional(CONF_FADE_OUT_RATE, default=120): cv.uint8_t,
})
def addressable_fireworks_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_spark_probability(config[CONF_SPARK_PROBABILITY]))
    cg.add(var.set_use_random_color(config[CONF_USE_RANDOM_COLOR]))
    cg.add(var.set_fade_out_rate(config[CONF_FADE_OUT_RATE]))
    yield var


@register_effect('addressable_flicker', AddressableFlickerEffect, "Addressable Flicker", {
    cv.Optional(CONF_UPDATE_INTERVAL, default='16ms'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_INTENSITY, default='5%'): cv.percentage,
})
def addressable_flicker_effect_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    yield var


def validate_effects(allowed_effects):
    def validator(value):
        value = cv.validate_registry('effect', EFFECTS_REGISTRY, [])(value)
        errors = []
        names = set()
        for i, x in enumerate(value):
            key = next(it for it in x.keys())
            if key not in allowed_effects:
                errors.append(
                    cv.Invalid("The effect '{}' is not allowed for this "
                               "light type".format(key), [i])
                )
                continue
            name = x[key][CONF_NAME]
            if name in names:
                errors.append(
                    cv.Invalid(u"Found the effect name '{}' twice. All effects must have "
                               u"unique names".format(name), [i])
                )
                continue
            names.add(name)
        if errors:
            raise cv.MultipleInvalid(errors)
        return value

    return validator


LIGHT_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(LightState),
    cv.OnlyWith(CONF_MQTT_ID, 'mqtt'): cv.declare_variable_id(mqtt.MQTTJSONLightComponent),
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
    cv.GenerateID(): cv.declare_variable_id(AddressableLightState),
    cv.Optional(CONF_EFFECTS): validate_effects(ADDRESSABLE_EFFECTS),
    cv.Optional(CONF_COLOR_CORRECT): cv.All([cv.percentage], cv.Length(min=3, max=4)),
})


@coroutine
def setup_light_core_(light_var, output_var, config):
    if CONF_INTERNAL in config:
        cg.add(light_var.set_internal(config[CONF_INTERNAL]))
    if CONF_DEFAULT_TRANSITION_LENGTH in config:
        cg.add(light_var.set_default_transition_length(config[CONF_DEFAULT_TRANSITION_LENGTH]))
    if CONF_GAMMA_CORRECT in config:
        cg.add(light_var.set_gamma_correct(config[CONF_GAMMA_CORRECT]))
    effects = yield cg.build_registry_list(EFFECTS_REGISTRY, config.get(CONF_EFFECTS, []))
    cg.add(light_var.add_effects(effects))

    if CONF_COLOR_CORRECT in config:
        cg.add(output_var.set_correction(*config[CONF_COLOR_CORRECT]))

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], light_var)
        yield mqtt.register_mqtt_component(mqtt_, config)


@coroutine
def register_light(output_var, config):
    light_var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], output_var)
    cg.add(cg.App.register_light(light_var))
    yield cg.register_component(light_var, config)
    yield setup_light_core_(light_var, output_var, config)


@ACTION_REGISTRY.register('light.toggle', maybe_simple_id({
    cv.Required(CONF_ID): cv.use_variable_id(LightState),
    cv.Optional(CONF_TRANSITION_LENGTH): cv.templatable(cv.positive_time_period_milliseconds),
}))
def light_toggle_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = ToggleAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    if CONF_TRANSITION_LENGTH in config:
        template_ = yield cg.templatable(config[CONF_TRANSITION_LENGTH], args, cg.uint32)
        cg.add(action.set_transition_length(template_))
    yield action


@ACTION_REGISTRY.register('light.turn_off', maybe_simple_id({
    cv.Required(CONF_ID): cv.use_variable_id(LightState),
    cv.Optional(CONF_TRANSITION_LENGTH): cv.templatable(cv.positive_time_period_milliseconds),
}))
def light_turn_off_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = LightControlAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    if CONF_TRANSITION_LENGTH in config:
        template_ = yield cg.templatable(config[CONF_TRANSITION_LENGTH], args, cg.uint32)
        cg.add(action.set_transition_length(template_))
    yield action


LIGHT_CONTROL_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(LightState),
    cv.Optional(CONF_STATE): cv.templatable(cv.boolean),
    cv.Exclusive(CONF_TRANSITION_LENGTH, 'transformer'):
        cv.templatable(cv.positive_time_period_milliseconds),
    cv.Exclusive(CONF_FLASH_LENGTH, 'transformer'):
        cv.templatable(cv.positive_time_period_milliseconds),
    cv.Exclusive(CONF_EFFECT, 'transformer'): cv.templatable(cv.string),
    cv.Optional(CONF_BRIGHTNESS): cv.templatable(cv.percentage),
    cv.Optional(CONF_RED): cv.templatable(cv.percentage),
    cv.Optional(CONF_GREEN): cv.templatable(cv.percentage),
    cv.Optional(CONF_BLUE): cv.templatable(cv.percentage),
    cv.Optional(CONF_WHITE): cv.templatable(cv.percentage),
    cv.Optional(CONF_COLOR_TEMPERATURE): cv.templatable(cv.color_temperature),
})
LIGHT_TURN_OFF_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_variable_id(LightState),
    cv.Optional(CONF_TRANSITION_LENGTH): cv.templatable(cv.positive_time_period_milliseconds),
    cv.Optional(CONF_STATE, default=False): False,
})
LIGHT_TURN_ON_ACTION_SCHEMA = maybe_simple_id(LIGHT_CONTROL_ACTION_SCHEMA.extend({
    cv.Optional(CONF_STATE, default=True): True,
}))


@ACTION_REGISTRY.register('light.turn_off', LIGHT_TURN_OFF_ACTION_SCHEMA)
@ACTION_REGISTRY.register('light.turn_on', LIGHT_TURN_ON_ACTION_SCHEMA)
@ACTION_REGISTRY.register('light.control', LIGHT_CONTROL_ACTION_SCHEMA)
def light_control_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = LightControlAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    if CONF_STATE in config:
        template_ = yield cg.templatable(config[CONF_STATE], args, bool)
        cg.add(action.set_state(template_))
    if CONF_TRANSITION_LENGTH in config:
        template_ = yield cg.templatable(config[CONF_TRANSITION_LENGTH], args, cg.uint32)
        cg.add(action.set_transition_length(template_))
    if CONF_FLASH_LENGTH in config:
        template_ = yield cg.templatable(config[CONF_FLASH_LENGTH], args, cg.uint32)
        cg.add(action.set_flash_length(template_))
    if CONF_BRIGHTNESS in config:
        template_ = yield cg.templatable(config[CONF_BRIGHTNESS], args, float)
        cg.add(action.set_brightness(template_))
    if CONF_RED in config:
        template_ = yield cg.templatable(config[CONF_RED], args, float)
        cg.add(action.set_red(template_))
    if CONF_GREEN in config:
        template_ = yield cg.templatable(config[CONF_GREEN], args, float)
        cg.add(action.set_green(template_))
    if CONF_BLUE in config:
        template_ = yield cg.templatable(config[CONF_BLUE], args, float)
        cg.add(action.set_blue(template_))
    if CONF_WHITE in config:
        template_ = yield cg.templatable(config[CONF_WHITE], args, float)
        cg.add(action.set_white(template_))
    if CONF_COLOR_TEMPERATURE in config:
        template_ = yield cg.templatable(config[CONF_COLOR_TEMPERATURE], args, float)
        cg.add(action.set_color_temperature(template_))
    if CONF_EFFECT in config:
        template_ = yield cg.templatable(config[CONF_EFFECT], args, cg.std_string)
        cg.add(action.set_effect(template_))
    yield action


def to_code(config):
    cg.add_define('USE_LIGHT')
    cg.add_global(light_ns.using)
