from esphome import automation
import esphome.codegen as cg
from esphome.components import display, esp32, uart
import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_ID, CONF_LAMBDA, CONF_ON_TOUCH
from esphome.core import CORE, TimePeriod

from . import (  # noqa: F401  pylint: disable=unused-import
    FILTER_SOURCE_FILES,
    Nextion,
    nextion_ns,
    nextion_ref,
)
from .base_component import (
    CONF_AUTO_WAKE_ON_TOUCH,
    CONF_COMMAND_SPACING,
    CONF_DUMP_DEVICE_INFO,
    CONF_EXIT_REPARSE_ON_START,
    CONF_MAX_COMMANDS_PER_LOOP,
    CONF_MAX_QUEUE_AGE,
    CONF_MAX_QUEUE_SIZE,
    CONF_ON_BUFFER_OVERFLOW,
    CONF_ON_PAGE,
    CONF_ON_SETUP,
    CONF_ON_SLEEP,
    CONF_ON_WAKE,
    CONF_SKIP_CONNECTION_HANDSHAKE,
    CONF_START_UP_PAGE,
    CONF_STARTUP_OVERRIDE_MS,
    CONF_TFT_UPLOAD_HTTP_RETRIES,
    CONF_TFT_UPLOAD_HTTP_TIMEOUT,
    CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT,
    CONF_TFT_URL,
    CONF_TOUCH_SLEEP_TIMEOUT,
    CONF_WAKE_UP_PAGE,
)

CODEOWNERS = ["@senexcrenshaw", "@edwardtfn"]
DEPENDENCIES = ["uart"]


def AUTO_LOAD() -> list[str]:
    base = ["binary_sensor", "switch", "sensor", "text_sensor"]
    if CORE.is_esp32:
        base.append("watchdog")
    return base


NextionSetBrightnessAction = nextion_ns.class_(
    "NextionSetBrightnessAction", automation.Action
)


def _validate_tft_upload(config):
    has_tft_url = CONF_TFT_URL in config
    for conf_key in (
        CONF_TFT_UPLOAD_HTTP_TIMEOUT,
        CONF_TFT_UPLOAD_HTTP_RETRIES,
        CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT,
    ):
        if conf_key in config and not has_tft_url:
            raise cv.Invalid(f"{conf_key} requires {CONF_TFT_URL} to be set")
    if CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT in config and not CORE.is_esp32:
        raise cv.Invalid(
            f"{CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT} is only available on ESP32"
        )
    return config


CONFIG_SCHEMA = cv.All(
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Nextion),
            cv.Optional(CONF_AUTO_WAKE_ON_TOUCH, default=True): cv.boolean,
            cv.Optional(CONF_BRIGHTNESS): cv.percentage,
            cv.Optional(CONF_COMMAND_SPACING): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=TimePeriod(milliseconds=255)),
            ),
            cv.Optional(CONF_DUMP_DEVICE_INFO, default=False): cv.boolean,
            cv.Optional(CONF_EXIT_REPARSE_ON_START, default=False): cv.boolean,
            cv.Optional(CONF_MAX_QUEUE_AGE, default="8000ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=TimePeriod(milliseconds=0), max=TimePeriod(milliseconds=65535)
                ),
            ),
            cv.Optional(CONF_MAX_COMMANDS_PER_LOOP): cv.uint16_t,
            cv.Optional(CONF_MAX_QUEUE_SIZE): cv.positive_int,
            cv.Optional(CONF_ON_BUFFER_OVERFLOW): automation.validate_automation({}),
            cv.Optional(CONF_ON_PAGE): automation.validate_automation({}),
            cv.Optional(CONF_ON_SETUP): automation.validate_automation({}),
            cv.Optional(CONF_ON_SLEEP): automation.validate_automation({}),
            cv.Optional(CONF_ON_TOUCH): automation.validate_automation({}),
            cv.Optional(CONF_ON_WAKE): automation.validate_automation({}),
            cv.Optional(CONF_SKIP_CONNECTION_HANDSHAKE, default=False): cv.boolean,
            cv.Optional(CONF_STARTUP_OVERRIDE_MS, default="8000ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=TimePeriod(milliseconds=0), max=TimePeriod(milliseconds=65535)
                ),
            ),
            cv.Optional(CONF_START_UP_PAGE): cv.uint8_t,
            cv.Optional(CONF_TFT_UPLOAD_HTTP_RETRIES): cv.int_range(min=1, max=255),
            cv.Optional(CONF_TFT_UPLOAD_HTTP_TIMEOUT): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=TimePeriod(milliseconds=65535)),
            ),
            cv.Optional(
                CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TFT_URL): cv.url,
            cv.Optional(CONF_TOUCH_SLEEP_TIMEOUT): cv.Any(
                0, cv.int_range(min=3, max=65535)
            ),
            cv.Optional(CONF_WAKE_UP_PAGE): cv.uint8_t,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_tft_upload,
)


@automation.register_action(
    "display.nextion.set_brightness",
    NextionSetBrightnessAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(Nextion),
            cv.Required(CONF_BRIGHTNESS): cv.templatable(cv.percentage),
        },
        key=CONF_BRIGHTNESS,
    ),
    synchronous=True,
)
async def nextion_set_brightness_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_BRIGHTNESS], args, float)
    cg.add(var.set_brightness(template_))

    return var


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_SETUP, "add_setup_state_callback"),
    automation.CallbackAutomation(CONF_ON_SLEEP, "add_sleep_state_callback"),
    automation.CallbackAutomation(CONF_ON_WAKE, "add_wake_state_callback"),
    automation.CallbackAutomation(
        CONF_ON_PAGE, "add_new_page_callback", [(cg.uint8, "x")]
    ),
    automation.CallbackAutomation(
        CONF_ON_TOUCH,
        "add_touch_event_callback",
        [
            (cg.uint8, "page_id"),
            (cg.uint8, "component_id"),
            (cg.bool_, "touch_event"),
        ],
    ),
    automation.CallbackAutomation(
        CONF_ON_BUFFER_OVERFLOW, "add_buffer_overflow_event_callback"
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await uart.register_uart_device(var, config)

    cg.add(var.set_max_queue_age(config[CONF_MAX_QUEUE_AGE]))

    if max_queue_size := config.get(CONF_MAX_QUEUE_SIZE):
        cg.add_define("USE_NEXTION_MAX_QUEUE_SIZE")
        cg.add(var.set_max_queue_size(max_queue_size))

    if command_spacing := config.get(CONF_COMMAND_SPACING):
        cg.add_define("USE_NEXTION_COMMAND_SPACING")
        cg.add(var.set_command_spacing(command_spacing.total_milliseconds))

    cg.add(var.set_startup_override_ms(config[CONF_STARTUP_OVERRIDE_MS]))

    if CONF_BRIGHTNESS in config:
        cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(nextion_ref, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_TFT_URL in config:
        cg.add_define("USE_NEXTION_TFT_UPLOAD")
        cg.add(var.set_tft_url(config[CONF_TFT_URL]))

        # TFT upload HTTP timeout (default: 4.5s)
        if CONF_TFT_UPLOAD_HTTP_TIMEOUT in config:
            cg.add(
                var.set_tft_upload_http_timeout(
                    config[CONF_TFT_UPLOAD_HTTP_TIMEOUT].total_milliseconds
                )
            )

        # TFT upload HTTP retries (default: 5)
        if CONF_TFT_UPLOAD_HTTP_RETRIES in config:
            cg.add(
                var.set_tft_upload_http_retries(config[CONF_TFT_UPLOAD_HTTP_RETRIES])
            )

        # TFT upload watchdog timeout (default: 0 = no adjustment)
        if CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT in config:
            cg.add(
                var.set_tft_upload_watchdog_timeout(
                    config[CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT].total_milliseconds
                )
            )

        if CORE.is_esp32:
            # Re-enable ESP-IDF's HTTP client (excluded by default to save compile time)
            esp32.include_builtin_idf_component("esp_http_client")
            esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True
            )
        elif CORE.is_esp8266:
            cg.add_library("ESP8266HTTPClient", None)

    if CONF_TOUCH_SLEEP_TIMEOUT in config:
        cg.add(var.set_touch_sleep_timeout(config[CONF_TOUCH_SLEEP_TIMEOUT]))

    if CONF_WAKE_UP_PAGE in config:
        cg.add(var.set_wake_up_page(config[CONF_WAKE_UP_PAGE]))

    if CONF_START_UP_PAGE in config:
        cg.add_define("USE_NEXTION_CONF_START_UP_PAGE")
        cg.add(var.set_start_up_page(config[CONF_START_UP_PAGE]))

    cg.add(var.set_auto_wake_on_touch(config[CONF_AUTO_WAKE_ON_TOUCH]))

    if config[CONF_DUMP_DEVICE_INFO]:
        cg.add_define("USE_NEXTION_CONFIG_DUMP_DEVICE_INFO")

    if config[CONF_EXIT_REPARSE_ON_START]:
        cg.add_define("USE_NEXTION_CONFIG_EXIT_REPARSE_ON_START")

    if config[CONF_SKIP_CONNECTION_HANDSHAKE]:
        cg.add_define("USE_NEXTION_CONFIG_SKIP_CONNECTION_HANDSHAKE")

    if max_commands_per_loop := config.get(CONF_MAX_COMMANDS_PER_LOOP):
        cg.add_define("USE_NEXTION_MAX_COMMANDS_PER_LOOP")
        cg.add(var.set_max_commands_per_loop(max_commands_per_loop))

    await display.register_display(var, config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
