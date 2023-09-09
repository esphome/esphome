import re

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components.esp32.const import (
    KEY_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
)
from esphome import pins
from esphome.const import (
    CONF_CLK_PIN,
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_SPI_ID,
    CONF_CS_PIN,
    CONF_NUMBER,
    CONF_INVERTED,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
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
CONF_INTERFACE_INDEX = "interface_index"


def get_target_platform():
    return (
        CORE.data[KEY_CORE][KEY_TARGET_PLATFORM]
        if KEY_TARGET_PLATFORM in CORE.data[KEY_CORE]
        else ""
    )


def get_target_variant():
    return (
        CORE.data[KEY_ESP32][KEY_VARIANT] if KEY_VARIANT in CORE.data[KEY_ESP32] else ""
    )


# Get a list of available hardware interfaces based on target and variant.
# The returned value is a list of lists of names
def get_hw_interface_list():
    target_platform = get_target_platform()
    if target_platform == "esp8266":
        return [["spi", "hspi"]]
    if target_platform == "esp32":
        if get_target_variant() in [
            VARIANT_ESP32C2,
            VARIANT_ESP32C3,
            VARIANT_ESP32C6,
            VARIANT_ESP32H2,
        ]:
            return [["spi", "spi2"]]
        return [["spi", "spi2"], ["spi3"]]
    if target_platform == "rp2040":
        return [["spi"]]
    return []


# Given an SPI name, return the index of it in the available list
def get_spi_index(name):
    for i, ilist in enumerate(get_hw_interface_list()):
        if name in ilist:
            return i
    # Should never get to here.
    raise cv.Invalid(f"{name} is not an available SPI")


# Check that pins are suitable for HW spi
# TODO verify that the pins are internal
def validate_hw_pins(spi):
    clk_pin = spi[CONF_CLK_PIN]
    if clk_pin[CONF_INVERTED]:
        return False
    clk_pin_no = clk_pin[CONF_NUMBER]
    sdo_pin_no = -1
    sdi_pin_no = -1
    if CONF_MOSI_PIN in spi:
        sdo_pin = spi[CONF_MOSI_PIN]
        if sdo_pin[CONF_INVERTED]:
            return False
        sdo_pin_no = sdo_pin[CONF_NUMBER]
    if CONF_MISO_PIN in spi:
        sdi_pin = spi[CONF_MISO_PIN]
        if sdi_pin[CONF_INVERTED]:
            return False
        sdi_pin_no = sdi_pin[CONF_NUMBER]

    target_platform = get_target_platform()
    if target_platform == "esp8266":
        if clk_pin_no == 6:
            return sdo_pin_no in (-1, 8) and sdi_pin_no in (-1, 7)
        if clk_pin_no == 14:
            return sdo_pin_no in (-1, 13) and sdi_pin_no in (-1, 12)
        return False

    if target_platform == "esp32":
        return clk_pin_no >= 0

    return False


def validate_spi_config(config):
    available = list(range(len(get_hw_interface_list())))
    for spi in config:
        interface = spi[CONF_INTERFACE]
        if spi[CONF_FORCE_SW]:
            if interface == "any":
                spi[CONF_INTERFACE] = interface = "software"
            elif interface != "software":
                raise cv.Invalid("force_sw is deprecated - use interface: software")
        if interface == "software":
            pass
        elif interface == "any":
            if not validate_hw_pins(spi):
                spi[CONF_INTERFACE] = "software"
        elif interface == "hardware":
            if len(available) == 0:
                raise cv.Invalid("No hardware interface available")
            index = spi[CONF_INTERFACE_INDEX] = available[0]
            available.remove(index)
        else:
            # Must be a specific name
            index = spi[CONF_INTERFACE_INDEX] = get_spi_index(interface)
            if index not in available:
                raise cv.Invalid(
                    f"interface '{interface}' not available here (may be already assigned)"
                )
            available.remove(index)

    # Second time around:
    # Any specific names and any 'hardware' requests will have already been filled,
    # so just need to assign remaining hardware to 'any' requests.
    for spi in config:
        if spi[CONF_INTERFACE] == "any" and len(available) != 0:
            index = available[0]
            spi[CONF_INTERFACE_INDEX] = index
            available.remove(index)
        if CONF_INTERFACE_INDEX in spi and not validate_hw_pins(spi):
            raise cv.Invalid("Invalid pin selections for hardware SPI interface")

    return config


# Given an SPI index, convert to a string that represents the C++ object for it.
def get_spi_interface(index):
    if CORE.using_esp_idf:
        return ["SPI2_HOST", "SPI3_HOST"][index]
    # Arduino code follows
    platform = get_target_platform()
    if platform == "rp2040":
        return "&spi1"
    if index == 0:
        return "&SPI"
    # Following code can't apply to C2, H2 or 8266 since they have only one SPI
    if get_target_variant() in (VARIANT_ESP32S3, VARIANT_ESP32S2):
        return "new SPIClass(FSPI)"
    return "return new SPIClass(HSPI)"


SPI_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIComponent),
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MISO_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_FORCE_SW, default=False): cv.boolean,
            cv.Optional(CONF_INTERFACE, default="any"): cv.one_of(
                *sum(get_hw_interface_list(), ["software", "hardware", "any"]),
                lower=True,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_MISO_PIN, CONF_MOSI_PIN),
    cv.only_on(["esp32", "esp8266", "rp2040"]),
)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(SPI_SCHEMA),
    validate_spi_config,
)


@coroutine_with_priority(1.0)
async def to_code(configs):
    cg.add_global(spi_ns.using)
    for spi in configs:
        var = cg.new_Pvariable(spi[CONF_ID])
        await cg.register_component(var, spi)

        clk = await cg.gpio_pin_expression(spi[CONF_CLK_PIN])
        cg.add(var.set_clk(clk))
        if CONF_MISO_PIN in spi:
            miso = await cg.gpio_pin_expression(spi[CONF_MISO_PIN])
            cg.add(var.set_miso(miso))
        if CONF_MOSI_PIN in spi:
            mosi = await cg.gpio_pin_expression(spi[CONF_MOSI_PIN])
            cg.add(var.set_mosi(mosi))
        if CONF_INTERFACE_INDEX in spi:
            index = spi[CONF_INTERFACE_INDEX]
            cg.add(var.set_interface(cg.RawExpression(get_spi_interface(index))))
            cg.add(
                var.set_interface_name(
                    re.sub(
                        r"\W", "", get_spi_interface(index).replace("new SPIClass", "")
                    )
                )
            )

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
