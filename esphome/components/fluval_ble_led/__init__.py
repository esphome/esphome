import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, time
from esphome.const import CONF_ID, CONF_TIME_ID
from esphome import automation
from esphome.automation import maybe_simple_id

CONF_NUMBER_OF_CHANNELS = "number_of_channels"

CODEOWNERS = ["@mrzottel"]
DEPENDENCIES = ["ble_client"]

fluval_ble_led_ns = cg.esphome_ns.namespace("fluval_ble_led")
FluvalBleLed = fluval_ble_led_ns.class_(
    "FluvalBleLed", ble_client.BLEClientNode, cg.Component
)

# Actions
SyncTimeAction = fluval_ble_led_ns.class_(
    "FluvalBleLedTimeSyncAction", automation.Action
)

MULTI_CONF = 3

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FluvalBleLed),
            cv.Required(CONF_NUMBER_OF_CHANNELS): cv.int_range(4, 5),
            cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)

CONF_FLUVAL_BLE_LED_ID = "fluval_ble_led_id"

FLUVAL_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FLUVAL_BLE_LED_ID): cv.use_id(FluvalBleLed),
    }
)

FLUVAL_BLE_LED_SYNC_TIME_SCHEMA = maybe_simple_id(
    {cv.Required(CONF_ID): cv.use_id(FluvalBleLed)}
)


async def register_fluval_led_client(var, config):
    parent = await cg.get_variable(config[CONF_FLUVAL_BLE_LED_ID])
    cg.add(parent.register_fluval_led_client(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_number_of_channels(config[CONF_NUMBER_OF_CHANNELS]))

    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_))


@automation.register_action(
    "fluval_ble_led.sync_time", SyncTimeAction, FLUVAL_BLE_LED_SYNC_TIME_SCHEMA
)
async def sync_time_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
