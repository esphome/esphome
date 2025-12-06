import re

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
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
from esphome.components.libretiny import get_libretiny_component, get_libretiny_family
from esphome.components.libretiny.const import (
    COMPONENT_BK72XX,
    COMPONENT_LN882X,
    COMPONENT_RTL87XX,
)
from esphome.components.zephyr import (
    zephyr_add_cdc_acm,
    zephyr_add_overlay,
    zephyr_add_prj_conf,
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
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
    PlatformFramework,
)
from esphome.core import CORE, CoroPriority, Lambda, coroutine_with_priority

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

UART_SELECTION_ESP32 = {
    VARIANT_ESP32: [UART0, UART1, UART2],
    VARIANT_ESP32C2: [UART0, UART1],
    VARIANT_ESP32C3: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32C5: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32C6: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32C61: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32H2: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32P4: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
    VARIANT_ESP32S2: [UART0, UART1, USB_CDC],
    VARIANT_ESP32S3: [UART0, UART1, USB_CDC, USB_SERIAL_JTAG],
}

UART_SELECTION_ESP8266 = [UART0, UART0_SWAP, UART1]

UART_SELECTION_LIBRETINY = {
    COMPONENT_BK72XX: [DEFAULT, UART1, UART2],
    COMPONENT_LN882X: [DEFAULT, UART0, UART1, UART2],
    COMPONENT_RTL87XX: [DEFAULT, UART0, UART1, UART2],
}

UART_SELECTION_RP2040 = [USB_CDC, UART0, UART1]

UART_SELECTION_NRF52 = [USB_CDC, UART0]

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
    PLATFORM_RP2040: {
        UART0: cg.global_ns.Serial1,
        UART1: cg.global_ns.Serial2,
        USB_CDC: cg.global_ns.Serial,
    },
}

is_log_level = cv.one_of(*LOG_LEVELS, upper=True)


def uart_selection(value):
    if CORE.is_esp32:
        variant = get_esp32_variant()
        if variant in UART_SELECTION_ESP32:
            return cv.one_of(*UART_SELECTION_ESP32[variant], upper=True)(value)
    if CORE.is_esp8266:
        return cv.one_of(*UART_SELECTION_ESP8266, upper=True)(value)
    if CORE.is_rp2040:
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
    raise NotImplementedError


def validate_local_no_higher_than_global(config):
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


def validate_initial_no_higher_than_global(config):
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
            ): cv.All(
                cv.only_on_esp32,
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
                esp32_c3=USB_SERIAL_JTAG,
                esp32_c5=USB_SERIAL_JTAG,
                esp32_c6=USB_SERIAL_JTAG,
                esp32_p4=USB_SERIAL_JTAG,
                esp32_s2=USB_CDC,
                esp32_s3=USB_SERIAL_JTAG,
                rp2040=USB_CDC,
                bk72xx=DEFAULT,
                ln882x=DEFAULT,
                rtl87xx=DEFAULT,
                nrf52=USB_CDC,
            ): cv.All(
                cv.only_on(
                    [
                        PLATFORM_ESP8266,
                        PLATFORM_ESP32,
                        PLATFORM_RP2040,
                        PLATFORM_BK72XX,
                        PLATFORM_LN882X,
                        PLATFORM_RTL87XX,
                        PLATFORM_NRF52,
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
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_local_no_higher_than_global,
    validate_initial_no_higher_than_global,
)


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def to_code(config):
    baud_rate = config[CONF_BAUD_RATE]
    level = config[CONF_LEVEL]
    CORE.data.setdefault(CONF_LOGGER, {})[CONF_LEVEL] = level
    initial_level = LOG_LEVELS[config.get(CONF_INITIAL_LEVEL, level)]
    log = cg.new_Pvariable(
        config[CONF_ID],
        baud_rate,
        config[CONF_TX_BUFFER_SIZE],
    )
    if CORE.is_esp32:
        cg.add(log.create_pthread_key())
        task_log_buffer_size = config[CONF_TASK_LOG_BUFFER_SIZE]
        if task_log_buffer_size > 0:
            cg.add_define("USE_ESPHOME_TASK_LOG_BUFFER")
            cg.add(log.init_log_buffer(task_log_buffer_size))

    cg.add(log.set_log_level(initial_level))
    if CONF_HARDWARE_UART in config:
        cg.add(
            log.set_uart_selection(
                HARDWARE_UART_TO_UART_SELECTION[config[CONF_HARDWARE_UART]]
            )
        )
    cg.add(log.pre_setup())

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

    if (
        (CORE.is_esp8266 or CORE.is_rp2040)
        and has_serial_logging
        and is_at_least_verbose
    ):
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
    try:
        uart_selection(USB_SERIAL_JTAG)
        cg.add_define("USE_LOGGER_USB_SERIAL_JTAG")
    except cv.Invalid:
        pass
    try:
        uart_selection(USB_CDC)
        cg.add_define("USE_LOGGER_USB_CDC")
    except cv.Invalid:
        pass

    if CORE.using_zephyr:
        if config[CONF_HARDWARE_UART] == UART0:
            zephyr_add_overlay("""&uart0 { status = "okay";};""")
        if config[CONF_HARDWARE_UART] == UART1:
            zephyr_add_overlay("""&uart1 { status = "okay";};""")
        if config[CONF_HARDWARE_UART] == USB_CDC:
            zephyr_add_prj_conf("UART_LINE_CTRL", True)
            zephyr_add_cdc_acm(config, 0)

    # Register at end for safe mode
    await cg.register_component(log, config)

    for conf in config.get(CONF_ON_MESSAGE, []):
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


def validate_printf(value):
    # https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
    cfmt = r"""
    (                                  # start of capture group 1
    %                                  # literal "%"
    (?:[-+0 #]{0,5})                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    [cCdiouxXeEfgGaAnpsSZ]             # type
    )
    """  # noqa
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


@automation.register_action(CONF_LOGGER_LOG, LambdaAction, LOGGER_LOG_ACTION_SCHEMA)
async def logger_log_action_to_code(config, action_id, template_arg, args):
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
)
async def logger_set_level_to_code(config, action_id, template_arg, args):
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
        "logger_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
        "logger_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "logger_zephyr.cpp": {PlatformFramework.NRF52_ZEPHYR},
        "task_log_buffer.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
    }
)

# Keys for CORE.data storage
DOMAIN = "logger"
KEY_LEVEL_LISTENERS = "level_listeners"


def request_logger_level_listeners() -> None:
    """Request that logger level listeners be compiled in.

    Components that need to be notified about log level changes should call this
    function during their code generation. This enables the add_level_listener()
    method and compiles in the listener vector.
    """
    CORE.data.setdefault(DOMAIN, {})[KEY_LEVEL_LISTENERS] = True


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step():
    """Final code generation step to configure optional logger features."""
    if CORE.data.get(DOMAIN, {}).get(KEY_LEVEL_LISTENERS, False):
        cg.add_define("USE_LOGGER_LEVEL_LISTENERS")
