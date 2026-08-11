from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, uart, usb_cdc_acm
from esphome.components.bridge import DOMAIN as BRIDGE_DOMAIN
from esphome.components.esp32 import VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["tinyusb", "uart", "usb_cdc_acm"]

CONF_DTR_PIN = "dtr_pin"
CONF_RTS_PIN = "rts_pin"
CONF_USB_CDC_ACM_ID = "usb_cdc_acm_id"
CONF_UART_RX_BUFFER_SIZE = "uart_rx_buffer_size"
CONF_UART_TX_BUFFER_SIZE = "uart_tx_buffer_size"

usb_uart_bridge_ns = cg.esphome_ns.namespace("usb_uart_bridge")
USBUARTBridge = usb_uart_bridge_ns.class_("USBUARTBridge", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(USBUARTBridge),
            cv.Required(CONF_UART_ID): cv.use_id(uart.IDFUARTComponent),
            cv.Required(CONF_USB_CDC_ACM_ID): cv.use_id(usb_cdc_acm.USBCDCACMInstance),
            cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_UART_RX_BUFFER_SIZE, default=256): cv.int_range(
                min=64, max=65535
            ),
            cv.Optional(CONF_UART_TX_BUFFER_SIZE, default=256): cv.int_range(
                min=64, max=65535
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    # Narrower than usb_cdc_acm's variant list on purpose: S31/H4 untested on
    # hardware; extend once verified.
    esp32.only_on_variant(
        supported=[VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3],
    ),
)


def _subtree_references_uart(node: object, uart_id: str) -> bool:
    """Return True if any dict in the subtree has a uart_id entry naming this bus."""
    if isinstance(node, dict):
        return any(
            (key == CONF_UART_ID and str(value) == uart_id)
            or _subtree_references_uart(value, uart_id)
            for key, value in node.items()
        )
    if isinstance(node, list):
        return any(_subtree_references_uart(item, uart_id) for item in node)
    return False


def _final_validate(config: ConfigType) -> ConfigType:
    # Bridges of any platform must own their interfaces exclusively; shared ring
    # buffers and overwritten callbacks would corrupt both streams silently. The
    # seen-set is keyed on the bridge domain so future platforms share it.
    data = fv.full_config.get().data.setdefault(BRIDGE_DOMAIN, {})
    for conf_key, label in (
        (CONF_UART_ID, "UART"),
        (CONF_USB_CDC_ACM_ID, "USB CDC-ACM interface"),
    ):
        used = data.setdefault(conf_key, set())
        key = str(config[conf_key])
        if key in used:
            raise cv.Invalid(
                f"The {label} '{key}' is already bridged by another 'bridge' instance; "
                f"each bridge requires its own {label}.",
                [conf_key],
            )
        used.add(key)

    # Other components bind either interface through the same uart_id key (the CDC
    # instance is itself a uart::UARTComponent) and would race the worker tasks.
    # Bare `id:` references (a uart.write action) cannot be distinguished; not caught.
    for conf_key, label in (
        (CONF_UART_ID, "UART"),
        (CONF_USB_CDC_ACM_ID, "USB CDC-ACM interface"),
    ):
        owned_id = str(config[conf_key])
        for domain, domain_conf in fv.full_config.get().items():
            # Bridge-vs-bridge sharing is already rejected above.
            if domain == BRIDGE_DOMAIN:
                continue
            if _subtree_references_uart(domain_conf, owned_id):
                raise cv.Invalid(
                    f"The {label} '{owned_id}' is also used by '{domain}'; a bridge "
                    f"requires exclusive use of its {label}.",
                    [conf_key],
                )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    usb_cdc = await cg.get_variable(config[CONF_USB_CDC_ACM_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        uart_component,
        usb_cdc,
        config[CONF_UART_RX_BUFFER_SIZE],
        config[CONF_UART_TX_BUFFER_SIZE],
    )
    await cg.register_component(var, config)

    if dtr_pin_config := config.get(CONF_DTR_PIN):
        dtr_pin = await cg.gpio_pin_expression(dtr_pin_config)
        cg.add(var.set_dtr_pin(dtr_pin))
    if rts_pin_config := config.get(CONF_RTS_PIN):
        rts_pin = await cg.gpio_pin_expression(rts_pin_config)
        cg.add(var.set_rts_pin(rts_pin))
