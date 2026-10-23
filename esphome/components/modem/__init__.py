import voluptuous as vol

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components.const import CONF_RTS_PIN
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.network import add_use_address, require_ipv4
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
    CONF_UPDATE_INTERVAL,
    CONF_USE_ADDRESS,
)
from esphome.core import CORE, ID, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

# Static so `api:` finds network before its dependency check; a config-dependent
# AUTO_LOAD resolves after that check and fails a cellular-only node
AUTO_LOAD = ["network"]
CODEOWNERS = ["@oarcher"]
CONFLICTS_WITH = ["captive_portal", "ethernet"]
DEPENDENCIES = ["esp32"]
DOMAIN = "modem"

CONF_PIN_CODE = "pin_code"
CONF_APN = "apn"
CONF_CTS_PIN = "cts_pin"
CONF_INIT_AT = "init_at"
CONF_ON_SYNC = "on_sync"
CONF_DTE_BUFFER_SIZE = "dte_buffer_size"
CONF_MODEM_ID = "modem_id"

esp_modem_ns = cg.global_ns.namespace("esp_modem")

# Model name to the esp_modem factory that drives it
MODEM_MODELS = {
    "BG96": esp_modem_ns.create_BG96_dce,
    "SIM800": esp_modem_ns.create_SIM800_dce,
    "SIM7000": esp_modem_ns.create_SIM7000_dce,
    "SIM7070": esp_modem_ns.create_SIM7070_dce,
    "SIM7080": esp_modem_ns.create_SIM7000_dce,
    "SIM7600": esp_modem_ns.create_SIM7600_dce,
    "SIM7670": esp_modem_ns.create_SIM7600_dce,
    "GENERIC": esp_modem_ns.create_generic_dce,
}
modem_ns = cg.esphome_ns.namespace("modem")
ModemComponent = modem_ns.class_("ModemComponent", cg.Component)

ModemState = modem_ns.enum("ModemState", is_class=True)


def _on_state(conf_key: str, state: str) -> automation.CallbackAutomation:
    return automation.CallbackAutomation(
        conf_key,
        "add_on_state_callback",
        params=[(ModemState, "state")],
        forward=[],
        when=f"state == modem::ModemState::{state}",
    )


def _default_use_address(config: ConfigType) -> ConfigType:
    # The host tools resolve the device through this, like the other network components
    config.setdefault(CONF_USE_ADDRESS, f"{CORE.name}.local")
    return config


_CALLBACK_AUTOMATIONS = (
    _on_state(CONF_ON_CONNECT, "MODEM_STATE_CONNECTED"),
    _on_state(CONF_ON_DISCONNECT, "MODEM_STATE_DISCONNECTING"),
    _on_state(CONF_ON_SYNC, "MODEM_STATE_SYNCED"),
)

# AT_COMMAND_SIZE - 1 in modem_component.h; the buffer keeps one byte for the terminator
MAX_AT_COMMAND_LENGTH = 127


# A SIM PIN is 4 to 8 digits; anything else would spend one of the SIM's attempts. \Z, not $, so a
# trailing newline is refused too
_validate_pin_code = cv.All(
    cv.string_strict,
    vol.Match(r"^[0-9]{4,8}\Z", msg="pin_code must be 4 to 8 digits"),
)
# The IDF UART driver needs more than its 128 byte FIFO; the setters take uint16_t
_UART_BUFFER_SIZE = cv.int_range(min=129, max=65535)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemComponent),
            cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RTS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CTS_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_BAUD_RATE): cv.positive_int,
            cv.Required(CONF_MODEL): cv.one_of(*MODEM_MODELS, upper=True),
            cv.Required(CONF_APN): cv.string,
            cv.Optional(CONF_PIN_CODE): cv.sensitive(_validate_pin_code),
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            # The command count is stored in a byte
            cv.Optional(CONF_INIT_AT): cv.All(
                cv.ensure_list(
                    cv.All(cv.string, cv.ByteLength(max=MAX_AT_COMMAND_LENGTH))
                ),
                cv.Length(max=255),
            ),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_TX_BUFFER_SIZE, default=1024): _UART_BUFFER_SIZE,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=1024): _UART_BUFFER_SIZE,
            # CMUX keeps 128 bytes of the DTE buffer back, so a smaller buffer could not be read
            cv.Optional(CONF_DTE_BUFFER_SIZE, default=1024): cv.int_range(
                min=256, max=65535
            ),
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="10min"
            ): cv.positive_time_period_milliseconds,
            # How often the sensor and text_sensor platforms query the modem
            # Each poll is several AT exchanges, so not more often than every 10 s; the top keeps deadlines in int32_t range
            cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(seconds=10), max=cv.TimePeriod(days=24)),
            ),
            **{
                cv.Optional(a.conf_key): automation.validate_automation({})
                for a in _CALLBACK_AUTOMATIONS
            },
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_none_or_all_keys(CONF_RTS_PIN, CONF_CTS_PIN),
    _default_use_address,
    require_ipv4(DOMAIN),
)


MODEM_ID_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(ModemComponent)}
)
SEND_AT_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(ModemComponent),
        cv.Required(CONF_COMMAND): cv.templatable(
            cv.All(cv.string, cv.ByteLength(max=MAX_AT_COMMAND_LENGTH))
        ),
    },
    key=CONF_COMMAND,
)

for _name in ("enable", "disable"):
    automation.register_apply_action(
        f"modem.{_name}", MODEM_ID_SCHEMA, automation.ApplyCall(f"{_name}()")
    )
_send_at_builder = automation.apply_action_builder(
    "modem.send_at",
    SEND_AT_SCHEMA,
    # A constant renders as a literal and takes the const char * overload; a lambda returns std::string
    automation.ApplyField(CONF_COMMAND, "send_at", cg.std_string),
)


@automation.register_action(
    "modem.send_at", automation.ApplyAction, SEND_AT_SCHEMA, synchronous=True
)
async def send_at_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define("USE_MODEM_SEND_AT")
    return await _send_at_builder(config, action_id, template_arg, args)


automation.register_apply_condition(
    "modem.connected", MODEM_ID_SCHEMA, "is_connected()"
)
automation.register_apply_condition("modem.enabled", MODEM_ID_SCHEMA, "is_enabled()")


@coroutine_with_priority(CoroPriority.COMMUNICATION)
async def to_code(config: ConfigType) -> None:
    add_idf_component(name="espressif/esp_modem", ref="2.1.0")

    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_SUPPORT", True)
    # LCP echo probes notice a modem that went away without a word, which ends the PPP session
    add_idf_sdkconfig_option("CONFIG_LWIP_ENABLE_LCP_ECHO", True)
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_DELAY_AFTER_DLCI_SETUP", 500)
    add_idf_sdkconfig_option(
        "CONFIG_ESP_MODEM_CMUX_USE_SHORT_PAYLOADS_ONLY", True
    )  # Slower but more reliable

    cg.add_define("USE_MODEM")

    model = config[CONF_MODEL]
    factory = MODEM_MODELS[model]
    tx = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    rx = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    var = cg.new_Pvariable(config[CONF_ID], tx, rx, model, factory, config[CONF_APN])

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))
    add_use_address(var, config[CONF_USE_ADDRESS])
    if (pin_code := config.get(CONF_PIN_CODE)) is not None:
        cg.add_define("USE_MODEM_PIN_CODE")
        cg.add(var.set_pin_code(pin_code))

    if config[CONF_ENABLE_ON_BOOT]:
        cg.add(var.enable())

    if init_at := config.get(CONF_INIT_AT):
        # The commands are literals in flash; the component keeps a pointer to the array
        arr = cg.shared_progmem_array(
            "modem_init_at", cg.const_char_ptr, cg.ArrayInitializer(*init_at)
        )
        cg.add(var.set_init_at_commands(arr, len(init_at)))

    await cg.add_pin(var.set_rts_pin, config.get(CONF_RTS_PIN))
    await cg.add_pin(var.set_cts_pin, config.get(CONF_CTS_PIN))

    cg.add_optional(var.set_baud_rate, config.get(CONF_BAUD_RATE))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    cg.add(var.set_tx_buffer_size(config[CONF_TX_BUFFER_SIZE]))
    cg.add(var.set_dte_buffer_size(config[CONF_DTE_BUFFER_SIZE]))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    await cg.register_component(var, config)


def request_polling(hub: MockObj) -> None:
    """Have the modem task query the modem every update_interval for the sensor platforms."""
    cg.add_define("USE_MODEM_POLL")
    cg.add(hub.set_update_interval(CORE.config[DOMAIN][CONF_UPDATE_INTERVAL]))
