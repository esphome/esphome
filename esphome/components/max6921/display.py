from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import display, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_DURATION,
    CONF_EFFECT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODE,
    CONF_POSITION,
    CONF_TEXT,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = ["spi", "esp32"]
CODEOWNERS = ["@endym"]
CONF_LOAD_PIN = "load_pin"
CONF_BLANK_PIN = "blank_pin"
CONF_OUT_PIN_MAPPING = "out_pin_mapping"
CONF_SEG_TO_OUT_MAP = "seg_to_out_map"
CONF_SEG_A_PIN = "seg_a_pin"
CONF_SEG_B_PIN = "seg_b_pin"
CONF_SEG_C_PIN = "seg_c_pin"
CONF_SEG_D_PIN = "seg_d_pin"
CONF_SEG_E_PIN = "seg_e_pin"
CONF_SEG_F_PIN = "seg_f_pin"
CONF_SEG_G_PIN = "seg_g_pin"
CONF_SEG_P_PIN = "seg_p_pin"
CONF_POS_TO_OUT_MAP = "pos_to_out_map"
CONF_POS_0_PIN = "pos_0_pin"
CONF_POS_1_PIN = "pos_1_pin"
CONF_POS_2_PIN = "pos_2_pin"
CONF_POS_3_PIN = "pos_3_pin"
CONF_POS_4_PIN = "pos_4_pin"
CONF_POS_5_PIN = "pos_5_pin"
CONF_POS_6_PIN = "pos_6_pin"
CONF_POS_7_PIN = "pos_7_pin"
CONF_POS_8_PIN = "pos_8_pin"
CONF_POS_9_PIN = "pos_9_pin"
CONF_POS_10_PIN = "pos_10_pin"
CONF_POS_11_PIN = "pos_11_pin"
CONF_POS_12_PIN = "pos_12_pin"
CONF_ALIGN = "align"
CONF_EFFECT_CYCLE_NUM = "effect_cycle_num"
CONF_CYCLE_NUM = "cycle_num"
CONF_OFF = "off"
CONF_REPEAT_INTERVAL = "repeat_interval"
CONF_REPEAT_NUM = "repeat_num"
CONF_SCROLL_FONT = "scroll_font"

max6921_ns = cg.esphome_ns.namespace("max6921")
MAX6921Component = max6921_ns.class_(
    "MAX6921Component", cg.PollingComponent, spi.SPIDevice
)
MAX6921ComponentRef = MAX6921Component.operator("ref")
SetBrightnessAction = max6921_ns.class_("SetBrightnessAction", automation.Action)
SetDemoModeAction = max6921_ns.class_("SetDemoModeAction", automation.Action)
SetTextAction = max6921_ns.class_("SetTextAction", automation.Action)


def validate_out_pin_mapping(value):
    # segment pins must not have gaps
    seg_pins = list(value[CONF_SEG_TO_OUT_MAP].values())
    if (max(seg_pins) - min(seg_pins)) >= len(seg_pins):
        raise cv.Invalid("There must be no gaps between segment pin numbers")
    # max. number of OUT pins
    pos_pins = list(value[CONF_POS_TO_OUT_MAP].values())
    mapped_out_pins = seg_pins + pos_pins
    # duplicates (and indirect max. pin number)
    if len(mapped_out_pins) != len(set(mapped_out_pins)):
        raise cv.Invalid("OUT pin duplicate")
    return value


OUT_PIN_MAPPING_SCHEMA = cv.Schema(
    cv.All(
        {
            cv.Required(CONF_SEG_TO_OUT_MAP): cv.Schema(
                {
                    cv.Required(CONF_SEG_A_PIN): cv.int_range(min=0, max=19),
                    cv.Required(CONF_SEG_B_PIN): cv.int_range(min=0, max=19),
                    cv.Required(CONF_SEG_C_PIN): cv.int_range(min=0, max=19),
                    cv.Required(CONF_SEG_D_PIN): cv.int_range(min=0, max=19),
                    cv.Required(CONF_SEG_E_PIN): cv.int_range(min=0, max=19),
                    cv.Required(CONF_SEG_F_PIN): cv.int_range(min=0, max=19),
                    cv.Required(CONF_SEG_G_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_SEG_P_PIN): cv.int_range(min=0, max=19),
                }
            ),
            cv.Required(CONF_POS_TO_OUT_MAP): cv.Schema(
                {
                    cv.Required(CONF_POS_0_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_1_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_2_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_3_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_4_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_5_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_6_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_7_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_8_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_9_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_10_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_11_PIN): cv.int_range(min=0, max=19),
                    cv.Optional(CONF_POS_12_PIN): cv.int_range(min=0, max=19),
                }
            ),
        },
        validate_out_pin_mapping,
    )
)


CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MAX6921Component),
            cv.Required(CONF_LOAD_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_BLANK_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_OUT_PIN_MAPPING): OUT_PIN_MAPPING_SCHEMA,
            cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.templatable(cv.percentage),
        }
    )
    .extend(cv.polling_component_schema("500ms"))
    .extend(spi.spi_device_schema(cs_pin_required=False))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await display.register_display(var, config)

    load_pin = await cg.gpio_pin_expression(config[CONF_LOAD_PIN])
    cg.add(var.set_load_pin(load_pin))
    blank_pin = await cg.gpio_pin_expression(config[CONF_BLANK_PIN])
    cg.add(var.set_blank_pin(blank_pin))
    # pass array of display segment pin numbers sorted by pin name...
    sorted_list_of_tuples = sorted(
        config[CONF_OUT_PIN_MAPPING][CONF_SEG_TO_OUT_MAP].items()
    )
    cg.add(
        var.set_seg_to_out_pin_map(
            cg.ArrayInitializer(*[tuple[1] for tuple in sorted_list_of_tuples])
        )
    )
    # pass array of display position pin numbers sorted by pin name...
    sorted_list_of_tuples = sorted(
        config[CONF_OUT_PIN_MAPPING][CONF_POS_TO_OUT_MAP].items()
    )
    cg.add(
        var.set_pos_to_out_pin_map(
            cg.ArrayInitializer(*[tuple[1] for tuple in sorted_list_of_tuples])
        )
    )
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(MAX6921ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))


ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MAX6921Component),
    }
)


ACTION_SET_BRIGHTNESS_SCHEMA = cv.All(
    automation.maybe_simple_id(
        ACTION_SCHEMA.extend(
            cv.Schema(
                {
                    cv.Required(CONF_BRIGHTNESS): cv.templatable(cv.percentage),
                }
            )
        )
    ),
)


@automation.register_action(
    "max6921.set_brightness", SetBrightnessAction, ACTION_SET_BRIGHTNESS_SCHEMA
)
async def max6921_set_brightness_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_BRIGHTNESS], args, float)
    cg.add(var.set_brightness(template_))
    return var


def validate_action_set_text(value):
    duration = value.get(CONF_DURATION)
    effect_cycle_num = value.get(CONF_EFFECT_CYCLE_NUM)
    repeat_interval = value.get(CONF_REPEAT_INTERVAL)
    repeat_num = value.get(CONF_REPEAT_NUM)
    if (
        not isinstance(duration, cv.Lambda)
        and not isinstance(effect_cycle_num, cv.Lambda)
        and not isinstance(repeat_interval, cv.Lambda)
        and not isinstance(repeat_num, cv.Lambda)
    ):
        if ((repeat_interval.total_milliseconds > 0) or (repeat_num > 0)) and (
            (effect_cycle_num == 0) and (duration.total_milliseconds == 0)
        ):
            raise cv.Invalid(
                f'If "{CONF_REPEAT_INTERVAL}" or "{CONF_REPEAT_NUM}" is non-zero, '
                f'then "{CONF_EFFECT_CYCLE_NUM}" or "{CONF_DURATION}" must be non-zero too'
            )
        if (repeat_num > 0) and (repeat_interval.total_milliseconds == 0):
            raise cv.Invalid(
                f'If "{CONF_REPEAT_NUM}" is non-zero, then "{CONF_REPEAT_INTERVAL}" must be non-zero too'
            )
    return value


ACTION_SET_TEXT_SCHEMA = cv.All(
    automation.maybe_simple_id(
        ACTION_SCHEMA.extend(
            cv.Schema(
                {
                    cv.Required(CONF_TEXT): cv.templatable(cv.string),
                    cv.Optional(CONF_POSITION, default=0): cv.templatable(
                        cv.int_range(min=0, max=13)
                    ),
                    cv.Optional(CONF_ALIGN, default="center"): cv.templatable(
                        cv.string
                    ),
                    cv.Optional(CONF_DURATION, default="0ms"): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                    cv.Optional(CONF_EFFECT, default="none"): cv.templatable(cv.string),
                    cv.Optional(CONF_EFFECT_CYCLE_NUM, default=0): cv.templatable(
                        cv.uint8_t
                    ),
                    cv.Optional(CONF_REPEAT_INTERVAL, default="0ms"): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                    cv.Optional(CONF_REPEAT_NUM, default=0): cv.templatable(cv.uint8_t),
                    cv.Optional(CONF_UPDATE_INTERVAL, default="150ms"): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                }
            )
        )
    ),
    validate_action_set_text,
)


@automation.register_action("max6921.set_text", SetTextAction, ACTION_SET_TEXT_SCHEMA)
async def max6921_set_text_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_TEXT], args, cg.std_string)
    cg.add(var.set_text(template_))
    if CONF_POSITION in config:
        template_ = await cg.templatable(config[CONF_POSITION], args, cg.uint8)
        cg.add(var.set_text_position(template_))
    if CONF_ALIGN in config:
        template_ = await cg.templatable(config[CONF_ALIGN], args, cg.std_string)
        cg.add(var.set_text_align(template_))
    if CONF_DURATION in config:
        template_ = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
        cg.add(var.set_text_effect_duration(template_))
    if CONF_EFFECT in config:
        template_ = await cg.templatable(config[CONF_EFFECT], args, cg.std_string)
        cg.add(var.set_text_effect(template_))
    if CONF_EFFECT_CYCLE_NUM in config:
        template_ = await cg.templatable(config[CONF_EFFECT_CYCLE_NUM], args, cg.uint8)
        cg.add(var.set_text_effect_cycle_num(template_))
    if CONF_REPEAT_NUM in config:
        template_ = await cg.templatable(config[CONF_REPEAT_NUM], args, cg.uint8)
        cg.add(var.set_text_repeat_num(template_))
    if CONF_REPEAT_INTERVAL in config:
        template_ = await cg.templatable(config[CONF_REPEAT_INTERVAL], args, cg.uint32)
        cg.add(var.set_text_repeat_interval(template_))
    if CONF_UPDATE_INTERVAL in config:
        template_ = await cg.templatable(config[CONF_UPDATE_INTERVAL], args, cg.uint32)
        cg.add(var.set_text_effect_update_interval(template_))
    return var


ACTION_SET_DEMO_MODE_SCHEMA = cv.All(
    automation.maybe_simple_id(
        ACTION_SCHEMA.extend(
            cv.Schema(
                {
                    cv.Required(CONF_MODE): cv.templatable(cv.string),
                    cv.Optional(CONF_UPDATE_INTERVAL, default="150ms"): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                    cv.Optional(CONF_CYCLE_NUM, default=0): cv.templatable(cv.uint8_t),
                }
            )
        )
    ),
)


@automation.register_action(
    "max6921.set_demo_mode", SetDemoModeAction, ACTION_SET_DEMO_MODE_SCHEMA
)
async def max6921_set_demo_mode_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_MODE], args, cg.std_string)
    cg.add(var.set_mode(template_))
    if CONF_UPDATE_INTERVAL in config:
        template_ = await cg.templatable(config[CONF_UPDATE_INTERVAL], args, cg.uint32)
        cg.add(var.set_demo_update_interval(template_))
    if CONF_CYCLE_NUM in config:
        template_ = await cg.templatable(config[CONF_CYCLE_NUM], args, cg.uint8)
        cg.add(var.set_demo_cycle_num(template_))
    return var
