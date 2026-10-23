from esphome import automation
import esphome.codegen as cg
from esphome.components import canbus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INDEX, CONF_SIZE, CONF_VALUE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kj4dxk"]
DEPENDENCIES = ["canbus"]
DOMAIN = "canopen"
MULTI_CONF = True

CONF_CANOPEN_ID = "canopen_id"
CONF_NODE_ID = "node_id"
CONF_SUBINDEX = "subindex"
CONF_RESPONSE_TIMEOUT = "response_timeout"

canopen_ns = cg.esphome_ns.namespace(DOMAIN)
CANopenComponent = canopen_ns.class_("CANopenComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CANopenComponent),
        cv.GenerateID(canbus.CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_RESPONSE_TIMEOUT, default="500ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(max=cv.TimePeriod(milliseconds=60000)),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

SDO_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NODE_ID): cv.int_range(min=1, max=127),
        cv.Required(CONF_INDEX): cv.hex_uint16_t,
        cv.Optional(CONF_SUBINDEX, default=0): cv.hex_uint8_t,
    }
)


def validate_single_hub_per_bus(config: ConfigType) -> ConfigType:
    hubs = fv.full_config.get()[DOMAIN]
    bus_id = config[canbus.CONF_CANBUS_ID]
    if sum(hub[canbus.CONF_CANBUS_ID] == bus_id for hub in hubs) > 1:
        raise cv.Invalid("Only one CANopen SDO client hub may use each CAN bus")
    return config


FINAL_VALIDATE_SCHEMA = validate_single_hub_per_bus


async def to_code(config: ConfigType) -> None:
    bus = await cg.get_variable(config[canbus.CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], bus)
    await cg.register_component(var, config)
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT].total_milliseconds))


automation.register_apply_action(
    "canopen.sdo_write",
    SDO_SCHEMA.extend(
        {
            cv.GenerateID(): cv.use_id(CANopenComponent),
            cv.Required(CONF_VALUE): cv.templatable(cv.uint32_t),
            cv.Required(CONF_SIZE): cv.one_of(1, 2, 4, int=True),
        }
    ),
    automation.ApplyCall(
        "write_sdo({}, {}, {}, {}, {})",
        (
            (CONF_NODE_ID, cg.uint8),
            (CONF_INDEX, cg.uint16),
            (CONF_SUBINDEX, cg.uint8),
            (CONF_VALUE, cg.uint32),
            (CONF_SIZE, cg.uint8),
        ),
    ),
)
