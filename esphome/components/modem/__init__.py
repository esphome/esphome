from esphome import automation, pins
import esphome.codegen as cg
from esphome.components.const import CONF_RTS_PIN
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.network import add_use_address, require_ipv4
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
from esphome.core import CORE, ID, coroutine_with_priority
from esphome.coroutine import CoroPriority
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
MODEM_MODELS = {m: m for m in ("BG96", "SIM800", "SIM7000", "SIM7070", "SIM7600")} | {
    "SIM7080": "SIM7000",
    "SIM7670": "SIM7600",
    "GENERIC": "generic",
}
modem_ns = cg.esphome_ns.namespace("modem")
ModemComponent = modem_ns.class_("ModemComponent", cg.Component)

ModemComponentState = modem_ns.enum("ModemComponentState", is_class=True)
# Element type of the static init_at array: `static const char *const name[]`
_CHAR_PTR_CONST = cg.global_ns.namespace("char *const")


# Static so `api:` finds network before its dependency check; a config-dependent
# AUTO_LOAD resolves after that check and fails a cellular-only node
AUTO_LOAD = ["network", "text_sensor"]


def _on_state(conf_key: str, state: str) -> automation.CallbackAutomation:
    return automation.CallbackAutomation(
        conf_key,
        "add_on_state_callback",
        params=[(ModemComponentState, "state")],
        forward=[],
        when=f"state == modem::ModemComponentState::{state}",
    )


def _default_use_address(config: ConfigType) -> ConfigType:
    # The host tools resolve the device through this, like the other network components
    config.setdefault(CONF_USE_ADDRESS, f"{CORE.name}.local")
    return config


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
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            # The command count is stored in a byte
            cv.Optional(CONF_INIT_AT): cv.All(
                cv.ensure_list(cv.string), cv.Length(max=255)
            ),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            # The IDF UART driver needs more than its 128 byte FIFO; the setters take uint16_t
            **{
                cv.Optional(k, default=1024): cv.int_range(min=129, max=65535)
                for k in (CONF_TX_BUFFER_SIZE, CONF_RX_BUFFER_SIZE)
            },
            # CMUX keeps 128 bytes of the DTE buffer back, so a smaller buffer could not be read
            cv.Optional(CONF_DTE_BUFFER_SIZE, default=1024): cv.int_range(
                min=256, max=65535
            ),
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="10min"
            ): cv.positive_time_period_milliseconds,
            **{
                cv.Optional(a.conf_key): automation.validate_automation({})
                for a in _CALLBACK_AUTOMATIONS
            },
            cv.Optional(CONF_URC): text_sensor_schema(),
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
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    },
    key=CONF_COMMAND,
)

for _name in ("enable", "disable"):
    automation.register_apply_action(
        f"modem.{_name}", MODEM_ID_SCHEMA, automation.ApplyCall(f"{_name}()")
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
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_URC_HANDLER", True)

    cg.add_define("USE_MODEM")

    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))
    add_use_address(var, config[CONF_USE_ADDRESS])
    cg.add_optional(var.set_pin_code, config.get(CONF_PIN_CODE))

    if config[CONF_ENABLE_ON_BOOT]:
        cg.add(var.enable())

    if init_at := config.get(CONF_INIT_AT):
        # The commands are literals in flash; the component keeps a pointer to the array
        arr_id = ID(
            f"{config[CONF_ID]}_init_at", is_declaration=True, type=_CHAR_PTR_CONST
        )
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*init_at))
        cg.add(var.set_init_at_commands(arr, len(init_at)))

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
