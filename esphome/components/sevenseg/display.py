import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.pins import gpio_output_pin_schema

sevensegment_ns = cg.esphome_ns.namespace("sevenseg")
SEVENSEGComponent = sevensegment_ns.class_("SEVENSEGComponent", cg.PollingComponent)
SEVENSEGComponentRef = SEVENSEGComponent.operator("ref")

CONF_A_PIN = "a_pin"
CONF_B_PIN = "b_pin"
CONF_C_PIN = "c_pin"
CONF_D_PIN = "d_pin"
CONF_E_PIN = "e_pin"
CONF_F_PIN = "f_pin"
CONF_G_PIN = "g_pin"
CONF_DP_PIN = "dp_pin"
CONF_HOLD_TIME = "hold_time"
CONF_BLANK_TIME = "blank_time"
CONF_DIGITS = "digits"

CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SEVENSEGComponent),
        cv.Required(CONF_A_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_B_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_C_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_D_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_E_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_F_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_G_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_DP_PIN): cv.ensure_schema(gpio_output_pin_schema),
        cv.Required(CONF_DIGITS): cv.ensure_list(gpio_output_pin_schema),
        cv.Optional(CONF_HOLD_TIME, default="5"): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_BLANK_TIME, default="0"): cv.int_range(min=0, max=65535),
    }
).extend(cv.polling_component_schema("25ms"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    # Setup each pin
    pin_a = await cg.gpio_pin_expression(config[CONF_A_PIN])
    cg.add(var.set_a_pin(pin_a))

    pin_b = await cg.gpio_pin_expression(config[CONF_B_PIN])
    cg.add(var.set_b_pin(pin_b))

    pin_c = await cg.gpio_pin_expression(config[CONF_C_PIN])
    cg.add(var.set_c_pin(pin_c))

    pin_d = await cg.gpio_pin_expression(config[CONF_D_PIN])
    cg.add(var.set_d_pin(pin_d))

    pin_e = await cg.gpio_pin_expression(config[CONF_E_PIN])
    cg.add(var.set_e_pin(pin_e))

    pin_f = await cg.gpio_pin_expression(config[CONF_F_PIN])
    cg.add(var.set_f_pin(pin_f))

    pin_g = await cg.gpio_pin_expression(config[CONF_G_PIN])
    cg.add(var.set_g_pin(pin_g))

    pin_dp = await cg.gpio_pin_expression(config[CONF_DP_PIN])
    cg.add(var.set_dp_pin(pin_dp))

    hold_time = await cg.add(config[CONF_HOLD_TIME])
    cg.add(var.set_hold_time(hold_time))

    blank_time = await cg.add(config[CONF_BLANK_TIME])
    cg.add(var.set_blank_time(blank_time))

    digits = []
    for pin in config[CONF_DIGITS]:
        pin_digit = await cg.gpio_pin_expression(pin)
        digits.append(pin_digit)
    cg.add(var.set_digits(digits))

    # cg.add(var.set_num_chips(config[CONF_NUM_CHIPS]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(SEVENSEGComponent, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
