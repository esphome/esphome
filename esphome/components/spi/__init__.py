import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import pins
from esphome.const import (
    CONF_CLK_PIN,
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_SPI_ID,
    CONF_CS_PIN,
    CONF_NUMBER,
)
from esphome.core import coroutine_with_priority, CORE

CODEOWNERS = ["@esphome/core"]
spi_ns = cg.esphome_ns.namespace("spi")
SPIComponent = spi_ns.class_("SPIComponent", cg.Component)
SPIDevice = spi_ns.class_("SPIDevice")
SPIDataRate = spi_ns.enum("SPIDataRate")

SPI_DATA_RATE_OPTIONS = {
    80e6: SPIDataRate.DATA_RATE_80MHZ,
    40e6: SPIDataRate.DATA_RATE_40MHZ,
    20e6: SPIDataRate.DATA_RATE_20MHZ,
    10e6: SPIDataRate.DATA_RATE_10MHZ,
    8e6: SPIDataRate.DATA_RATE_8MHZ,
    5e6: SPIDataRate.DATA_RATE_5MHZ,
    4e6: SPIDataRate.DATA_RATE_4MHZ,
    2e6: SPIDataRate.DATA_RATE_2MHZ,
    1e6: SPIDataRate.DATA_RATE_1MHZ,
    2e5: SPIDataRate.DATA_RATE_200KHZ,
    75e3: SPIDataRate.DATA_RATE_75KHZ,
    1e3: SPIDataRate.DATA_RATE_1KHZ,
}
SPI_DATA_RATE_SCHEMA = cv.All(cv.frequency, cv.enum(SPI_DATA_RATE_OPTIONS))

CONF_FORCE_SW = "force_sw"
CONF_INTERFACE = "interface"


def get_spi_interface_list():
    if CORE.is_esp8266:
        return [1]
    if CORE.is_esp32:
        return [2, 3]
    if CORE.is_rp2040:
        return [0, 1]
    return 0


# Check that pins are suitable for HW spi
def validate_hw_pins(spi):
    clk_pin = spi[CONF_CLK_PIN][CONF_NUMBER]
    sdo_pin = spi[CONF_MOSI_PIN][CONF_NUMBER] if CONF_MOSI_PIN in spi else -1
    sdi_pin = spi[CONF_MISO_PIN][CONF_NUMBER] if CONF_MISO_PIN in spi else -1

    if CORE.is_esp8266:
        match spi[CONF_MOSI_PIN][CONF_NUMBER]:
            case 6:
                return sdo_pin == -1 or sdo_pin == 8 and sdi_pin == -1 or sdi_pin == 7
            case 14:
                return sdo_pin == -1 or sdo_pin == 13 and sdi_pin == -1 or sdi_pin == 12
            case _:
                return False
    return clk_pin >= 0


def validate_spi_config(config):
    available = get_spi_interface_list()
    for spi_index, spi in enumerate(config):
        interface = spi[CONF_INTERFACE]
        is_sw = spi[CONF_FORCE_SW] or interface == "software"
        if interface == "any":
            if is_sw or not validate_hw_pins(spi):
                spi[CONF_INTERFACE] = "software"
        if isinstance(interface, int):
            if is_sw:
                raise cv.Invalid("Software SPI does not use hardware interface")
            if interface not in available:
                raise cv.Invalid(f"SPI interface {interface} already claimed")
            available.remove(interface)

    # Second time around, assign interfaces if required
    for spi_index, spi in enumerate(config):
        if spi[CONF_INTERFACE] == "any" and len(available) != 0:
            interface = available[0]
            spi[CONF_INTERFACE] = interface
            available.remove(interface)

    return config


SPI_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIComponent),
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MISO_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_FORCE_SW, default=False): cv.boolean,
            cv.Optional(CONF_INTERFACE, default="any"): cv.Any(
                cv.string("any"),
                cv.string("software"),
                cv.int_range(get_spi_interface_list()[0], get_spi_interface_list()[-1]),
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_MISO_PIN, CONF_MOSI_PIN),
)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(SPI_SCHEMA),
    validate_spi_config,
)


@coroutine_with_priority(1.0)
async def to_code(configs):
    cg.add_global(spi_ns.using)
    for config in configs:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

        clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
        cg.add(var.set_clk(clk))
        if CONF_MISO_PIN in config:
            miso = await cg.gpio_pin_expression(config[CONF_MISO_PIN])
            cg.add(var.set_miso(miso))
        if CONF_MOSI_PIN in config:
            mosi = await cg.gpio_pin_expression(config[CONF_MOSI_PIN])
            cg.add(var.set_mosi(mosi))
        interface = config[CONF_INTERFACE]
        if isinstance(interface, int):
            # zero-index so can be used to index the array.
            interface = interface - get_spi_interface_list()[0]
            cg.add(var.set_interface(interface))

    if CORE.using_arduino:
        cg.add_library("SPI", None)


def spi_device_schema(cs_pin_required=True):
    """Create a schema for an SPI device.
    :param cs_pin_required: If true, make the CS_PIN required in the config.
    :return: The SPI device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_SPI_ID): cv.use_id(SPIComponent),
    }
    if cs_pin_required:
        schema[cv.Required(CONF_CS_PIN)] = pins.gpio_output_pin_schema
    else:
        schema[cv.Optional(CONF_CS_PIN)] = pins.gpio_output_pin_schema
    return cv.Schema(schema)


async def register_spi_device(var, config):
    parent = await cg.get_variable(config[CONF_SPI_ID])
    cg.add(var.set_spi_parent(parent))
    if CONF_CS_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cg.add(var.set_cs_pin(pin))


def final_validate_device_schema(name: str, *, require_mosi: bool, require_miso: bool):
    hub_schema = {}
    if require_miso:
        hub_schema[
            cv.Required(
                CONF_MISO_PIN,
                msg=f"Component {name} requires this spi bus to declare a miso_pin",
            )
        ] = cv.valid
    if require_mosi:
        hub_schema[
            cv.Required(
                CONF_MOSI_PIN,
                msg=f"Component {name} requires this spi bus to declare a mosi_pin",
            )
        ] = cv.valid

    return cv.Schema(
        {cv.Required(CONF_SPI_ID): fv.id_declaration_match_schema(hub_schema)},
        extra=cv.ALLOW_EXTRA,
    )
