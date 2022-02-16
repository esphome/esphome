from esphome.jsonschema import jschema_extractor
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation

from esphome.const import (
    CONF_NAME,
    CONF_LAMBDA,
    CONF_UPDATE_INTERVAL,
    CONF_TRANSITION_LENGTH,
    CONF_COLORS,
    CONF_STATE,
    CONF_DURATION,
    CONF_BRIGHTNESS,
    CONF_COLOR_MODE,
    CONF_COLOR_BRIGHTNESS,
    CONF_RED,
    CONF_GREEN,
    CONF_BLUE,
    CONF_WHITE,
    CONF_COLOR_TEMPERATURE,
    CONF_COLD_WHITE,
    CONF_WARM_WHITE,
    CONF_ALPHA,
    CONF_INTENSITY,
    CONF_SPEED,
    CONF_WIDTH,
    CONF_NUM_LEDS,
    CONF_RANDOM,
    CONF_SEQUENCE,
)
from esphome.util import Registry
from .types import (
    ColorMode,
    COLOR_MODES,
    LambdaLightEffect,
    PulseLightEffect,
    RandomLightEffect,
    StrobeLightEffect,
    StrobeLightEffectColor,
    LightColorValues,
    AddressableLightRef,
    AddressableLambdaLightEffect,
    FlickerLightEffect,
    AddressableRainbowLightEffect,
    AddressableColorWipeEffect,
    AddressableColorWipeEffectColor,
    AddressableScanEffect,
    AddressableTwinkleEffect,
    AddressableRandomTwinkleEffect,
    AddressableFireworksEffect,
    AddressableFlickerEffect,
    AutomationLightEffect,
    Color,
)

CONF_ADD_LED_INTERVAL = "add_led_interval"
CONF_REVERSE = "reverse"
CONF_MOVE_INTERVAL = "move_interval"
CONF_SCAN_WIDTH = "scan_width"
CONF_TWINKLE_PROBABILITY = "twinkle_probability"
CONF_PROGRESS_INTERVAL = "progress_interval"
CONF_SPARK_PROBABILITY = "spark_probability"
CONF_USE_RANDOM_COLOR = "use_random_color"
CONF_FADE_OUT_RATE = "fade_out_rate"
CONF_STROBE = "strobe"
CONF_FLICKER = "flicker"
CONF_ADDRESSABLE_LAMBDA = "addressable_lambda"
CONF_ADDRESSABLE_RAINBOW = "addressable_rainbow"
CONF_ADDRESSABLE_COLOR_WIPE = "addressable_color_wipe"
CONF_ADDRESSABLE_SCAN = "addressable_scan"
CONF_ADDRESSABLE_TWINKLE = "addressable_twinkle"
CONF_ADDRESSABLE_RANDOM_TWINKLE = "addressable_random_twinkle"
CONF_ADDRESSABLE_FIREWORKS = "addressable_fireworks"
CONF_ADDRESSABLE_FLICKER = "addressable_flicker"
CONF_AUTOMATION = "automation"

BINARY_EFFECTS = []
MONOCHROMATIC_EFFECTS = []
RGB_EFFECTS = []
ADDRESSABLE_EFFECTS = []

EFFECTS_REGISTRY = Registry()


def register_effect(name, effect_type, default_name, schema, *extra_validators):
    schema = cv.Schema(schema).extend(
        {
            cv.Optional(CONF_NAME, default=default_name): cv.string_strict,
        }
    )
    validator = cv.All(schema, *extra_validators)
    return EFFECTS_REGISTRY.register(name, effect_type, validator)


def register_binary_effect(name, effect_type, default_name, schema, *extra_validators):
    # binary effect can be used for all lights
    BINARY_EFFECTS.append(name)
    MONOCHROMATIC_EFFECTS.append(name)
    RGB_EFFECTS.append(name)
    ADDRESSABLE_EFFECTS.append(name)

    return register_effect(name, effect_type, default_name, schema, *extra_validators)


def register_monochromatic_effect(
    name, effect_type, default_name, schema, *extra_validators
):
    # monochromatic effect can be used for all lights expect binary
    MONOCHROMATIC_EFFECTS.append(name)
    RGB_EFFECTS.append(name)
    ADDRESSABLE_EFFECTS.append(name)

    return register_effect(name, effect_type, default_name, schema, *extra_validators)


def register_rgb_effect(name, effect_type, default_name, schema, *extra_validators):
    # RGB effect can be used for RGB and addressable lights
    RGB_EFFECTS.append(name)
    ADDRESSABLE_EFFECTS.append(name)

    return register_effect(name, effect_type, default_name, schema, *extra_validators)


def register_addressable_effect(
    name, effect_type, default_name, schema, *extra_validators
):
    # addressable effect can be used only in addressable
    ADDRESSABLE_EFFECTS.append(name)

    return register_effect(name, effect_type, default_name, schema, *extra_validators)


@register_binary_effect(
    "lambda",
    LambdaLightEffect,
    "Lambda",
    {
        cv.Required(CONF_LAMBDA): cv.lambda_,
        cv.Optional(CONF_UPDATE_INTERVAL, default="0ms"): cv.update_interval,
    },
)
async def lambda_effect_to_code(config, effect_id):
    lambda_ = await cg.process_lambda(
        config[CONF_LAMBDA], [(bool, "initial_run")], return_type=cg.void
    )
    return cg.new_Pvariable(
        effect_id, config[CONF_NAME], lambda_, config[CONF_UPDATE_INTERVAL]
    )


@register_binary_effect(
    "automation",
    AutomationLightEffect,
    "Automation",
    {
        cv.Required(CONF_SEQUENCE): automation.validate_automation(single=True),
    },
)
async def automation_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    await automation.build_automation(var.get_trig(), [], config[CONF_SEQUENCE])
    return var


@register_monochromatic_effect(
    "pulse",
    PulseLightEffect,
    "Pulse",
    {
        cv.Optional(
            CONF_TRANSITION_LENGTH, default="1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="1s"
        ): cv.positive_time_period_milliseconds,
    },
)
async def pulse_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_transition_length(config[CONF_TRANSITION_LENGTH]))
    cg.add(effect.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    return effect


@register_monochromatic_effect(
    "random",
    RandomLightEffect,
    "Random",
    {
        cv.Optional(
            CONF_TRANSITION_LENGTH, default="7.5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="10s"
        ): cv.positive_time_period_milliseconds,
    },
)
async def random_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_transition_length(config[CONF_TRANSITION_LENGTH]))
    cg.add(effect.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    return effect


@register_binary_effect(
    "strobe",
    StrobeLightEffect,
    "Strobe",
    {
        cv.Optional(
            CONF_COLORS,
            default=[
                {CONF_STATE: True, CONF_DURATION: "0.5s"},
                {CONF_STATE: False, CONF_DURATION: "0.5s"},
            ],
        ): cv.All(
            cv.ensure_list(
                cv.Schema(
                    {
                        cv.Optional(CONF_STATE, default=True): cv.boolean,
                        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
                        cv.Optional(CONF_COLOR_MODE): cv.enum(
                            COLOR_MODES, upper=True, space="_"
                        ),
                        cv.Optional(CONF_COLOR_BRIGHTNESS, default=1.0): cv.percentage,
                        cv.Optional(CONF_RED, default=1.0): cv.percentage,
                        cv.Optional(CONF_GREEN, default=1.0): cv.percentage,
                        cv.Optional(CONF_BLUE, default=1.0): cv.percentage,
                        cv.Optional(CONF_WHITE, default=1.0): cv.percentage,
                        cv.Optional(CONF_COLOR_TEMPERATURE): cv.color_temperature,
                        cv.Optional(CONF_COLD_WHITE, default=1.0): cv.percentage,
                        cv.Optional(CONF_WARM_WHITE, default=1.0): cv.percentage,
                        cv.Required(
                            CONF_DURATION
                        ): cv.positive_time_period_milliseconds,
                    }
                ),
                cv.has_at_least_one_key(
                    CONF_STATE,
                    CONF_BRIGHTNESS,
                    CONF_COLOR_MODE,
                    CONF_COLOR_BRIGHTNESS,
                    CONF_RED,
                    CONF_GREEN,
                    CONF_BLUE,
                    CONF_WHITE,
                    CONF_COLOR_TEMPERATURE,
                    CONF_COLD_WHITE,
                    CONF_WARM_WHITE,
                ),
            ),
            cv.Length(min=2),
        ),
    },
)
async def strobe_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    colors = []
    for color in config.get(CONF_COLORS, []):
        colors.append(
            cg.StructInitializer(
                StrobeLightEffectColor,
                (
                    "color",
                    LightColorValues(
                        color.get(CONF_COLOR_MODE, ColorMode.UNKNOWN),
                        color[CONF_STATE],
                        color[CONF_BRIGHTNESS],
                        color[CONF_COLOR_BRIGHTNESS],
                        color[CONF_RED],
                        color[CONF_GREEN],
                        color[CONF_BLUE],
                        color[CONF_WHITE],
                        color.get(CONF_COLOR_TEMPERATURE, 0.0),
                        color[CONF_COLD_WHITE],
                        color[CONF_WARM_WHITE],
                    ),
                ),
                ("duration", color[CONF_DURATION]),
            )
        )
    cg.add(var.set_colors(colors))
    return var


@register_monochromatic_effect(
    "flicker",
    FlickerLightEffect,
    "Flicker",
    {
        cv.Optional(CONF_ALPHA, default=0.95): cv.percentage,
        cv.Optional(CONF_INTENSITY, default=0.015): cv.percentage,
    },
)
async def flicker_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_alpha(config[CONF_ALPHA]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    return var


@register_addressable_effect(
    "addressable_lambda",
    AddressableLambdaLightEffect,
    "Addressable Lambda",
    {
        cv.Required(CONF_LAMBDA): cv.lambda_,
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="0ms"
        ): cv.positive_time_period_milliseconds,
    },
)
async def addressable_lambda_effect_to_code(config, effect_id):
    args = [
        (AddressableLightRef, "it"),
        (Color, "current_color"),
        (bool, "initial_run"),
    ]
    lambda_ = await cg.process_lambda(config[CONF_LAMBDA], args, return_type=cg.void)
    var = cg.new_Pvariable(
        effect_id, config[CONF_NAME], lambda_, config[CONF_UPDATE_INTERVAL]
    )
    return var


@register_addressable_effect(
    "addressable_rainbow",
    AddressableRainbowLightEffect,
    "Rainbow",
    {
        cv.Optional(CONF_SPEED, default=10): cv.uint32_t,
        cv.Optional(CONF_WIDTH, default=50): cv.uint32_t,
    },
)
async def addressable_rainbow_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_speed(config[CONF_SPEED]))
    cg.add(var.set_width(config[CONF_WIDTH]))
    return var


@register_addressable_effect(
    "addressable_color_wipe",
    AddressableColorWipeEffect,
    "Color Wipe",
    {
        cv.Optional(
            CONF_COLORS, default=[{CONF_NUM_LEDS: 1, CONF_RANDOM: True}]
        ): cv.ensure_list(
            {
                cv.Optional(CONF_RED, default=1.0): cv.percentage,
                cv.Optional(CONF_GREEN, default=1.0): cv.percentage,
                cv.Optional(CONF_BLUE, default=1.0): cv.percentage,
                cv.Optional(CONF_WHITE, default=1.0): cv.percentage,
                cv.Optional(CONF_RANDOM, default=False): cv.boolean,
                cv.Required(CONF_NUM_LEDS): cv.All(cv.uint32_t, cv.Range(min=1)),
            }
        ),
        cv.Optional(
            CONF_ADD_LED_INTERVAL, default="0.1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_REVERSE, default=False): cv.boolean,
    },
)
async def addressable_color_wipe_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_add_led_interval(config[CONF_ADD_LED_INTERVAL]))
    cg.add(var.set_reverse(config[CONF_REVERSE]))
    colors = []
    for color in config.get(CONF_COLORS, []):
        colors.append(
            cg.StructInitializer(
                AddressableColorWipeEffectColor,
                ("r", int(round(color[CONF_RED] * 255))),
                ("g", int(round(color[CONF_GREEN] * 255))),
                ("b", int(round(color[CONF_BLUE] * 255))),
                ("w", int(round(color[CONF_WHITE] * 255))),
                ("random", color[CONF_RANDOM]),
                ("num_leds", color[CONF_NUM_LEDS]),
            )
        )
    cg.add(var.set_colors(colors))
    return var


@register_addressable_effect(
    "addressable_scan",
    AddressableScanEffect,
    "Scan",
    {
        cv.Optional(
            CONF_MOVE_INTERVAL, default="0.1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SCAN_WIDTH, default=1): cv.int_range(min=1),
    },
)
async def addressable_scan_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_move_interval(config[CONF_MOVE_INTERVAL]))
    cg.add(var.set_scan_width(config[CONF_SCAN_WIDTH]))
    return var


@register_addressable_effect(
    "addressable_twinkle",
    AddressableTwinkleEffect,
    "Twinkle",
    {
        cv.Optional(CONF_TWINKLE_PROBABILITY, default="5%"): cv.percentage,
        cv.Optional(
            CONF_PROGRESS_INTERVAL, default="4ms"
        ): cv.positive_time_period_milliseconds,
    },
)
async def addressable_twinkle_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_twinkle_probability(config[CONF_TWINKLE_PROBABILITY]))
    cg.add(var.set_progress_interval(config[CONF_PROGRESS_INTERVAL]))
    return var


@register_addressable_effect(
    "addressable_random_twinkle",
    AddressableRandomTwinkleEffect,
    "Random Twinkle",
    {
        cv.Optional(CONF_TWINKLE_PROBABILITY, default="5%"): cv.percentage,
        cv.Optional(
            CONF_PROGRESS_INTERVAL, default="32ms"
        ): cv.positive_time_period_milliseconds,
    },
)
async def addressable_random_twinkle_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_twinkle_probability(config[CONF_TWINKLE_PROBABILITY]))
    cg.add(var.set_progress_interval(config[CONF_PROGRESS_INTERVAL]))
    return var


@register_addressable_effect(
    "addressable_fireworks",
    AddressableFireworksEffect,
    "Fireworks",
    {
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="32ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SPARK_PROBABILITY, default="10%"): cv.percentage,
        cv.Optional(CONF_USE_RANDOM_COLOR, default=False): cv.boolean,
        cv.Optional(CONF_FADE_OUT_RATE, default=120): cv.uint8_t,
    },
)
async def addressable_fireworks_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_spark_probability(config[CONF_SPARK_PROBABILITY]))
    cg.add(var.set_use_random_color(config[CONF_USE_RANDOM_COLOR]))
    cg.add(var.set_fade_out_rate(config[CONF_FADE_OUT_RATE]))
    return var


@register_addressable_effect(
    "addressable_flicker",
    AddressableFlickerEffect,
    "Addressable Flicker",
    {
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="16ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_INTENSITY, default="5%"): cv.percentage,
    },
)
async def addressable_flicker_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    return var


def validate_effects(allowed_effects):
    @jschema_extractor("effects")
    def validator(value):
        # pylint: disable=comparison-with-callable
        if value == jschema_extractor:
            return (allowed_effects, EFFECTS_REGISTRY)
        value = cv.validate_registry("effect", EFFECTS_REGISTRY)(value)
        errors = []
        names = set()
        for i, x in enumerate(value):
            key = next(it for it in x.keys())
            if key not in allowed_effects:
                errors.append(
                    cv.Invalid(
                        f"The effect '{key}' is not allowed for this light type",
                        [i],
                    )
                )
                continue
            name = x[key][CONF_NAME]
            if name in names:
                errors.append(
                    cv.Invalid(
                        f"Found the effect name '{name}' twice. All effects must have unique names",
                        [i],
                    )
                )
                continue
            names.add(name)
        if errors:
            raise cv.MultipleInvalid(errors)
        return value

    return validator
