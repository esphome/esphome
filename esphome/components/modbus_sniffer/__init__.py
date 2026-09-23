from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ON_RESPONSE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@dalklein"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True

CONF_ON_REQUEST = "on_request"

# Spans, not copies, matching modbus_client. The PDUs are handed over undecoded so a lambda can
# pass them straight to the modbus::helpers functions.
_PDU_SPAN = cg.std_span.template(cg.uint8.operator("const"))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.ModbusSniffer),
        cv.Optional(CONF_ON_REQUEST): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
    }
)


def _final_validate(config: ConfigType) -> None:
    # A hub's triggers hold one automation each, so a second modbus_sniffer on the same hub would
    # silently replace the first one's on_request/on_response rather than run alongside it. One
    # sniffer per hub; watching two buses means two uarts, two hubs and two of these.
    hubs = [
        instance[modbus.CONF_MODBUS_ID]
        for instance in fv.full_config.get().get("modbus_sniffer", [])
    ]
    own_hub = config[modbus.CONF_MODBUS_ID]
    if hubs.count(own_hub) > 1:
        raise cv.Invalid(
            f"Multiple modbus_sniffer components use the hub '{own_hub}'. Each one needs its own "
            f"hub, or their automations would overwrite each other."
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    # The hub IS the sniffer: `role: sniffer` creates it and owns the uart. This only attaches
    # behaviour to it.
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])

    if on_request := config.get(CONF_ON_REQUEST):
        await automation.build_automation(
            hub.get_request_trigger(),
            [(cg.uint8, "address"), (_PDU_SPAN, "request_pdu")],
            on_request,
        )

    if on_response := config.get(CONF_ON_RESPONSE):
        await automation.build_automation(
            hub.get_response_trigger(),
            [
                (cg.uint8, "address"),
                (_PDU_SPAN, "request_pdu"),
                (_PDU_SPAN, "response_pdu"),
            ],
            on_response,
        )
