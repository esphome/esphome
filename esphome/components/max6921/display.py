from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA


DEPENDENCIES = ["spi", "esp32"]
CODEOWNERS = ["@endym"]
CONF_NUM_DIGITS = "num_digits"
CONF_DEMO_MODE = "demo_mode"
CONF_LOAD_PIN = "load_pin"
CONF_BLANK_PIN = "blank_pin"
CONF_SEG_TO_OUT_MAP = "seg_to_out_map"
CONF_SEG_A_PIN = "seg_a_pin"
CONF_SEG_B_PIN = "seg_b_pin"
CONF_SEG_C_PIN = "seg_c_pin"
CONF_SEG_D_PIN = "seg_d_pin"
CONF_SEG_E_PIN = "seg_e_pin"
CONF_SEG_F_PIN = "seg_f_pin"
CONF_SEG_G_PIN = "seg_g_pin"
CONF_SEG_DP_PIN = "seg_dp_pin"


max6921_ns = cg.esphome_ns.namespace("max6921")
MAX6921Component = max6921_ns.class_(
    "MAX6921Component", cg.PollingComponent, spi.SPIDevice
)
MAX6921ComponentRef = MAX6921Component.operator("ref")

# optional "demo_mode" configuration
CONF_DEMO_MODE_OFF = "off"
CONF_DEMO_MODE_SCROLL_FONT = "scroll_font"
DemoMode = max6921_ns.enum("DemoMode")
DEMO_MODES = {
    CONF_DEMO_MODE_OFF: DemoMode.DEMO_MODE_OFF,
    CONF_DEMO_MODE_SCROLL_FONT: DemoMode.DEMO_MODE_SCROLL_FONT,
}


def validate_pin_numbers(value):
    # print(f"seg_to_out_pin_map: {value}")
    values = value.values()
    if (max(values) - min(values)) >= len(value):
        raise cv.Invalid("There must be no gaps between segment pin numbers")
    return value


SEG_TO_OUT_MAP_SCHEMA = cv.Schema(
    cv.All(
        {
            cv.Required(CONF_SEG_A_PIN): cv.int_range(min=0, max=19),
            cv.Required(CONF_SEG_B_PIN): cv.int_range(min=0, max=19),
            cv.Required(CONF_SEG_C_PIN): cv.int_range(min=0, max=19),
            cv.Required(CONF_SEG_D_PIN): cv.int_range(min=0, max=19),
            cv.Required(CONF_SEG_E_PIN): cv.int_range(min=0, max=19),
            cv.Required(CONF_SEG_F_PIN): cv.int_range(min=0, max=19),
            cv.Required(CONF_SEG_G_PIN): cv.int_range(min=0, max=19),
            cv.Optional(CONF_SEG_DP_PIN): cv.int_range(min=0, max=19),
        },
        validate_pin_numbers,
    )
)

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MAX6921Component),
            cv.Required(CONF_LOAD_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_BLANK_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_SEG_TO_OUT_MAP): SEG_TO_OUT_MAP_SCHEMA,
            cv.Required(CONF_NUM_DIGITS): cv.int_range(min=1, max=20),
            cv.Optional(CONF_INTENSITY, default=16): cv.int_range(min=0, max=16),
            cv.Optional(CONF_DEMO_MODE, default=CONF_DEMO_MODE_OFF): cv.enum(
                DEMO_MODES
            ),
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
    cg.add(
        var.set_seg_to_out_pin_map(
            cg.ArrayInitializer(
                *[
                    config[CONF_SEG_TO_OUT_MAP][seg]
                    for seg in config[CONF_SEG_TO_OUT_MAP]
                ]
            )
        )
    )
    cg.add(var.set_num_digits(config[CONF_NUM_DIGITS]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    cg.add(var.set_demo_mode(config[CONF_DEMO_MODE]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(MAX6921ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
