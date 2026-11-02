from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ON_RESPONSE
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["modbus"]
DOMAIN = "modbus_monitor"
MULTI_CONF = True

CONF_ON_REQUEST = "on_request"

modbus_monitor_ns = cg.esphome_ns.namespace("modbus_monitor")
MonitorItem = modbus_monitor_ns.class_("MonitorItem")
RegisterDecoder = modbus_monitor_ns.class_("RegisterDecoder")


_HANDLER = automation.validate_synchronous_automation(
    "modbus_monitor handlers", "the request/response data", single=True
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.Modbus),
            cv.Optional(CONF_ON_REQUEST): _HANDLER,
            cv.Optional(CONF_ON_RESPONSE): _HANDLER,
        }
    ),
    cv.has_at_least_one_key(CONF_ON_REQUEST, CONF_ON_RESPONSE),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    if on_request := config.get(CONF_ON_REQUEST):
        await modbus.register_on_request_automation(hub, on_request)
    if on_response := config.get(CONF_ON_RESPONSE):
        await modbus.register_on_response_automation(hub, on_response)


async def add_monitor_item(hub_id: ID, item: cg.MockObj) -> None:
    """Add a platform item to the hub's RegisterDecoder.

    One decoder per hub, created with its on_response callback on first use, so a hundred register sensors
    cost one callback slot rather than a hundred. The items are added at FINAL priority, once every platform
    has run, so the decoder is sized once to exactly the items it gets.
    """
    decoders = CORE.data.setdefault(DOMAIN, {})
    if (entry := decoders.get(hub_id.id)) is None:
        decoder = cg.new_Pvariable(
            ID(
                f"{hub_id.id}_register_decoder",
                is_declaration=True,
                type=RegisterDecoder,
            )
        )
        # Stored before the await below, so a second item on this hub cannot create another decoder.
        entry = decoders[hub_id.id] = (decoder, [])
        CORE.add_job(_add_items, *entry)
        hub = await cg.get_variable(hub_id)
        modbus.add_on_response_callback(
            hub,
            cg.RawExpression(
                "[](uint8_t address, std::span<const uint8_t> request, std::span<const uint8_t> response) {"
                f" {decoder}->on_response(address, request, response); }}"
            ),
        )
    entry[1].append(item)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_items(decoder: cg.MockObj, items: list[cg.MockObj]) -> None:
    cg.add(decoder.init_items(len(items)))
    for item in items:
        cg.add(decoder.add_item(item))
