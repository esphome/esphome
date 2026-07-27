import logging
import re
import sys

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
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    get_esp32_variant,
)
from esphome.components.esp32.gpio_esp32_c5 import esp32_c5_validate_lp_i2c
from esphome.components.esp32.gpio_esp32_c6 import esp32_c6_validate_lp_i2c
from esphome.components.esp32.gpio_esp32_p4 import esp32_p4_validate_lp_i2c
from esphome.components.esp32.gpio_esp32_s31 import esp32_s31_validate_lp_i2c
from esphome.components.zephyr import (
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_dts_board_id,
    zephyr_setup_i2c_pinctrl,
    zephyr_variant,
)
from esphome.components.zephyr.const import (
    KEY_BOARD,
    ZEPHYR_VARIANT_NATIVE_SIM,
    ZephyrI2CEmulator,
)
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DEVICE,
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
    PLATFORM_HOST,
    PLATFORM_NRF52,
    PLATFORM_RP2,
    PLATFORM_ZEPHYR,
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
HostI2CBus = i2c_ns.class_("HostI2CBus", I2CBus, cg.Component)
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
    VARIANT_ESP32H4: {"NUM": 2, "HP": 2},
    VARIANT_ESP32H21: {"NUM": 2, "HP": 2},
    VARIANT_ESP32P4: {"NUM": 3, "HP": 2, "LP": 1},
    VARIANT_ESP32S2: {"NUM": 2, "HP": 2},
    VARIANT_ESP32S3: {"NUM": 2, "HP": 2},
    VARIANT_ESP32S31: {"NUM": 3, "HP": 2, "LP": 1},
}
VALIDATE_LP_I2C = {
    VARIANT_ESP32C5: esp32_c5_validate_lp_i2c,
    VARIANT_ESP32C6: esp32_c6_validate_lp_i2c,
    VARIANT_ESP32P4: esp32_p4_validate_lp_i2c,
    VARIANT_ESP32S31: esp32_s31_validate_lp_i2c,
}
LP_I2C_VARIANT = list(VALIDATE_LP_I2C.keys())

CONF_SDA_PULLUP_ENABLED = "sda_pullup_enabled"
CONF_SCL_PULLUP_ENABLED = "scl_pullup_enabled"
CONF_EMULATION = "emulation"
CONF_REGISTERS = "registers"
CONF_DTS_NODE_OVERRIDE = "dts_node_override"
MULTI_CONF = True


def _normalize_register_value(value):
    """Normalize a registers: map value to (entry_len, flat_byte_list).

    - scalar int → one static byte
    - flat list of ints → cycling single-byte reads
    - list of lists of ints → cycling multi-byte reads (all entries same length)
    """
    if isinstance(value, int):
        return 1, [cv.uint8_t(value)]
    if isinstance(value, list) and value:
        if len(value) > 255:
            raise cv.Invalid(
                f"A register sequence supports at most 255 entries, got {len(value)}"
            )
        if isinstance(value[0], list):
            entry_len = len(value[0])
            if entry_len == 0:
                raise cv.Invalid("Register sequence entries must not be empty lists")
            data = []
            for entry in value:
                if not isinstance(entry, list) or len(entry) != entry_len:
                    raise cv.Invalid(
                        "All entries in a register sequence must be lists of the same length"
                    )
                data.extend(cv.uint8_t(b) for b in entry)
            return entry_len, data
        return 1, [cv.uint8_t(b) for b in value]
    raise cv.Invalid(
        "Register value must be an integer, a list of integers, "
        "or a list of lists of integers"
    )


def _registers_schema(value):
    if not isinstance(value, dict):
        raise cv.Invalid("registers must be a mapping of register address to value(s)")
    entries = []
    for reg, val in value.items():
        entry_len, data = _normalize_register_value(val)
        entries.append((cv.uint8_t(reg), entry_len, data))
    return entries


_I2C_EMULATOR_ITEM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZephyrI2CEmulator),
        cv.Required(CONF_ADDRESS): cv.i2c_address,
        cv.Required(CONF_REGISTERS): _registers_schema,
    }
)


def validate_device(value):
    if not re.match(r"^/(?:[^/]+/)*[^/]+$", value):
        raise cv.Invalid("Device must be an absolute device path (e.g., /dev/i2c-0)")
    return value


def _bus_declare_type(value):
    if CORE.is_esp32:
        return cv.declare_id(IDFI2CBus)(value)
    if CORE.using_arduino:
        return cv.declare_id(ArduinoI2CBus)(value)
    if CORE.using_zephyr:
        return cv.declare_id(ZephyrI2CBus)(value)
    if CORE.is_host:
        return cv.declare_id(HostI2CBus)(value)
    raise NotImplementedError


def _rp2040_i2c_controller(pin):
    """Return the I2C controller number (0 or 1) for a given RP2040/RP2350 GPIO pin.

    See RP2040 datasheet Table 2 (section 1.4.3, "GPIO Functions"):
    https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
    See RP2350 datasheet Table 7 (section 9.4, "Function Select"):
    https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf
    """
    return (pin // 2) % 2


def validate_config(config):
    if CORE.is_esp32:
        return cv.require_framework_version(
            esp_idf=cv.Version(5, 4, 2), esp32_arduino=cv.Version(3, 2, 1)
        )(config)
    if CORE.is_rp2:
        sda_controller = _rp2040_i2c_controller(config[CONF_SDA])
        scl_controller = _rp2040_i2c_controller(config[CONF_SCL])
        if sda_controller != scl_controller:
            raise cv.Invalid(
                f"SDA pin GPIO{config[CONF_SDA]} is on I2C{sda_controller} but "
                f"SCL pin GPIO{config[CONF_SCL]} is on I2C{scl_controller}. "
                f"Both pins must be on the same I2C controller."
            )
    return config


def validate_host_config(config):
    if CORE.is_host:
        # Host I2C is currently only supported on Linux
        if not sys.platform.lower().startswith("linux"):
            raise cv.Invalid(
                "I2C is only supported on Linux for the host platform. "
                f"Current platform: {sys.platform}"
            )
        if CONF_SDA in config or CONF_SCL in config:
            raise cv.Invalid(
                "'sda' and 'scl' are not supported on host platform; use 'device' instead."
            )
        if CONF_SDA_PULLUP_ENABLED in config or CONF_SCL_PULLUP_ENABLED in config:
            raise cv.Invalid("Pull-up configuration is not supported on host platform.")
        if CONF_DEVICE not in config:
            raise cv.Invalid(
                "'device' is required for host platform (e.g., /dev/i2c-0)."
            )
    elif CORE.using_zephyr:
        is_native_sim = zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM
        if CONF_DEVICE in config and not is_native_sim:
            raise cv.Invalid(
                "'device' is only supported on the native_sim Zephyr variant."
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _bus_declare_type,
            cv.SplitDefault(
                CONF_SDA,
                esp32="SDA",
                esp8266="SDA",
                rp2="SDA",
                nrf52="SDA",
            ): pins.internal_gpio_pin_number,
            cv.SplitDefault(CONF_SDA_PULLUP_ENABLED, esp32=True): cv.All(
                cv.only_on_esp32, cv.boolean
            ),
            cv.SplitDefault(
                CONF_SCL,
                esp32="SCL",
                esp8266="SCL",
                rp2="SCL",
                nrf52="SCL",
            ): pins.internal_gpio_pin_number,
            cv.SplitDefault(CONF_SCL_PULLUP_ENABLED, esp32=True): cv.All(
                cv.only_on_esp32, cv.boolean
            ),
            cv.SplitDefault(
                CONF_FREQUENCY,
                esp32="50kHz",
                esp8266="50kHz",
                rp2="50kHz",
                nrf52="100kHz",
                host="50kHz",
                zephyr="100kHz",
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
            cv.Optional(CONF_DEVICE): cv.All(
                cv.only_on([PLATFORM_HOST, PLATFORM_ZEPHYR]), validate_device
            ),
            cv.Optional(CONF_DTS_NODE_OVERRIDE): cv.All(
                cv.only_on([PLATFORM_ZEPHYR]), cv.string
            ),
            cv.Optional(CONF_EMULATION): cv.All(
                cv.only_on([PLATFORM_ZEPHYR]),
                cv.ensure_list(_I2C_EMULATOR_ITEM_SCHEMA),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_RP2,
            PLATFORM_NRF52,
            PLATFORM_HOST,
            PLATFORM_ZEPHYR,
        ]
    ),
    validate_config,
    validate_host_config,
)


def _final_validate(config):
    if CONF_DEVICE in config and config.get(CONF_EMULATION):
        LOGGER.warning(
            "'%s: %s' is ignored: 'emulation:' assigns the bus instead of the device",
            CONF_DEVICE,
            config[CONF_DEVICE],
        )

    full_config = fv.full_config.get()[CONF_I2C]
    if CORE.using_zephyr and len(full_config) > 1:
        raise cv.Invalid("Second i2c is not implemented on Zephyr yet")
    if CORE.is_rp2:
        if len(full_config) > 2:
            raise cv.Invalid(
                "The maximum number of I2C interfaces for RP2040/RP2350 is 2"
            )
        if len(full_config) > 1:
            controllers = [
                _rp2040_i2c_controller(conf[CONF_SDA]) for conf in full_config
            ]
            if len(set(controllers)) != len(controllers):
                raise cv.Invalid(
                    "Multiple I2C buses are configured to use the same I2C controller. "
                    "Each bus must use pins on a different controller. "
                    "The I2C controller is determined by (gpio / 2) % 2: "
                    "even pin pairs (0-1, 4-5, 8-9, ...) use I2C0, "
                    "odd pin pairs (2-3, 6-7, 10-11, ...) use I2C1."
                )
    if CORE.is_esp32 and get_esp32_variant() in ESP32_I2C_CAPABILITIES:
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
    if CORE.is_host:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        cg.add(var.set_device(config[CONF_DEVICE]))
        cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
        cg.add(var.set_scan(config[CONF_SCAN]))
    elif CORE.using_zephyr:
        from pathlib import Path

        from esphome.components.zephyr import add_extra_build_file, zephyr_add_overlay
        from esphome.components.zephyr.dts_lookup import resolve_zephyr_bus

        zephyr_add_prj_conf("I2C", True)

        board = zephyr_data()[KEY_BOARD]
        is_native_sim = zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM
        has_emulation = bool(config.get(CONF_EMULATION))
        zephyr_here = Path(__file__).parent.parent / "zephyr"

        if is_native_sim:
            zephyr_add_prj_conf("EMUL", True)
            zephyr_add_prj_conf("I2C_EMUL", True)
            i2c = resolve_zephyr_bus(
                "i2c",
                zephyr_dts_board_id(board),
                override=config.get(CONF_DTS_NODE_OVERRIDE),
            )
            zephyr_add_overlay(f'&{i2c} {{ status = "okay"; }};')
            sda, scl = 0, 0
            if not has_emulation and CONF_DEVICE in config:
                for fname in ("i2c_passthrough_bottom.h", "i2c_passthrough_bottom.cpp"):
                    add_extra_build_file(fname, zephyr_here / fname)
        elif has_emulation:
            i2c = "esphome_i2c_emul"
            zephyr_add_overlay(
                f"/ {{ {i2c}: {i2c} {{"
                f' compatible = "zephyr,i2c-emul-controller";'
                f" #address-cells = <1>; #size-cells = <0>;"
                f" clock-frequency = <{int(config[CONF_FREQUENCY])}>;"
                f' status = "okay"; }}; }};'
            )
            sda, scl = 0, 0
        else:
            if CORE.is_nrf52 and CONF_DTS_NODE_OVERRIDE not in config:
                # nrf52's PlatformIO build never has a dts_base_path, so DTS
                # auto-detection can't succeed -- match dev's original default.
                i2c = "i2c1" if board == "xiao_ble" else "i2c0"
            else:
                i2c = resolve_zephyr_bus(
                    "i2c",
                    zephyr_dts_board_id(board),
                    override=config.get(CONF_DTS_NODE_OVERRIDE),
                )
            zephyr_add_overlay(f'&{i2c} {{ status = "okay"; }};')
            sda, scl = zephyr_setup_i2c_pinctrl(
                board, i2c, config.get(CONF_SDA), config.get(CONF_SCL)
            )
        var = cg.new_Pvariable(
            config[CONF_ID], MockObj(f"DEVICE_DT_GET(DT_NODELABEL({i2c}))")
        )
        await cg.register_component(var, config)

        if is_native_sim and not has_emulation and (device := config.get(CONF_DEVICE)):
            cg.add(var.set_linux_bus(device))

        cg.add(var.set_sda_pin(sda))
        if CONF_SDA_PULLUP_ENABLED in config:
            cg.add(var.set_sda_pullup_enabled(config[CONF_SDA_PULLUP_ENABLED]))
        cg.add(var.set_scl_pin(scl))
        if CONF_SCL_PULLUP_ENABLED in config:
            cg.add(var.set_scl_pullup_enabled(config[CONF_SCL_PULLUP_ENABLED]))

        cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
        cg.add(var.set_scan(config[CONF_SCAN]))
        if CONF_TIMEOUT in config:
            cg.add(var.set_timeout(int(config[CONF_TIMEOUT].total_microseconds)))
        if CONF_LOW_POWER_MODE in config:
            cg.add(var.set_lp_mode(bool(config[CONF_LOW_POWER_MODE])))

        if emulation := config.get(CONF_EMULATION):
            from esphome.core import ID

            zephyr_add_prj_conf("EMUL", True)
            zephyr_add_prj_conf("I2C_EMUL", True)
            for fname in ("i2c_emulator.h", "i2c_emulator.cpp"):
                add_extra_build_file(fname, zephyr_here / fname)
            for item in emulation:
                emul_var = cg.new_Pvariable(
                    item[CONF_ID],
                    var.get_i2c_dev(),
                    item[CONF_ADDRESS],
                    len(item[CONF_REGISTERS]),
                )
                await cg.register_component(emul_var, {})
                for index, (reg, entry_len, data) in enumerate(item[CONF_REGISTERS]):
                    arr_id = ID(
                        f"{item[CONF_ID].id}_reg_{index}",
                        is_declaration=True,
                        type=cg.uint8,
                    )
                    arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
                    cg.add(
                        emul_var.add_register(
                            reg, arr, entry_len, len(data) // entry_len
                        )
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
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.i2c_address
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.i2c_address
    return cv.Schema(schema)


async def register_i2c_device(var, config):
    """Register an i2c device with the given config.

    Sets the i2c bus to use and the i2c address.

    This is a coroutine, you need to await it with an 'await' expression!
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
            PlatformFramework.RP2_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "i2c_bus_esp_idf.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        # Remove NRF52_ZEPHYR when platform: nrf52 deprecation is complete.
        "i2c_bus_zephyr.cpp": {
            PlatformFramework.NRF52_ZEPHYR,
            PlatformFramework.ZEPHYR_ZEPHYR,
        },
        "i2c_bus_host.cpp": {PlatformFramework.HOST_NATIVE},
    }
)
