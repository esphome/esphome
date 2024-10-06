import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import display, uart
from esphome.components import esp32
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_BRIGHTNESS,
    CONF_TRIGGER_ID,
    CONF_ON_TOUCH,
)
from esphome.core import CORE
from . import Nextion, nextion_ns, nextion_ref
from .base_component import (
    CONF_ON_SLEEP,
    CONF_ON_WAKE,
    CONF_ON_SETUP,
    CONF_ON_PAGE,
    CONF_TFT_URL,
    CONF_TOUCH_SLEEP_TIMEOUT,
    CONF_WAKE_UP_PAGE,
    CONF_START_UP_PAGE,
    CONF_AUTO_WAKE_ON_TOUCH,
    CONF_EXIT_REPARSE_ON_START,
    CONF_SKIP_CONNECTION_HANDSHAKE,
)

CODEOWNERS = ["@senexcrenshaw", "@edwardtfn"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "switch", "sensor", "text_sensor"]

SetupTrigger = nextion_ns.class_("SetupTrigger", automation.Trigger.template())
SleepTrigger = nextion_ns.class_("SleepTrigger", automation.Trigger.template())
WakeTrigger = nextion_ns.class_("WakeTrigger", automation.Trigger.template())
PageTrigger = nextion_ns.class_("PageTrigger", automation.Trigger.template())
TouchTrigger = nextion_ns.class_("TouchTrigger", automation.Trigger.template())

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Nextion),
            cv.Optional(CONF_TFT_URL): cv.url,
            cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
            cv.Optional(CONF_ON_SETUP): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SetupTrigger),
                }
            ),
            cv.Optional(CONF_ON_SLEEP): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SleepTrigger),
                }
            ),
            cv.Optional(CONF_ON_WAKE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WakeTrigger),
                }
            ),
            cv.Optional(CONF_ON_PAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PageTrigger),
                }
            ),
            cv.Optional(CONF_ON_TOUCH): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TouchTrigger),
                }
            ),
            cv.Optional(CONF_TOUCH_SLEEP_TIMEOUT): cv.int_range(min=3, max=65535),
            cv.Optional(CONF_WAKE_UP_PAGE): cv.uint8_t,
            cv.Optional(CONF_START_UP_PAGE): cv.uint8_t,
            cv.Optional(CONF_AUTO_WAKE_ON_TOUCH, default=True): cv.boolean,
            cv.Optional(CONF_EXIT_REPARSE_ON_START, default=False): cv.boolean,
            cv.Optional(CONF_SKIP_CONNECTION_HANDSHAKE, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await uart.register_uart_device(var, config)

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
        if CORE.is_esp32 and CORE.using_arduino:
            cg.add_library("WiFiClientSecure", None)
            cg.add_library("HTTPClient", None)
        elif CORE.is_esp32 and CORE.using_esp_idf:
            esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True
            )
        elif CORE.is_esp8266 and CORE.using_arduino:
            cg.add_library("ESP8266HTTPClient", None)

    if CONF_TOUCH_SLEEP_TIMEOUT in config:
        cg.add(var.set_touch_sleep_timeout_internal(config[CONF_TOUCH_SLEEP_TIMEOUT]))

    if CONF_WAKE_UP_PAGE in config:
        cg.add(var.set_wake_up_page_internal(config[CONF_WAKE_UP_PAGE]))

    if CONF_START_UP_PAGE in config:
        cg.add(var.set_start_up_page_internal(config[CONF_START_UP_PAGE]))

    cg.add(var.set_auto_wake_on_touch_internal(config[CONF_AUTO_WAKE_ON_TOUCH]))

    cg.add(var.set_exit_reparse_on_start_internal(config[CONF_EXIT_REPARSE_ON_START]))

    cg.add(var.set_skip_connection_handshake(config[CONF_SKIP_CONNECTION_HANDSHAKE]))

    await display.register_display(var, config)

    for conf in config.get(CONF_ON_SETUP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SLEEP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_WAKE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_PAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "x")], conf)

    for conf in config.get(CONF_ON_TOUCH, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (cg.uint8, "page_id"),
                (cg.uint8, "component_id"),
                (cg.bool_, "touch_event"),
            ],
            conf,
        )
