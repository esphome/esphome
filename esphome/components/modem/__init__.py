from esphome import automation, pins
import esphome.codegen as cg
from esphome.components.const import CONF_RTS_PIN
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.text_sensor import new_text_sensor, text_sensor_schema
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_COMMAND,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_MODEL,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_REBOOT_TIMEOUT,
    CONF_RX_BUFFER_SIZE,
    CONF_RX_PIN,
    CONF_TX_BUFFER_SIZE,
    CONF_TX_PIN,
    CONF_USE_ADDRESS,
)
from esphome.core import coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@oarcher"]
DEPENDENCIES = ["esp32"]
CONFLICTS_WITH = ["captive_portal", "ethernet"]
DOMAIN = "modem"

CONF_PIN_CODE = "pin_code"
CONF_APN = "apn"
CONF_CTS_PIN = "cts_pin"
CONF_INIT_AT = "init_at"
CONF_ON_SYNC = "on_sync"
CONF_DTE_BUFFER_SIZE = "dte_buffer_size"
CONF_URC = "urc"

# Model name to the esp_modem factory that drives it
MODEM_MODELS = {
    "BG96": "BG96",
    "SIM800": "SIM800",
    "SIM7000": "SIM7000",
    "SIM7070": "SIM7070",
    "SIM7080": "SIM7000",
    "SIM7600": "SIM7600",
    "SIM7670": "SIM7600",
    "GENERIC": "generic",
}
modem_ns = cg.esphome_ns.namespace("modem")
ModemComponent = modem_ns.class_("ModemComponent", cg.Component)

ModemComponentState = modem_ns.enum("ModemComponentState", is_class=True)


def AUTO_LOAD(config: ConfigType) -> list[str]:
    # The URC text sensor is the only entity the component owns
    return ["network", "text_sensor"] if CONF_URC in config else ["network"]


# Each trigger fires on one state, and none while the component is failed
_NOT_FAILED = "!{parent}->is_failed()"


def _on_state(conf_key: str, state: str) -> automation.CallbackAutomation:
    return automation.CallbackAutomation(
        conf_key,
        "add_on_state_callback",
        params=[(ModemComponentState, "state")],
        forward=[],
        when=f"{_NOT_FAILED} && state == modem::ModemComponentState::{state}",
    )


_CALLBACK_AUTOMATIONS = (
    _on_state(CONF_ON_CONNECT, "MODEM_CONNECTED"),
    _on_state(CONF_ON_DISCONNECT, "MODEM_DISCONNECTING"),
    _on_state(CONF_ON_SYNC, "MODEM_SYNCED"),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ModemComponent),
            cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RTS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CTS_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_BAUD_RATE): cv.positive_int,
            cv.Required(CONF_MODEL): cv.one_of(*MODEM_MODELS, upper=True),
            cv.Required(CONF_APN): cv.string,
            cv.Optional(CONF_PIN_CODE): cv.sensitive(cv.string_strict),
            cv.Optional(CONF_USE_ADDRESS): cv.string,
            cv.Optional(CONF_INIT_AT): cv.ensure_list(cv.string),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_TX_BUFFER_SIZE, default=1024): cv.positive_int,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=1024): cv.positive_int,
            cv.Optional(CONF_DTE_BUFFER_SIZE, default=1024): cv.positive_int,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="10min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation({}),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation({}),
            cv.Optional(CONF_ON_SYNC): automation.validate_automation({}),
            cv.Optional(CONF_URC): text_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_none_or_all_keys(CONF_RTS_PIN, CONF_CTS_PIN),
)


MODEM_ID_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(ModemComponent)}
)
SEND_AT_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(ModemComponent),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    },
    key=CONF_COMMAND,
)

automation.register_apply_action(
    "modem.enable", MODEM_ID_SCHEMA, automation.ApplyCall("enable()")
)
automation.register_apply_action(
    "modem.disable", MODEM_ID_SCHEMA, automation.ApplyCall("disable()")
)
automation.register_apply_action(
    "modem.send_at",
    SEND_AT_SCHEMA,
    automation.ApplyCall("send_at({}, 1000, true)", ((CONF_COMMAND, cg.std_string),)),
)
automation.register_apply_condition(
    "modem.connected", MODEM_ID_SCHEMA, "is_connected()"
)
automation.register_apply_condition("modem.enabled", MODEM_ID_SCHEMA, "is_enabled()")


@coroutine_with_priority(60.0)
async def to_code(config: ConfigType) -> None:
    add_idf_component(name="espressif/esp_modem", ref="2.0.2")

    add_idf_sdkconfig_option("CONFIG_PPP", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_DELAY_AFTER_DLCI_SETUP", 500)
    add_idf_sdkconfig_option("CONFIG_PPP_SUPPORT", True)
    add_idf_sdkconfig_option(
        "CONFIG_ESP_MODEM_CMUX_USE_SHORT_PAYLOADS_ONLY", True
    )  # Slower but more reliable
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_URC_HANDLER", True)

    cg.add_define("USE_MODEM")

    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))
    cg.add_optional(var.set_use_address, config.get(CONF_USE_ADDRESS))
    cg.add_optional(var.set_pin_code, config.get(CONF_PIN_CODE))

    if config[CONF_ENABLE_ON_BOOT]:
        cg.add(var.enable())

    cg.add_optional(var.set_init_at_commands, config.get(CONF_INIT_AT))

    model = config[CONF_MODEL]
    factory = cg.RawExpression(f"esp_modem::create_{MODEM_MODELS[model]}_dce")
    cg.add(var.set_model(model, factory))
    cg.add(var.set_apn(config[CONF_APN]))

    await cg.add_pin(var.set_tx_pin, config[CONF_TX_PIN])
    await cg.add_pin(var.set_rx_pin, config[CONF_RX_PIN])
    await cg.add_pin(var.set_rts_pin, config.get(CONF_RTS_PIN))
    await cg.add_pin(var.set_cts_pin, config.get(CONF_CTS_PIN))

    cg.add_optional(var.set_baud_rate, config.get(CONF_BAUD_RATE))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    cg.add(var.set_tx_buffer_size(config[CONF_TX_BUFFER_SIZE]))
    cg.add(var.set_dte_buffer_size(config[CONF_DTE_BUFFER_SIZE]))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    if (urc_config := config.get(CONF_URC)) is not None:
        cg.add_define("USE_MODEM_URC")
        cg.add(var.set_urc_text_sensor(await new_text_sensor(urc_config)))

    await cg.register_component(var, config)
