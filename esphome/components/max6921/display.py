from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA


DEPENDENCIES = ["spi", "esp32"]
CODEOWNERS = ["@endym"]
CONF_DEMO_MODE = "demo_mode"
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
CONF_SEG_DP_PIN = "seg_dp_pin"
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
    # if (len(mapped_out_pins) > 20):
    #     raise cv.Invalid("Not more than 20 OUT pins supported")
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
                    cv.Optional(CONF_SEG_DP_PIN): cv.int_range(min=0, max=19),
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
                    config[CONF_OUT_PIN_MAPPING][CONF_SEG_TO_OUT_MAP][seg]
                    for seg in config[CONF_OUT_PIN_MAPPING][CONF_SEG_TO_OUT_MAP]
                ]
            )
        )
    )
    cg.add(
        var.set_pos_to_out_pin_map(
            cg.ArrayInitializer(
                *[
                    config[CONF_OUT_PIN_MAPPING][CONF_POS_TO_OUT_MAP][seg]
                    for seg in config[CONF_OUT_PIN_MAPPING][CONF_POS_TO_OUT_MAP]
                ]
            )
        )
    )
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    cg.add(var.set_demo_mode(config[CONF_DEMO_MODE]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(MAX6921ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
