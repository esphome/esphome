from esphome.const import (
    CONF_ID,
    CONF_USE_ADDRESS,
    CONF_TX_PIN,
    CONF_RX_PIN,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_MODEL,
    CONF_TRIGGER_ID,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ENABLE_ON_BOOT,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import coroutine_with_priority
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome import pins, automation

CODEOWNERS = ["@oarcher"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]
# following should be removed if conflicts are resolved (so we can have a wifi ap using modem)
CONFLICTS_WITH = ["wifi", "captive_portal", "ethernet"]

CONF_PIN_CODE = "pin_code"
CONF_APN = "apn"
CONF_DTR_PIN = "dtr_pin"
CONF_STATUS_PIN = "status_pin"
CONF_POWER_PIN = "power_pin"
CONF_INIT_AT = "init_at"
CONF_ON_NOT_RESPONDING = "on_not_responding"

MODEM_MODELS = ["BG96", "SIM800", "SIM7000", "SIM7600", "GENERIC"]
MODEM_MODELS_POWER = {
    "BG96": {"ton": 600, "tonuart": 4900, "toff": 650, "toffuart": 2000},
    "SIM800": {"ton": 1300, "tonuart": 3000, "toff": 200, "toffuart": 3000},
    "SIM7000": {"ton": 1100, "tonuart": 4500, "toff": 1300, "toffuart": 1800},
    "SIM7600": {"ton": 500, "tonuart": 12000, "toff": 2800, "toffuart": 25000},
}

modem_ns = cg.esphome_ns.namespace("modem")
ModemComponent = modem_ns.class_("ModemComponent", cg.Component)
ModemComponentState = modem_ns.enum("ModemComponentState")
ModemOnNotRespondingTrigger = modem_ns.class_(
    "ModemOnNotRespondingTrigger", automation.Trigger.template()
)
ModemOnConnectTrigger = modem_ns.class_(
    "ModemOnConnectTrigger", automation.Trigger.template()
)
ModemOnDisconnectTrigger = modem_ns.class_(
    "ModemOnDisconnectTrigger", automation.Trigger.template()
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemComponent),
            cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_RX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_MODEL): cv.one_of(*MODEM_MODELS, upper=True),
            cv.Required(CONF_APN): cv.string,
            cv.Optional(CONF_STATUS_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_POWER_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_DTR_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_PIN_CODE): cv.string_strict,
            cv.Optional(CONF_USERNAME): cv.string,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Optional(CONF_USE_ADDRESS): cv.string,
            cv.Optional(CONF_INIT_AT): cv.All(cv.ensure_list(cv.string)),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_ON_NOT_RESPONDING): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ModemOnNotRespondingTrigger
                    )
                }
            ),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ModemOnConnectTrigger)}
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ModemOnDisconnectTrigger
                    )
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp_idf=cv.Version(4, 0, 0),  # 5.2.0 OK
    ),
)


def _final_validate(config):
    if config.get(CONF_POWER_PIN, None) and not config.get(CONF_STATUS_PIN, None):
        raise cv.Invalid(
            f"'{CONF_STATUS_PIN}' must be declared if using '{CONF_POWER_PIN}'"
        )
    if config.get(CONF_POWER_PIN, None):
        if config[CONF_MODEL] not in MODEM_MODELS_POWER:
            raise cv.Invalid(
                f"Modem model '{config[CONF_MODEL]}' has no power power specs."
            )


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(60.0)
async def to_code(config):
    add_idf_component(
        name="esp_modem",
        repo="https://github.com/espressif/esp-protocols.git",
        ref="modem-v1.1.0",
        path="components/esp_modem",
    )

    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", False)
    add_idf_sdkconfig_option("CONFIG_PPP", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_PPP_PAP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_PAP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_DEFRAGMENT_PAYLOAD", True)
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_DELAY_AFTER_DLCI_SETUP", 0)
    add_idf_sdkconfig_option("CONFIG_PPP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_PPP_NOTIFY_PHASE_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_PPP_CHAP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_VJ_HEADER_COMPRESSION", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_NOTIFY_PHASE_SUPPORT", True)
    # commented because cause crash if another UART is defined in the yaml
    # If enabled, it should increase the reliability and the speed of the connection (TODO: test)
    # add_idf_sdkconfig_option("CONFIG_UART_ISR_IN_IRAM", True)

    cg.add_define("USE_MODEM")

    var = cg.new_Pvariable(config[CONF_ID])
    if use_address := config.get(CONF_USE_ADDRESS, None):
        cg.add(var.set_use_address(use_address))

    if username := config.get(CONF_USERNAME, None):
        cg.add(var.set_username(username))

    if password := config.get(CONF_PASSWORD, None):
        cg.add(var.set_password(password))

    if pin_code := config.get(CONF_PIN_CODE, None):
        cg.add(var.set_pin_code(pin_code))

    if enable_on_boot := config.get(CONF_ENABLE_ON_BOOT, None):
        if enable_on_boot:
            cg.add(var.enable())

    if init_at := config.get(CONF_INIT_AT, None):
        for cmd in init_at:
            cg.add(var.add_init_at_command(cmd))

    modem_model = config[CONF_MODEL]
    cg.add_define("USE_MODEM_MODEL", modem_model)
    cg.add_define(f"USE_MODEM_MODEL_{modem_model}")

    if power_spec := MODEM_MODELS_POWER.get(modem_model, None):
        cg.add_define("USE_MODEM_POWER")
        for spec, value in power_spec.items():
            cg.add_define(f"USE_MODEM_POWER_{spec.upper()}", value)

    cg.add(var.set_apn(config[CONF_APN]))

    tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    cg.add(var.set_tx_pin(tx_pin))

    rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    cg.add(var.set_rx_pin(rx_pin))

    if status_pin := config.get(CONF_STATUS_PIN, None):
        pin = await cg.gpio_pin_expression(status_pin)
        cg.add(var.set_status_pin(pin))
        cg.add_define("USE_MODEM_STATUS")

    if power_pin := config.get(CONF_POWER_PIN, None):
        pin = await cg.gpio_pin_expression(power_pin)
        cg.add(var.set_power_pin(pin))

    for conf in config.get(CONF_ON_NOT_RESPONDING, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    await cg.register_component(var, config)
