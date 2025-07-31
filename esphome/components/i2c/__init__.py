import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
    CONF_SCAN,
    CONF_SCL,
    CONF_SDA,
    CONF_TIMEOUT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    PlatformFramework,
)
from esphome.core import CORE, coroutine_with_priority
import esphome.final_validate as fv

LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@esphome/core"]
i2c_ns = cg.esphome_ns.namespace("i2c")
I2CBus = i2c_ns.class_("I2CBus")
InternalI2CBus = i2c_ns.class_("InternalI2CBus", I2CBus)
ArduinoI2CBus = i2c_ns.class_("ArduinoI2CBus", InternalI2CBus, cg.Component)
IDFI2CBus = i2c_ns.class_("IDFI2CBus", InternalI2CBus, cg.Component)
I2CDevice = i2c_ns.class_("I2CDevice")


CONF_SDA_PULLUP_ENABLED = "sda_pullup_enabled"
CONF_SCL_PULLUP_ENABLED = "scl_pullup_enabled"
MULTI_CONF = True


def _bus_declare_type(value):
    if CORE.using_arduino:
        return cv.declare_id(ArduinoI2CBus)(value)
    if CORE.using_esp_idf:
        return cv.declare_id(IDFI2CBus)(value)
    raise NotImplementedError


def validate_config(config):
    if (
        config[CONF_SCAN]
        and CORE.is_esp32
        and CORE.using_esp_idf
        and esp32.get_esp32_variant()
        in [
            esp32.const.VARIANT_ESP32C5,
            esp32.const.VARIANT_ESP32C6,
            esp32.const.VARIANT_ESP32P4,
        ]
    ):
        version: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
        if version.major == 5 and (
            (version.minor == 3 and version.patch <= 3)
            or (version.minor == 4 and version.patch <= 1)
        ):
            LOGGER.warning(
                "There is a bug in esp-idf version %s that breaks I2C scan, I2C scan "
                "has been disabled, see https://github.com/esphome/issues/issues/7128",
                str(version),
            )
            config[CONF_SCAN] = False
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _bus_declare_type,
            cv.Optional(CONF_SDA, default="SDA"): pins.internal_gpio_pin_number,
            cv.SplitDefault(CONF_SDA_PULLUP_ENABLED, esp32_idf=True): cv.All(
                cv.only_with_esp_idf, cv.boolean
            ),
            cv.Optional(CONF_SCL, default="SCL"): pins.internal_gpio_pin_number,
            cv.SplitDefault(CONF_SCL_PULLUP_ENABLED, esp32_idf=True): cv.All(
                cv.only_with_esp_idf, cv.boolean
            ),
            cv.Optional(CONF_FREQUENCY, default="50kHz"): cv.All(
                cv.frequency, cv.Range(min=0, min_included=False)
            ),
            cv.Optional(CONF_TIMEOUT): cv.positive_time_period,
            cv.Optional(CONF_SCAN, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_RP2040]),
    validate_config,
)


@coroutine_with_priority(1.0)
async def to_code(config):
    cg.add_global(i2c_ns.using)
    cg.add_define("USE_I2C")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sda_pin(config[CONF_SDA]))
    if CONF_SDA_PULLUP_ENABLED in config:
        cg.add(var.set_sda_pullup_enabled(config[CONF_SDA_PULLUP_ENABLED]))
    cg.add(var.set_scl_pin(config[CONF_SCL]))
    if CONF_SCL_PULLUP_ENABLED in config:
        cg.add(var.set_scl_pullup_enabled(config[CONF_SCL_PULLUP_ENABLED]))

    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_scan(config[CONF_SCAN]))
    if CONF_TIMEOUT in config:
        cg.add(var.set_timeout(int(config[CONF_TIMEOUT].total_microseconds)))
    if CORE.using_arduino:
        cg.add_library("Wire", None)


def i2c_device_schema(default_address):
    """Create a schema for a i2c device.

    :param default_address: The default address of the i2c device, can be None to represent
      a required option.
    :return: The i2c device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CBus),
        cv.Optional("multiplexer"): cv.invalid(
            "This option has been removed, please see "
            "the tca9584a docs for the updated way to use multiplexers"
        ),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.i2c_address
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.i2c_address
    return cv.Schema(schema)


async def register_i2c_device(var, config):
    """Register an i2c device with the given config.

    Sets the i2c bus to use and the i2c address.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(parent))
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))


def final_validate_device_schema(
    name: str,
    *,
    min_frequency: cv.frequency = None,
    max_frequency: cv.frequency = None,
    min_timeout: cv.time_period = None,
    max_timeout: cv.time_period = None,
):
    hub_schema = {}
    if (min_frequency is not None) and (max_frequency is not None):
        hub_schema[cv.Required(CONF_FREQUENCY)] = cv.Range(
            min=cv.frequency(min_frequency),
            min_included=True,
            max=cv.frequency(max_frequency),
            max_included=True,
            msg=f"Component {name} requires a frequency between {min_frequency} and {max_frequency} for the I2C bus",
        )
    elif min_frequency is not None:
        hub_schema[cv.Required(CONF_FREQUENCY)] = cv.Range(
            min=cv.frequency(min_frequency),
            min_included=True,
            msg=f"Component {name} requires a minimum frequency of {min_frequency} for the I2C bus",
        )
    elif max_frequency is not None:
        hub_schema[cv.Required(CONF_FREQUENCY)] = cv.Range(
            max=cv.frequency(max_frequency),
            max_included=True,
            msg=f"Component {name} cannot be used with a frequency of over {max_frequency} for the I2C bus",
        )

    if (min_timeout is not None) and (max_timeout is not None):
        hub_schema[cv.Required(CONF_TIMEOUT)] = cv.Range(
            min=cv.time_period(min_timeout),
            min_included=True,
            max=cv.time_period(max_timeout),
            max_included=True,
            msg=f"Component {name} requires a timeout between {min_timeout} and {max_timeout} for the I2C bus",
        )
    elif min_timeout is not None:
        hub_schema[cv.Required(CONF_TIMEOUT)] = cv.Range(
            min=cv.time_period(min_timeout),
            min_included=True,
            msg=f"Component {name} requires a minimum timeout of {min_timeout} for the I2C bus",
        )
    elif max_timeout is not None:
        hub_schema[cv.Required(CONF_TIMEOUT)] = cv.Range(
            max=cv.time_period(max_timeout),
            max_included=True,
            msg=f"Component {name} cannot be used with a timeout of over {max_timeout} for the I2C bus",
        )

    return cv.Schema(
        {cv.Required(CONF_I2C_ID): fv.id_declaration_match_schema(hub_schema)},
        extra=cv.ALLOW_EXTRA,
    )


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "i2c_bus_arduino.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.RP2040_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "i2c_bus_esp_idf.cpp": {PlatformFramework.ESP32_IDF},
    }
)
