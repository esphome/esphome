import re
from typing import Any

from esphome import automation
from esphome.automation import LambdaAction, StatelessLambdaAction
import esphome.codegen as cg
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
    add_idf_sdkconfig_option,
    get_esp32_variant,
    require_usb_serial_jtag_secondary,
    require_vfs_termios,
)
from esphome.components.libretiny import get_libretiny_component, get_libretiny_family
from esphome.components.libretiny.const import (
    COMPONENT_BK72XX,
    COMPONENT_LN882X,
    COMPONENT_RTL87XX,
)
from esphome.components.zephyr import (
    KEY_BOARD,
    VARIANTS,
    ZEPHYR_VARIANT_ESP32_C3,
    ZEPHYR_VARIANT_ESP32_C5,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
    zephyr_add_cdc_acm,
    zephyr_add_overlay,
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ARGS,
    CONF_BAUD_RATE,
    CONF_DEASSERT_RTS_DTR,
    CONF_FORMAT,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    CONF_LOGS,
    CONF_ON_MESSAGE,
    CONF_TAG,
    CONF_TRIGGER_ID,
    CONF_TX_BUFFER_SIZE,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_NRF52,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
    PLATFORM_ZEPHYR,
    PlatformFramework,
)
from esphome.core import CORE, ID, CoroPriority, Lambda, coroutine_with_priority
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
logger_ns = cg.esphome_ns.namespace("logger")
LOG_LEVELS = {
    "NONE": cg.global_ns.ESPHOME_LOG_LEVEL_NONE,
    "ERROR": cg.global_ns.ESPHOME_LOG_LEVEL_ERROR,
    "WARN": cg.global_ns.ESPHOME_LOG_LEVEL_WARN,
    "INFO": cg.global_ns.ESPHOME_LOG_LEVEL_INFO,
    "DEBUG": cg.global_ns.ESPHOME_LOG_LEVEL_DEBUG,
    "VERBOSE": cg.global_ns.ESPHOME_LOG_LEVEL_VERBOSE,
    "VERY_VERBOSE": cg.global_ns.ESPHOME_LOG_LEVEL_VERY_VERBOSE,
}

LOG_LEVEL_TO_ESP_LOG = {
    "ERROR": cg.global_ns.ESP_LOGE,
    "WARN": cg.global_ns.ESP_LOGW,
    "INFO": cg.global_ns.ESP_LOGI,
    "DEBUG": cg.global_ns.ESP_LOGD,
    "VERBOSE": cg.global_ns.ESP_LOGV,
    "VERY_VERBOSE": cg.global_ns.ESP_LOGVV,
}

LOG_LEVEL_SEVERITY = [
    "NONE",
    "ERROR",
    "WARN",
    "INFO",
    "CONFIG",
    "DEBUG",
    "VERBOSE",
    "VERY_VERBOSE",
]

UART0 = "UART0"
UART1 = "UART1"
UART2 = "UART2"
UART0_SWAP = "UART0_SWAP"
USB_SERIAL_JTAG = "USB_SERIAL_JTAG"
USB_CDC = "USB_CDC"
DEFAULT = "DEFAULT"

CONF_INITIAL_LEVEL = "initial_level"
CONF_LOGGER_ID = "logger_id"
CONF_RUNTIME_TAG_LEVELS = "runtime_tag_levels"
CONF_TASK_LOG_BUFFER_SIZE = "task_log_buffer_size"
CONF_WAIT_FOR_CDC = "wait_for_cdc"
CONF_EARLY_MESSAGE = "early_message"

UART_SELECTION_ESP32 = {
    VARIANT_ESP32: [UART0, UART1, UART2],
    VARIANT_ESP32C2: [UART0, UART1],
    VARIANT_ESP32C3: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32C5: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32C6: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32C61: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32H2: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32H4: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32H21: [UART0, UART1, USB_SERIAL_JTAG],
    VARIANT_ESP32P4: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32S2: [UART0, UART1, USB_CDC],
    VARIANT_ESP32S3: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32S31: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
}

UART_SELECTION_ESP8266 = [UART0, UART0_SWAP, UART1]

UART_SELECTION_LIBRETINY = {
    COMPONENT_BK72XX: [DEFAULT, UART1, UART2],
    COMPONENT_LN882X: [DEFAULT, UART0, UART1, UART2],
    COMPONENT_RTL87XX: [DEFAULT, UART0, UART1, UART2],
}

UART_SELECTION_RP2040 = [USB_CDC, UART0, UART1]

UART_SELECTION_NRF52 = [USB_CDC, UART0]

UART_SELECTION_HOST_ZEPHYR = [UART0, UART1, UART2]
# esp32_h2 and esp32_c6 both expose a native USB-Serial/JTAG peripheral as a Zephyr UART
# device (see the USB_SERIAL_JTAG codegen branch below) -- shared list for both.
UART_SELECTION_ZEPHYR_ESP32_JTAG = [UART0, UART1, USB_SERIAL_JTAG]
# nRF52840 and RP2040 both have native USB (same &usbd/zephyr_udc0 peripheral
# MCUboot's own serial recovery uses) -- see the USB_CDC codegen branch below.
UART_SELECTION_ZEPHYR_USB_CDC = [UART0, UART1, USB_CDC]

HARDWARE_UART_TO_UART_SELECTION = {
    UART0: logger_ns.UART_SELECTION_UART0,
    UART0_SWAP: logger_ns.UART_SELECTION_UART0_SWAP,
    UART1: logger_ns.UART_SELECTION_UART1,
    UART2: logger_ns.UART_SELECTION_UART2,
    USB_CDC: logger_ns.UART_SELECTION_USB_CDC,
    USB_SERIAL_JTAG: logger_ns.UART_SELECTION_USB_SERIAL_JTAG,
    DEFAULT: logger_ns.UART_SELECTION_DEFAULT,
}

HARDWARE_UART_TO_SERIAL = {
    PLATFORM_ESP8266: {
        UART0: cg.global_ns.Serial,
        UART0_SWAP: cg.global_ns.Serial,
        UART1: cg.global_ns.Serial1,
        UART2: cg.global_ns.Serial2,
        DEFAULT: cg.global_ns.Serial,
    },
    PLATFORM_RP2: {
        UART0: cg.global_ns.Serial1,
        UART1: cg.global_ns.Serial2,
        USB_CDC: cg.global_ns.Serial,
    },
}

is_log_level = cv.one_of(*LOG_LEVELS, upper=True)


def uart_selection(value: Any) -> str:
    if CORE.is_esp32:
        variant = get_esp32_variant()
        if variant in UART_SELECTION_ESP32:
            return cv.one_of(*UART_SELECTION_ESP32[variant], upper=True)(value)
    if CORE.is_esp8266:
        return cv.one_of(*UART_SELECTION_ESP8266, upper=True)(value)
    if CORE.is_rp2:
        return cv.one_of(*UART_SELECTION_RP2040, upper=True)(value)
    if CORE.is_libretiny:
        family = get_libretiny_family()
        if family in UART_SELECTION_LIBRETINY:
            return cv.one_of(*UART_SELECTION_LIBRETINY[family], upper=True)(value)
        component = get_libretiny_component()
        if component in UART_SELECTION_LIBRETINY:
            return cv.one_of(*UART_SELECTION_LIBRETINY[component], upper=True)(value)
    if CORE.is_host:
        raise cv.Invalid("Uart selection not valid for host platform")
    if CORE.is_nrf52:
        return cv.one_of(*UART_SELECTION_NRF52, upper=True)(value)
    if CORE.is_zephyr:
        from esphome.components.zephyr.dts_lookup import validate_uart_label_override

        if (
            isinstance(value, str)
            and (override := validate_uart_label_override(value)) is not None
        ):
            return override
        if zephyr_variant() in (
            ZEPHYR_VARIANT_ESP32_H2,
            ZEPHYR_VARIANT_ESP32_C6,
            ZEPHYR_VARIANT_ESP32_C5,
            ZEPHYR_VARIANT_ESP32_C3,
        ):
            return cv.one_of(*UART_SELECTION_ZEPHYR_ESP32_JTAG, upper=True)(value)
        family = zephyr_variant_family()
        if family in {"nordic", "rpi_pico", "renesas", "stm32"}:
            return cv.one_of(*UART_SELECTION_ZEPHYR_USB_CDC, upper=True)(value)
        return cv.one_of(*UART_SELECTION_HOST_ZEPHYR, upper=True)(value)
    raise NotImplementedError


def validate_local_no_higher_than_global(config: ConfigType) -> ConfigType:
    global_level = config[CONF_LEVEL]
    global_level_index = LOG_LEVEL_SEVERITY.index(global_level)
    errs = []
    for tag, level in config.get(CONF_LOGS, {}).items():
        if LOG_LEVEL_SEVERITY.index(level) > global_level_index:
            errs.append(
                cv.Invalid(
                    f"The configured log level for {tag} ({level}) must not be less severe than the global log level ({global_level})",
                    [CONF_LOGS, tag],
                )
            )
    if errs:
        raise cv.MultipleInvalid(errs)
    return config


def validate_initial_no_higher_than_global(config: ConfigType) -> ConfigType:
    if initial_level := config.get(CONF_INITIAL_LEVEL):
        global_level = config[CONF_LEVEL]
        if LOG_LEVEL_SEVERITY.index(initial_level) > LOG_LEVEL_SEVERITY.index(
            global_level
        ):
            raise cv.Invalid(
                f"The initial log level ({initial_level}) must not be less severe than the global log level ({global_level})",
                [CONF_INITIAL_LEVEL],
            )
    return config


def validate_wait_for_cdc(config: ConfigType) -> ConfigType:
    if config.get(CONF_WAIT_FOR_CDC) and config.get(CONF_HARDWARE_UART) != USB_CDC:
        raise cv.Invalid("wait_for_cdc requires hardware_uart: USB_CDC")
    return config


def _only_with_usb_cdc_uart(value):
    if value:
        try:
            uart_selection(USB_CDC)
        except cv.Invalid:
            raise cv.Invalid(
                "This option requires a platform that supports USB_CDC hardware_uart"
            ) from None
    return value


def _only_with_zephyr(value):
    if CORE.using_zephyr:
        return value
    raise cv.Invalid("This option is only available on Zephyr-based platforms")


Logger = logger_ns.class_("Logger", cg.Component)
LoggerMessageTrigger = logger_ns.class_(
    "LoggerMessageTrigger",
    automation.Trigger.template(cg.uint8, cg.const_char_ptr, cg.const_char_ptr),
)


CONF_ESP8266_STORE_LOG_STRINGS_IN_FLASH = "esp8266_store_log_strings_in_flash"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Logger),
            cv.Optional(CONF_BAUD_RATE, default=115200): cv.positive_int,
            cv.Optional(CONF_TX_BUFFER_SIZE, default=512): cv.All(
                cv.validate_bytes, cv.int_range(min=160, max=65535)
            ),
            cv.Optional(CONF_DEASSERT_RTS_DTR, default=False): cv.boolean,
            cv.SplitDefault(
                CONF_TASK_LOG_BUFFER_SIZE,
                esp32=768,  # Default: 768 bytes (~5-6 messages with 70-byte text plus thread names)
                bk72xx=768,
                ln882x=768,
                rtl87xx=768,
                nrf52=768,
                zephyr=768,
            ): cv.All(
                cv.only_on(
                    [
                        PLATFORM_ESP32,
                        PLATFORM_BK72XX,
                        PLATFORM_LN882X,
                        PLATFORM_RTL87XX,
                        PLATFORM_NRF52,
                        PLATFORM_ZEPHYR,
                    ]
                ),
                cv.validate_bytes,
                cv.Any(
                    cv.int_(0),  # Disabled
                    cv.int_range(
                        min=640,  # Min: ~4-5 messages with 70-byte text plus thread names
                        max=32768,  # Max: Depends on message sizes, typically ~300 messages with default size
                    ),
                ),
            ),
            cv.SplitDefault(
                CONF_HARDWARE_UART,
                esp8266=UART0,
                esp32=UART0,
                esp32_c2=UART0,
                esp32_c3=USB_SERIAL_JTAG,
                esp32_c5=USB_SERIAL_JTAG,
                esp32_c6=USB_SERIAL_JTAG,
                esp32_c61=USB_SERIAL_JTAG,
                esp32_h2=USB_SERIAL_JTAG,
                esp32_h4=USB_SERIAL_JTAG,
                esp32_h21=USB_SERIAL_JTAG,
                esp32_p4=USB_SERIAL_JTAG,
                esp32_s2=USB_CDC,
                esp32_s3=USB_SERIAL_JTAG,
                esp32_s31=USB_SERIAL_JTAG,
                rp2=USB_CDC,
                bk72xx=DEFAULT,
                ln882x=DEFAULT,
                rtl87xx=DEFAULT,
                nrf52=USB_CDC,
                zephyr=UART0,
                zephyr_esp32h2=USB_SERIAL_JTAG,
                zephyr_esp32c6=USB_SERIAL_JTAG,
                zephyr_esp32c5=USB_SERIAL_JTAG,
                zephyr_esp32c3=USB_SERIAL_JTAG,
                zephyr_nrf52=USB_CDC,
                zephyr_rp2040=USB_CDC,
                zephyr_rp2350=USB_CDC,
            ): cv.All(
                cv.only_on(
                    [
                        PLATFORM_ESP8266,
                        PLATFORM_ESP32,
                        PLATFORM_RP2,
                        PLATFORM_BK72XX,
                        PLATFORM_LN882X,
                        PLATFORM_RTL87XX,
                        PLATFORM_NRF52,
                        PLATFORM_ZEPHYR,
                    ]
                ),
                uart_selection,
            ),
            cv.Optional(CONF_LEVEL, default="DEBUG"): is_log_level,
            cv.Optional(CONF_LOGS, default={}): cv.Schema(
                {
                    cv.string: is_log_level,
                }
            ),
            cv.Optional(CONF_INITIAL_LEVEL): is_log_level,
            cv.Optional(CONF_RUNTIME_TAG_LEVELS, default=False): cv.boolean,
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LoggerMessageTrigger),
                    cv.Optional(CONF_LEVEL, default="WARN"): is_log_level,
                }
            ),
            cv.SplitDefault(
                CONF_ESP8266_STORE_LOG_STRINGS_IN_FLASH, esp8266=True
            ): cv.All(cv.only_on_esp8266, cv.boolean),
            cv.SplitDefault(CONF_WAIT_FOR_CDC, nrf52=False, zephyr=False): cv.All(
                _only_with_usb_cdc_uart,
                cv.boolean,
            ),
            cv.SplitDefault(CONF_EARLY_MESSAGE, nrf52=False, zephyr=False): cv.All(
                _only_with_zephyr, cv.boolean
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_local_no_higher_than_global,
    validate_initial_no_higher_than_global,
    validate_wait_for_cdc,
)


@coroutine_with_priority(CoroPriority.EARLY_INIT)
async def to_code(config: ConfigType) -> None:
    baud_rate: int = config[CONF_BAUD_RATE]
    level = config[CONF_LEVEL]
    CORE.data.setdefault(CONF_LOGGER, {})[CONF_LEVEL] = level
    tx_buffer_size = config[CONF_TX_BUFFER_SIZE]
    cg.add_define("ESPHOME_LOGGER_TX_BUFFER_SIZE", tx_buffer_size)
    # Determine task log buffer size. The buffer is a direct member of Logger
    # (no separate heap allocation).
    task_log_buffer_size = 0
    if CORE.is_esp32 or CORE.is_libretiny or CORE.using_zephyr:
        task_log_buffer_size = config[CONF_TASK_LOG_BUFFER_SIZE]
    elif CORE.is_host:
        task_log_buffer_size = 64  # Fixed 64 slots for host
    if task_log_buffer_size > 0:
        cg.add_define("USE_ESPHOME_TASK_LOG_BUFFER")
        cg.add_define("ESPHOME_TASK_LOG_BUFFER_SIZE", task_log_buffer_size)
    log = cg.new_Pvariable(
        config[CONF_ID],
        baud_rate,
    )
    if CORE.is_esp32 or CORE.is_host:
        cg.add(log.create_pthread_key())
    # set_uart_selection() must be called before pre_setup() because
    # pre_setup() switches on uart_ to decide which hardware to initialize
    # (e.g. UART0 vs USB_SERIAL_JTAG). Without this, uart_ is still the
    # default UART_SELECTION_UART0 and the wrong hardware gets initialized.
    if CONF_HARDWARE_UART in config:
        hw_uart = config[CONF_HARDWARE_UART]
        # An explicit "&<label>" override is always a plain hardware UART -- on Zephyr,
        # UART_SELECTION_UART0/UART1 both resolve identically via LOGGER_UART_NODE_LABEL
        # (see logger_zephyr.cpp), so either works as the runtime discriminator.
        selected_uart = (
            logger_ns.UART_SELECTION_UART0
            if hw_uart.startswith("&")
            else HARDWARE_UART_TO_UART_SELECTION[hw_uart]
        )
        cg.add(log.set_uart_selection(selected_uart))
    # pre_setup() sets global_logger and must run before any other code
    # that may call ESP_LOG* (e.g. setup_preferences contains ESP_LOGVV).
    cg.add(log.pre_setup())
    initial_level = LOG_LEVELS[config.get(CONF_INITIAL_LEVEL, level)]
    cg.add(log.set_log_level(initial_level))

    # Schedule the rest of logger setup at DIAGNOSTICS priority, after
    # Application is constructed (CORE priority) but before most components.
    CORE.add_job(_late_logger_init, config)


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def _late_logger_init(config: ConfigType) -> None:
    """Finish logger setup after Application is constructed."""
    log = await cg.get_variable(config[CONF_ID])
    level = config[CONF_LEVEL]
    baud_rate: int = config[CONF_BAUD_RATE]
    if CORE.using_zephyr:
        task_log_buffer_size = config.get(CONF_TASK_LOG_BUFFER_SIZE, 0)
        if task_log_buffer_size > 0:
            zephyr_add_prj_conf("MPSC_PBUF", True)

    # Enable runtime tag levels if logs are configured or explicitly enabled
    logs_config = config[CONF_LOGS]
    if logs_config or config[CONF_RUNTIME_TAG_LEVELS]:
        cg.add_define("USE_LOGGER_RUNTIME_TAG_LEVELS")
        for tag, log_level in logs_config.items():
            cg.add(log.set_log_level(tag, LOG_LEVELS[log_level]))

    cg.add_define("USE_LOGGER")
    this_severity = LOG_LEVEL_SEVERITY.index(level)
    cg.add_build_flag(f"-DESPHOME_LOG_LEVEL={LOG_LEVELS[level]}")

    verbose_severity = LOG_LEVEL_SEVERITY.index("VERBOSE")
    very_verbose_severity = LOG_LEVEL_SEVERITY.index("VERY_VERBOSE")
    is_at_least_verbose = this_severity >= verbose_severity
    is_at_least_very_verbose = this_severity >= very_verbose_severity
    has_serial_logging = baud_rate != 0

    # Add defines for which Serial object is needed (allows linker to exclude unused)
    if CORE.is_esp8266:
        from esphome.components.esp8266.const import enable_serial, enable_serial1

        hw_uart = config.get(CONF_HARDWARE_UART, UART0)
        if not has_serial_logging:
            # No serial logging: stub out ROM ets_putc so stray output (newlib
            # stdout, lwIP diagnostics) cannot block on a slow or shared UART0.
            # ets_putc always writes to the physical UART and cannot be disabled
            # through uart_set_debug(); see __wrap_ets_putc in logger_esp8266.cpp.
            cg.add_build_flag("-Wl,--wrap=ets_putc")
        elif hw_uart in (UART0, UART0_SWAP):
            cg.add_define("USE_ESP8266_LOGGER_SERIAL")
            enable_serial()
        elif hw_uart == UART1:
            cg.add_define("USE_ESP8266_LOGGER_SERIAL1")
            enable_serial1()

    if (CORE.is_esp8266 or CORE.is_rp2) and has_serial_logging and is_at_least_verbose:
        debug_serial_port = HARDWARE_UART_TO_SERIAL[CORE.target_platform][
            config.get(CONF_HARDWARE_UART)
        ]
        cg.add_build_flag(f"-DDEBUG_ESP_PORT={debug_serial_port}")
        cg.add_build_flag("-DLWIP_DEBUG")
        DEBUG_COMPONENTS = {
            "HTTP_CLIENT",
            "HTTP_SERVER",
            "HTTP_UPDATE",
            "OTA",
            "SSL",
            "TLS_MEM",
            "UPDATER",
            "WIFI",
            # Spams logs too much:
            # 'MDNS_RESPONDER',
        }
        for comp in DEBUG_COMPONENTS:
            cg.add_build_flag(f"-DDEBUG_ESP_{comp}")
    if CORE.is_esp32 and is_at_least_verbose:
        cg.add_build_flag("-DCORE_DEBUG_LEVEL=5")
    if CORE.is_esp32 and is_at_least_very_verbose:
        cg.add_build_flag("-DENABLE_I2C_DEBUG_BUFFER")
    if config.get(CONF_ESP8266_STORE_LOG_STRINGS_IN_FLASH):
        cg.add_build_flag("-DUSE_STORE_LOG_STR_IN_FLASH")

    if CORE.is_esp32:
        if config[CONF_HARDWARE_UART] == USB_CDC:
            add_idf_sdkconfig_option("CONFIG_ESP_CONSOLE_USB_CDC", True)
            cg.add_define("USE_LOGGER_UART_SELECTION_USB_CDC")
        elif config[CONF_HARDWARE_UART] == USB_SERIAL_JTAG:
            add_idf_sdkconfig_option("CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG", True)
            cg.add_define("USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG")
    # Define platform support flags for components that need auto-detection
    try:
        uart_selection(USB_SERIAL_JTAG)
        cg.add_define("USE_LOGGER_USB_SERIAL_JTAG")
        # USB Serial JTAG code is compiled when platform supports it.
        # Enable secondary USB serial JTAG console so the VFS functions are available.
        if (
            CORE.is_esp32
            and config[CONF_HARDWARE_UART] != USB_SERIAL_JTAG
            and has_serial_logging
        ):
            require_usb_serial_jtag_secondary()
            require_vfs_termios()
    except cv.Invalid:
        pass
    try:
        uart_selection(USB_CDC)
        cg.add_define("USE_LOGGER_USB_CDC")
    except cv.Invalid:
        pass

    if config.get(CONF_WAIT_FOR_CDC):
        cg.add_define("USE_LOGGER_WAIT_FOR_CDC")
    if config.get(CONF_EARLY_MESSAGE):
        cg.add_define("USE_LOGGER_EARLY_MESSAGE")

    if CORE.is_nrf52:
        # Nordic NCS-only Kconfig, absent from mainline Zephyr -- a hard error there,
        # not a no-op, so this must stay nrf52-only.
        zephyr_add_prj_conf("RESET_ON_FATAL_ERROR", False)
        zephyr_add_prj_conf("THREAD_LOCAL_STORAGE", True)
        if has_serial_logging:
            if config[CONF_HARDWARE_UART] == UART0:
                zephyr_add_overlay("""&uart0 { status = "okay";};""")
            if config[CONF_HARDWARE_UART] == UART1:
                zephyr_add_overlay("""&uart1 { status = "okay";};""")
            if config[CONF_HARDWARE_UART] == USB_CDC:
                cg.add_define("USE_LOGGER_UART_SELECTION_USB_CDC")
                zephyr_add_prj_conf("UART_LINE_CTRL", True)
                zephyr_add_cdc_acm(config, 0)
    if CORE.is_zephyr and has_serial_logging:
        zephyr_add_prj_conf("SERIAL", True)
        hw_uart = config.get(CONF_HARDWARE_UART, UART0)
        # Board defaults set zephyr,console to the variant's default UART node regardless
        # of hardware_uart; Zephyr's native LOG subsystem always attaches there, so leaving
        # it at the default would silently lose native log output whenever the user picks
        # a different UART. Node label varies by variant -- e.g. nRF54 numbers peripheral
        # instances (uart20/uart30) instead of nRF52/ESP32's uart0/uart1 -- and some
        # variants (e.g. stm32l4) declare no portable mapping at all, resolved per board
        # from DTS instead. See resolve_uart_node_label()'s own docstring.
        if hw_uart.startswith("&") or hw_uart in (UART0, UART1, UART2):
            if hw_uart.startswith("&"):
                from esphome.components.zephyr.dts_lookup import (
                    validate_dts_label_exists,
                )

                node = hw_uart[1:]
                validate_dts_label_exists("uart", zephyr_data()[KEY_BOARD], node)
            else:
                from esphome.components.zephyr.dts_lookup import resolve_uart_node_label

                node = resolve_uart_node_label(
                    zephyr_data()[KEY_BOARD],
                    hw_uart,
                    VARIANTS[zephyr_variant()].uart_node_labels,
                )
            zephyr_add_overlay(f"""&{node} {{ status = "okay";}};""")
            zephyr_add_overlay(
                f"""/ {{ chosen {{ zephyr,console = &{node}; zephyr,shell-uart = &{node}; }}; }};"""
            )
            # logger_zephyr.cpp's DEVICE_DT_GET_OR_NULL(DT_NODELABEL(...)) needs the
            # actual node label as a bare token at compile time -- variants that number
            # peripheral instances instead of the uart0/uart1 convention (e.g. nRF54's
            # uart20/uart30) would otherwise resolve to a nonexistent "uart0"/"uart1"
            # node, silently leaving uart_dev_ null and dropping every log line.
            cg.add_define("LOGGER_UART_NODE_LABEL", cg.RawExpression(node))
        elif hw_uart == USB_SERIAL_JTAG:
            # A standard Zephyr UART device, not a USB CDC-ACM stack like nrf52's
            # USB_CDC option -- CONFIG_SERIAL_ESP32_USB auto-selects once the DTS
            # node is enabled, no extra prj.conf needed.
            zephyr_add_overlay("""&usb_serial { status = "okay";};""")
            zephyr_add_overlay(
                """/ { chosen { zephyr,console = &usb_serial; zephyr,shell-uart = &usb_serial; }; };"""
            )
        elif hw_uart == USB_CDC:
            # Same generic CDC-ACM helper MCUboot's own serial recovery uses on this
            # hardware -- nRF52840's native USB, not a standard Zephyr UART device
            # like esp32_h2/c6's USB_SERIAL_JTAG above.
            cg.add_define("USE_LOGGER_UART_SELECTION_USB_CDC")
            zephyr_add_prj_conf("UART_LINE_CTRL", True)
            cdc_label = zephyr_add_cdc_acm(config, 0)
            # logger_zephyr.cpp's DEVICE_DT_GET_OR_NULL(DT_NODELABEL(...)) needs the
            # actual node label as a bare token at compile time -- it can't be reused
            # from the `chosen` overlay below, since that's a devicetree property, not
            # something the C++ side reads. RawExpression avoids add_define() quoting
            # this into a string literal, which DT_NODELABEL() can't accept.
            cg.add_define("LOGGER_CDC_ACM_UART_LABEL", cg.RawExpression(cdc_label))
            zephyr_add_overlay(
                f"""/ {{ chosen {{ zephyr,console = &{cdc_label}; """
                f"""zephyr,shell-uart = &{cdc_label}; }}; }};"""
            )

        # Zephyr's native logging defaults to its own LOG_BACKEND_UART, a second writer
        # that would contend with Logger's own write_msg_() for the same UART. Disable
        # it -- logger_zephyr_log_backend.cpp forwards native logs through Logger instead.
        zephyr_add_prj_conf("LOG_BACKEND_UART", False, required=False)

        # LOG_PRINTK redirects printk() through the LOG subsystem, which write_msg_()'s own
        # printk() call (for RTT/pyocd) would then re-enter -- an infinite feedback loop.
        zephyr_add_prj_conf("LOG_PRINTK", False, required=False)

        # logger_zephyr_log_backend.cpp reassembles a log line across chunks using a
        # single shared buffer -- unsafe if calls from different threads/ISRs interleave.
        # This Kconfig serializes calls into backend process() under LOG_MODE_IMMEDIATE
        # (native_sim's default); a no-op on variants using LOG_MODE_DEFERRED (h2, c6).
        zephyr_add_prj_conf("LOG_IMMEDIATE_CLEAN_OUTPUT", True, required=False)

    # Register at end for safe mode
    await cg.register_component(log, config)

    for conf in config.get(CONF_ON_MESSAGE, []):
        request_log_listener()  # Each on_message trigger needs a listener slot
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], log, LOG_LEVEL_SEVERITY.index(conf[CONF_LEVEL])
        )
        await automation.build_automation(
            trigger,
            [
                (cg.uint8, "level"),
                (cg.const_char_ptr, "tag"),
                (cg.const_char_ptr, "message"),
            ],
            conf,
        )

    CORE.add_job(final_step)


def validate_printf(value: ConfigType) -> ConfigType:
    # https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
    cfmt = r"""
    (                                   # start of capture group 1
    %                                   # literal "%"
    (?:[-+0 #]{0,5})                    # optional flags
    (?:\d+|\*)?                         # width
    (?:\.(?:\d+|\*))?                   # precision
    (?:hh|h|ll|l|j|z|t|L|w|I|I32|I64)?  # size
    [cCdiouxXeEfgGaAnpsSZ]              # type
    )
    """
    matches = re.findall(cfmt, value[CONF_FORMAT], flags=re.VERBOSE)
    if len(matches) != len(value[CONF_ARGS]):
        raise cv.Invalid(
            f"Found {len(matches)} printf-patterns ({', '.join(matches)}), but {len(value[CONF_ARGS])} args were given!"
        )
    return value


CONF_LOGGER_LOG = "logger.log"
LOGGER_LOG_ACTION_SCHEMA = cv.All(
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_LOGGER_ID): cv.use_id(Logger),
            cv.Required(CONF_FORMAT): cv.string,
            cv.Optional(CONF_ARGS, default=list): cv.ensure_list(cv.lambda_),
            cv.Optional(CONF_LEVEL, default="DEBUG"): cv.one_of(
                *LOG_LEVEL_TO_ESP_LOG, upper=True
            ),
            cv.Optional(CONF_TAG, default="main"): cv.string,
        },
        validate_printf,
        key=CONF_FORMAT,
    )
)


@automation.register_action(
    CONF_LOGGER_LOG, LambdaAction, LOGGER_LOG_ACTION_SCHEMA, synchronous=True
)
async def logger_log_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    esp_log = LOG_LEVEL_TO_ESP_LOG[config[CONF_LEVEL]]
    args_ = [cg.RawExpression(str(x)) for x in config[CONF_ARGS]]

    text = str(cg.statement(esp_log(config[CONF_TAG], config[CONF_FORMAT], *args_)))

    lambda_ = await cg.process_lambda(Lambda(text), args, return_type=cg.void)
    return automation.new_lambda_pvariable(
        action_id, lambda_, StatelessLambdaAction, template_arg
    )


@automation.register_action(
    "logger.set_level",
    LambdaAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_LOGGER_ID): cv.use_id(Logger),
            cv.Required(CONF_LEVEL): is_log_level,
            cv.Optional(CONF_TAG): cv.string,
        },
        key=CONF_LEVEL,
    ),
    synchronous=True,
)
async def logger_set_level_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    level = LOG_LEVELS[config[CONF_LEVEL]]
    logger = await cg.get_variable(config[CONF_LOGGER_ID])
    if tag := config.get(CONF_TAG):
        cg.add_define("USE_LOGGER_RUNTIME_TAG_LEVELS")
        text = str(cg.statement(logger.set_log_level(tag, level)))
    else:
        text = str(cg.statement(logger.set_log_level(level)))

    lambda_ = await cg.process_lambda(Lambda(text), args, return_type=cg.void)
    return automation.new_lambda_pvariable(
        action_id, lambda_, StatelessLambdaAction, template_arg
    )


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "logger_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "logger_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "logger_host.cpp": {PlatformFramework.HOST_NATIVE},
        "logger_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
        "logger_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        # Remove NRF52_ZEPHYR when platform: nrf52 deprecation is complete.
        "logger_zephyr.cpp": {
            PlatformFramework.NRF52_ZEPHYR,
            PlatformFramework.ZEPHYR_ZEPHYR,
        },
        "task_log_buffer_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "task_log_buffer_host.cpp": {PlatformFramework.HOST_NATIVE},
        "task_log_buffer_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        # Remove NRF52_ZEPHYR when platform: nrf52 deprecation is complete.
        "task_log_buffer_zephyr.cpp": {
            PlatformFramework.NRF52_ZEPHYR,
            PlatformFramework.ZEPHYR_ZEPHYR,
        },
    }
)

# Keys for CORE.data storage
DOMAIN = "logger"
KEY_LEVEL_LISTENERS = "level_listeners"
KEY_LOG_LISTENERS = "log_listeners"


def request_logger_level_listeners() -> None:
    """Request that logger level listeners be compiled in.

    Components that need to be notified about log level changes should call this
    function during their code generation. This enables the add_level_listener()
    method and compiles in the listener vector.
    """
    CORE.data.setdefault(DOMAIN, {})[KEY_LEVEL_LISTENERS] = True


def request_log_listener() -> None:
    """Request a log listener slot.

    Components that need to receive log messages should call this function
    during their code generation. This increments the listener count used
    to size the StaticVector.
    """
    data = CORE.data.setdefault(DOMAIN, {})
    data[KEY_LOG_LISTENERS] = data.get(KEY_LOG_LISTENERS, 0) + 1


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step() -> None:
    """Final code generation step to configure optional logger features."""
    domain_data = CORE.data.get(DOMAIN, {})
    if domain_data.get(KEY_LEVEL_LISTENERS, False):
        cg.add_define("USE_LOGGER_LEVEL_LISTENERS")

    # Only generate log listener code if any component needs it
    log_listener_count = domain_data.get(KEY_LOG_LISTENERS, 0)
    if log_listener_count > 0:
        cg.add_define("USE_LOG_LISTENERS")
        cg.add_define("ESPHOME_LOG_MAX_LISTENERS", log_listener_count)
