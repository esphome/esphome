from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, uart, usb_cdc_acm
from esphome.components.bridge import DOMAIN as BRIDGE_DOMAIN
from esphome.components.const import CONF_BRIDGE_ID
from esphome.components.esp32 import VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_ID, CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["tinyusb", "uart", "usb_cdc_acm"]

CONF_DTR_PIN = "dtr_pin"
CONF_RTS_PIN = "rts_pin"
CONF_USB_CDC_ACM_ID = "usb_cdc_acm_id"
UART_MUX_DOMAIN = "uart_mux"

cdc_acm_uart_ns = cg.esphome_ns.namespace("cdc_acm_uart")
CDCACMUARTBridge = cdc_acm_uart_ns.class_("CDCACMUARTBridge", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CDCACMUARTBridge),
            cv.Required(CONF_UART_ID): cv.use_id(uart.IDFUARTComponent),
            cv.Required(CONF_USB_CDC_ACM_ID): cv.use_id(usb_cdc_acm.USBCDCACMInstance),
            cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
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


def _reject_debug(uart_conf: ConfigType) -> ConfigType:
    # The worker tasks use the IDF driver directly, so the uart debugger never sees
    # bridge traffic and its dummy_receiver would drain RX bytes on the main loop.
    if CONF_DEBUG in uart_conf:
        raise cv.Invalid(
            "A bridged UART cannot use 'debug'; the bridge bypasses the UART "
            "component's read/write path.",
            [CONF_DEBUG],
        )
    return uart_conf


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    # Bridges of any platform must own their interfaces exclusively; shared ring
    # buffers and overwritten callbacks would corrupt both streams silently. The
    # seen-set is keyed on the bridge domain so future platforms share it.
    # Other components bind either interface through the same uart_id key (the CDC
    # instance is itself a uart::UARTComponent) and would race the worker tasks.
    # Bare `id:` references (a uart.write action) cannot be distinguished; not caught.
    data = full_config.data.setdefault(BRIDGE_DOMAIN, {})
    for conf_key, label in (
        (CONF_UART_ID, "UART"),
        (CONF_USB_CDC_ACM_ID, "USB CDC-ACM interface"),
    ):
        owned_id = str(config[conf_key])
        used = data.setdefault(conf_key, set())
        if owned_id in used:
            raise cv.Invalid(
                f"The {label} '{owned_id}' is already bridged by another 'bridge' "
                f"instance; each bridge requires its own {label}.",
                [conf_key],
            )
        used.add(owned_id)
        for domain, domain_conf in full_config.items():
            if domain == BRIDGE_DOMAIN:
                continue
            # A mux bound to this bridge exists to share its UART: it pauses the
            # bridge before touching the bus.
            if domain == UART_MUX_DOMAIN:
                domain_conf = [
                    mux
                    for mux in domain_conf
                    if str(mux.get(CONF_BRIDGE_ID)) != str(config[CONF_ID])
                ]
            if _subtree_references_uart(domain_conf, owned_id):
                raise cv.Invalid(
                    f"The {label} '{owned_id}' is also used by '{domain}'; a bridge "
                    f"requires exclusive use of its {label}.",
                    [conf_key],
                )

    fv.id_declaration_match_schema(_reject_debug)(config[CONF_UART_ID])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    usb_cdc = await cg.get_variable(config[CONF_USB_CDC_ACM_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component, usb_cdc)
    await cg.register_component(var, config)

    if dtr_pin_config := config.get(CONF_DTR_PIN):
        dtr_pin = await cg.gpio_pin_expression(dtr_pin_config)
        cg.add(var.set_dtr_pin(dtr_pin))
    if rts_pin_config := config.get(CONF_RTS_PIN):
        rts_pin = await cg.gpio_pin_expression(rts_pin_config)
        cg.add(var.set_rts_pin(rts_pin))
