import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.components import spi
from esphome.components import lora
from esphome.const import CONF_ID


DEPENDENCIES = ["spi"]

CONF_DI0_PIN = "di0_pin"
CONF_RST_PIN = "rst_pin"
CONF_BAND = "band"


sx127x_ns = cg.esphome_ns.namespace("sx127x")
SX127X = sx127x_ns.class_(
    "SX127X", lora.LoraComponent, sensor.Sensor, cg.Component, spi.SPIDevice
)

SX127XDIOPin = sx127x_ns.class_("SX127XDIOPin", cg.GPIOPin)

SX127XBands = sx127x_ns.enum("SX127XBANDS")


BANDS = {
    "433MHZ": 433,
    "868MHZ": 868,
    "915MHZ": 915,
    "923MHZ": 923,
}

SX127X_BAND = cv.enum(BANDS, upper=True, space="_")

CONFIG_SCHEMA = lora.LORA_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SX127X),
        cv.Required(CONF_BAND): SX127X_BAND,
        cv.Required(CONF_DI0_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_RST_PIN): pins.gpio_output_pin_schema,
    }
).extend(spi.spi_device_schema(cs_pin_required=True))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield spi.register_spi_device(var, config)
    yield cg.register_component(var, config)

    di0 = yield cg.gpio_pin_expression(config[CONF_DI0_PIN])
    cg.add(var.set_di0_pin(di0))

    rst = yield cg.gpio_pin_expression(config[CONF_RST_PIN])
    cg.add(var.set_rst_pin(rst))

    cg.add(var.set_band(config[CONF_BAND]))

    cg.add(var.set_sync_word_internal(config[lora.CONF_SYNC_WORD]))
