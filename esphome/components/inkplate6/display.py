import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, i2c
from esphome.const import (
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_WAKEUP_PIN,
    ESP_PLATFORM_ESP32,
)

DEPENDENCIES = ["i2c"]
ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONF_DISPLAY_DATA_0_PIN = "display_data_0_pin"
CONF_DISPLAY_DATA_1_PIN = "display_data_1_pin"
CONF_DISPLAY_DATA_2_PIN = "display_data_2_pin"
CONF_DISPLAY_DATA_3_PIN = "display_data_3_pin"
CONF_DISPLAY_DATA_4_PIN = "display_data_4_pin"
CONF_DISPLAY_DATA_5_PIN = "display_data_5_pin"
CONF_DISPLAY_DATA_6_PIN = "display_data_6_pin"
CONF_DISPLAY_DATA_7_PIN = "display_data_7_pin"

CONF_CL_PIN = "cl_pin"
CONF_CKV_PIN = "ckv_pin"
CONF_GREYSCALE = "greyscale"
CONF_GMOD_PIN = "gmod_pin"
CONF_GPIO0_ENABLE_PIN = "gpio0_enable_pin"
CONF_LE_PIN = "le_pin"
CONF_OE_PIN = "oe_pin"
CONF_PARTIAL_UPDATING = "partial_updating"
CONF_POWERUP_PIN = "powerup_pin"
CONF_SPH_PIN = "sph_pin"
CONF_SPV_PIN = "spv_pin"
CONF_VCOM_PIN = "vcom_pin"


inkplate6_ns = cg.esphome_ns.namespace("inkplate6")
Inkplate6 = inkplate6_ns.class_(
    "Inkplate6", cg.PollingComponent, i2c.I2CDevice, display.DisplayBuffer
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Inkplate6),
            cv.Optional(CONF_GREYSCALE, default=False): cv.boolean,
            cv.Optional(CONF_PARTIAL_UPDATING, default=True): cv.boolean,
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=10): cv.uint32_t,
            # Control pins
            cv.Required(CONF_CKV_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_GMOD_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_GPIO0_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_OE_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_POWERUP_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_SPH_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_SPV_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_VCOM_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_WAKEUP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CL_PIN, default=0): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_LE_PIN, default=2): pins.internal_gpio_output_pin_schema,
            # Data pins
            cv.Optional(
                CONF_DISPLAY_DATA_0_PIN, default=4
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_1_PIN, default=5
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_2_PIN, default=18
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_3_PIN, default=19
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_4_PIN, default=23
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_5_PIN, default=25
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_6_PIN, default=26
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_7_PIN, default=27
            ): pins.internal_gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x48)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield display.register_display(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_greyscale(config[CONF_GREYSCALE]))
    cg.add(var.set_partial_updating(config[CONF_PARTIAL_UPDATING]))
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))

    ckv = yield cg.gpio_pin_expression(config[CONF_CKV_PIN])
    cg.add(var.set_ckv_pin(ckv))

    gmod = yield cg.gpio_pin_expression(config[CONF_GMOD_PIN])
    cg.add(var.set_gmod_pin(gmod))

    gpio0_enable = yield cg.gpio_pin_expression(config[CONF_GPIO0_ENABLE_PIN])
    cg.add(var.set_gpio0_enable_pin(gpio0_enable))

    oe = yield cg.gpio_pin_expression(config[CONF_OE_PIN])
    cg.add(var.set_oe_pin(oe))

    powerup = yield cg.gpio_pin_expression(config[CONF_POWERUP_PIN])
    cg.add(var.set_powerup_pin(powerup))

    sph = yield cg.gpio_pin_expression(config[CONF_SPH_PIN])
    cg.add(var.set_sph_pin(sph))

    spv = yield cg.gpio_pin_expression(config[CONF_SPV_PIN])
    cg.add(var.set_spv_pin(spv))

    vcom = yield cg.gpio_pin_expression(config[CONF_VCOM_PIN])
    cg.add(var.set_vcom_pin(vcom))

    wakeup = yield cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])
    cg.add(var.set_wakeup_pin(wakeup))

    cl = yield cg.gpio_pin_expression(config[CONF_CL_PIN])
    cg.add(var.set_cl_pin(cl))

    le = yield cg.gpio_pin_expression(config[CONF_LE_PIN])
    cg.add(var.set_le_pin(le))

    display_data_0 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_0_PIN])
    cg.add(var.set_display_data_0_pin(display_data_0))

    display_data_1 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_1_PIN])
    cg.add(var.set_display_data_1_pin(display_data_1))

    display_data_2 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_2_PIN])
    cg.add(var.set_display_data_2_pin(display_data_2))

    display_data_3 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_3_PIN])
    cg.add(var.set_display_data_3_pin(display_data_3))

    display_data_4 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_4_PIN])
    cg.add(var.set_display_data_4_pin(display_data_4))

    display_data_5 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_5_PIN])
    cg.add(var.set_display_data_5_pin(display_data_5))

    display_data_6 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_6_PIN])
    cg.add(var.set_display_data_6_pin(display_data_6))

    display_data_7 = yield cg.gpio_pin_expression(config[CONF_DISPLAY_DATA_7_PIN])
    cg.add(var.set_display_data_7_pin(display_data_7))

    cg.add_build_flag("-DBOARD_HAS_PSRAM")
