import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_BRIGHTNESS, CONF_CONTRAST, CONF_ID, CONF_LAMBDA

CODEOWNERS = ["@rremy"]

CONF_SCROLL_ENABLED = "scroll_enabled"
CONF_WRAP_ENABLED = "wrap_enabled"
CONF_CURSOR_TYPE = "cursor_type"

DEPENDENCIES = ["spi"]

cfa632_ns = cg.esphome_ns.namespace("cfa632")
CFA632 = cfa632_ns.class_("CFA632", cg.PollingComponent, spi.SPIDevice)

CursorType = cfa632_ns.enum("CursorType")
WIFI_POWER_SAVE_MODES = {
    "HIDDEN": CursorType.HIDDEN,
    "UNDERLINE": CursorType.UNDERLINE,
    "BLOCK": CursorType.BLOCK,
    "INVERTING_BLOCK": CursorType.INVERTING_BLOCK,
}

CONFIG_SCHEMA = cv.All(
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(CFA632),
            cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
            cv.Optional(CONF_CONTRAST, default=1.0): cv.percentage,
            cv.Optional(CONF_SCROLL_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_WRAP_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_CURSOR_TYPE, default="UNDERLINE"): cv.enum(
                WIFI_POWER_SAVE_MODES, upper=True
            ),
        }
    )
    .extend(spi.spi_device_schema())
    .extend(cv.polling_component_schema("1s"))
)


def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_BRIGHTNESS],
        config[CONF_CONTRAST],
        config[CONF_SCROLL_ENABLED],
        config[CONF_WRAP_ENABLED],
        config[CONF_CURSOR_TYPE],
    )
    yield cg.register_component(var, config)
    yield display.register_display(var, config)
    yield spi.register_spi_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA],
            [(CFA632.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
