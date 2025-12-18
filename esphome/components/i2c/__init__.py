import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    get_esp32_variant,
)
from esphome.components.esp32.gpio_esp32_c5 import esp32_c5_validate_lp_i2c
from esphome.components.esp32.gpio_esp32_c6 import esp32_c6_validate_lp_i2c
from esphome.components.esp32.gpio_esp32_p4 import esp32_p4_validate_lp_i2c
from esphome.components.zephyr import (
    zephyr_add_overlay,
    zephyr_add_prj_conf,
    zephyr_data,
)
from esphome.components.zephyr.const import KEY_BOARD
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_FREQUENCY,
    CONF_I2C,
    CONF_I2C_ID,
    CONF_ID,
    CONF_LOW_POWER_MODE,
    CONF_SCAN,
    CONF_SCL,
    CONF_SDA,
    CONF_TIMEOUT,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_NRF52,
    PLATFORM_RP2040,
    PlatformFramework,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv

LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@esphome/core"]
i2c_ns = cg.esphome_ns.namespace("i2c")
I2CBus = i2c_ns.class_("I2CBus")
InternalI2CBus = i2c_ns.class_("InternalI2CBus", I2CBus)
ArduinoI2CBus = i2c_ns.class_("ArduinoI2CBus", InternalI2CBus, cg.Component)
IDFI2CBus = i2c_ns.class_("IDFI2CBus", InternalI2CBus, cg.Component)
ZephyrI2CBus = i2c_ns.class_("ZephyrI2CBus", I2CBus, cg.Component)
I2CDevice = i2c_ns.class_("I2CDevice")

ESP32_I2C_CAPABILITIES = {
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32/include/soc/soc_caps.h
    VARIANT_ESP32: {"NUM": 2, "HP": 2},
    VARIANT_ESP32C2: {"NUM": 1, "HP": 1},
    VARIANT_ESP32C3: {"NUM": 1, "HP": 1},
    VARIANT_ESP32C5: {"NUM": 2, "HP": 1, "LP": 1},
    VARIANT_ESP32C6: {"NUM": 2, "HP": 1, "LP": 1},
    VARIANT_ESP32C61: {"NUM": 1, "HP": 1},
    VARIANT_ESP32H2: {"NUM": 2, "HP": 2},
    VARIANT_ESP32P4: {"NUM": 3, "HP": 2, "LP": 1},
    VARIANT_ESP32S2: {"NUM": 2, "HP": 2},
    VARIANT_ESP32S3: {"NUM": 2, "HP": 2},
}
VALIDATE_LP_I2C = {
    VARIANT_ESP32C5: esp32_c5_validate_lp_i2c,
    VARIANT_ESP32C6: esp32_c6_validate_lp_i2c,
    VARIANT_ESP32P4: esp32_p4_validate_lp_i2c,
}
LP_I2C_VARIANT = list(VALIDATE_LP_I2C.keys())

CONF_SDA_PULLUP_ENABLED = "sda_pullup_enabled"
CONF_SCL_PULLUP_ENABLED = "scl_pullup_enabled"
MULTI_CONF = True


def _bus_declare_type(value):
    if CORE.is_esp32:
        return cv.declare_id(IDFI2CBus)(value)
    if CORE.using_arduino:
        return cv.declare_id(ArduinoI2CBus)(value)
    if CORE.using_zephyr:
        return cv.declare_id(ZephyrI2CBus)(value)
    raise NotImplementedError


def validate_config(config):
    if CORE.is_esp32:
        return cv.require_framework_version(
            esp_idf=cv.Version(5, 4, 2), esp32_arduino=cv.Version(3, 2, 1)
        )(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _bus_declare_type,
            cv.Optional(CONF_SDA, default="SDA"): pins.internal_gpio_pin_number,
            cv.SplitDefault(CONF_SDA_PULLUP_ENABLED, esp32=True): cv.All(
                cv.only_on_esp32, cv.boolean
            ),
            cv.Optional(CONF_SCL, default="SCL"): pins.internal_gpio_pin_number,
            cv.SplitDefault(CONF_SCL_PULLUP_ENABLED, esp32=True): cv.All(
                cv.only_on_esp32, cv.boolean
            ),
            cv.SplitDefault(
                CONF_FREQUENCY,
                esp32="50kHz",
                esp8266="50kHz",
                rp2040="50kHz",
                nrf52="100kHz",
            ): cv.All(
                cv.frequency,
                cv.float_range(min=0, min_included=False),
            ),
            cv.Optional(CONF_TIMEOUT): cv.All(
                cv.only_with_framework(["arduino", "esp-idf"]),
                cv.positive_time_period,
            ),
            cv.Optional(CONF_SCAN, default=True): cv.boolean,
            cv.Optional(CONF_LOW_POWER_MODE): cv.All(
                cv.only_on_esp32,
                esp32.only_on_variant(
                    supported=LP_I2C_VARIANT, msg_prefix="Low power i2c"
                ),
                cv.boolean,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_RP2040, PLATFORM_NRF52]),
    validate_config,
)


def _final_validate(config):
    full_config = fv.full_config.get()[CONF_I2C]
    if CORE.using_zephyr and len(full_config) > 1:
        raise cv.Invalid("Second i2c is not implemented on Zephyr yet")
    if CORE.using_esp_idf and get_esp32_variant() in ESP32_I2C_CAPABILITIES:
        variant = get_esp32_variant()
        max_num = ESP32_I2C_CAPABILITIES[variant]["NUM"]
        if len(full_config) > max_num:
            raise cv.Invalid(
                f"The maximum number of i2c interfaces for {variant} is {max_num}"
            )
        if variant in LP_I2C_VARIANT:
            max_lp_num = ESP32_I2C_CAPABILITIES[variant]["LP"]
            max_hp_num = ESP32_I2C_CAPABILITIES[variant]["HP"]
            lp_num = sum(
                CONF_LOW_POWER_MODE in conf and conf[CONF_LOW_POWER_MODE]
                for conf in full_config
            )
            hp_num = len(full_config) - lp_num
            if CONF_LOW_POWER_MODE in config and config[CONF_LOW_POWER_MODE]:
                VALIDATE_LP_I2C[variant](config)
            if lp_num > max_lp_num:
                raise cv.Invalid(
                    f"The maximum number of low power i2c interfaces for {variant} is {max_lp_num}"
                )
            if hp_num > max_hp_num:
                raise cv.Invalid(
                    f"The maximum number of high power i2c interfaces for {variant} is {max_hp_num}"
                )


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.BUS)
async def to_code(config):
    cg.add_global(i2c_ns.using)
    cg.add_define("USE_I2C")
    if CORE.using_zephyr:
        zephyr_add_prj_conf("I2C", True)
        i2c = "i2c0"
        if zephyr_data()[KEY_BOARD] in ["xiao_ble"]:
            i2c = "i2c1"
        zephyr_add_overlay(
            f"""
                &pinctrl {{
                    {i2c}_default: {i2c}_default {{
                        group1 {{
                            psels = <NRF_PSEL(TWIM_SDA, {config[CONF_SDA] // 32}, {config[CONF_SDA] % 32})>,
                                <NRF_PSEL(TWIM_SCL, {config[CONF_SCL] // 32}, {config[CONF_SCL] % 32})>;
                        }};
                    }};
                    {i2c}_sleep: {i2c}_sleep {{
                        group1 {{
                            psels = <NRF_PSEL(TWIM_SDA, {config[CONF_SDA] // 32}, {config[CONF_SDA] % 32})>,
                                <NRF_PSEL(TWIM_SCL, {config[CONF_SCL] // 32}, {config[CONF_SCL] % 32})>;
                            low-power-enable;
                        }};
                    }};
                }};
            """
        )
        var = cg.new_Pvariable(
            config[CONF_ID], MockObj(f"DEVICE_DT_GET(DT_NODELABEL({i2c}))")
        )
    else:
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
    if CORE.using_arduino and not CORE.is_esp32:
        cg.add_library("Wire", None)
    if CONF_LOW_POWER_MODE in config:
        cg.add(var.set_lp_mode(bool(config[CONF_LOW_POWER_MODE])))


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
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.RP2040_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "i2c_bus_esp_idf.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "i2c_bus_zephyr.cpp": {PlatformFramework.NRF52_ZEPHYR},
    }
)
