import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA, CONF_NUM_CHIPS

DEPENDENCIES = ["spi"]

max7219_ns = cg.esphome_ns.namespace("max7219")
MAX7219Component = max7219_ns.class_(
    "MAX7219Component", cg.PollingComponent, spi.SPIDevice
)
MAX7219ComponentRef = MAX7219Component.operator("ref")

CONF_REVERSE_ENABLE = "reverse_enable"

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MAX7219Component),
            cv.Optional(CONF_NUM_CHIPS, default=1): cv.int_range(min=1, max=255),
            cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
            cv.Optional(CONF_REVERSE_ENABLE, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema())
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)
    yield display.register_display(var, config)

    cg.add(var.set_num_chips(config[CONF_NUM_CHIPS]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    cg.add(var.set_reverse(config[CONF_REVERSE_ENABLE]))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(MAX7219ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
