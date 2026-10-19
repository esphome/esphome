import esphome.codegen as cg
from esphome.components import ble_client, time
import esphome.config_validation as cv
from esphome.const import CONF_RECEIVE_TIMEOUT, CONF_TIME_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["ble_client"]
DOMAIN = "bedjet"
MULTI_CONF = True
CONF_BEDJET_ID = "bedjet_id"

bedjet_ns = cg.esphome_ns.namespace("bedjet")
BedJetHub = bedjet_ns.class_("BedJetHub", ble_client.BLEClientNode, cg.PollingComponent)

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BedJetHub),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("15s"))
)

BEDJET_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BEDJET_ID): cv.use_id(BedJetHub),
    }
)


async def register_bedjet_child(var: MockObj, config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_BEDJET_ID])
    cg.add(parent.register_child(var))


async def to_code(config: ConfigType) -> None:
    var = await ble_client.new_ble_node_component(config)
    await cg.add_variable(var.set_time_id, config.get(CONF_TIME_ID))
    if (receive_timeout := config.get(CONF_RECEIVE_TIMEOUT)) is not None:
        cg.add(var.set_status_timeout(receive_timeout))
