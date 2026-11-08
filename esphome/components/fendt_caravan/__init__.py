import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@rawsludge"]
DEPENDENCIES = ["ble_client", "esp32_ble_tracker"]
AUTO_LOAD = []
PLATFORMS = ["esp32"]
DOMAIN = "fendt_caravan"

MULTI_CONF = True

fendt_caravan_ns = cg.esphome_ns.namespace("fendt_caravan")
FendtCaravan = fendt_caravan_ns.class_(
    "FendtCaravan", ble_client.BLEClientNode, cg.Component
)
FendtCaravanHubBase = fendt_caravan_ns.class_(
    "FendtCaravanHubBase", cg.PollingComponent
)
MainControlUnitHub = fendt_caravan_ns.class_("MainControlUnitHub", FendtCaravanHubBase)

CONF_BT_ID = "bt_id"
CONF_PARENT_ID = "parent_id"
CONF_MAIN_CONTROL_UNIT = "main_control_unit"

# The address the panel was paired with; it drops the link when given any other one
DEFAULT_BT_ID = "C0:EE:FB:90:B0:A7"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FendtCaravan),
            cv.Optional(CONF_BT_ID, default=DEFAULT_BT_ID): cv.mac_address,
            cv.Required(ble_client.CONF_BLE_CLIENT_ID): cv.use_id(ble_client.BLEClient),
            cv.Required(CONF_MAIN_CONTROL_UNIT): cv.Schema(
                {
                    cv.GenerateID(): cv.declare_id(MainControlUnitHub),
                }
            ).extend(cv.polling_component_schema("60s")),
        }
    ).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
)


async def to_code(config):
    # The handshake carries the paired address in lower case
    bt_id = ":".join(f"{part:02x}" for part in config[CONF_BT_ID].parts)
    var = cg.new_Pvariable(config[CONF_ID], f"net-BT_ID-{bt_id}")
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    hub = cg.new_Pvariable(config[CONF_MAIN_CONTROL_UNIT][CONF_ID])
    await cg.register_parented(hub, var)
    await cg.register_component(hub, config[CONF_MAIN_CONTROL_UNIT])
    cg.add(var.set_mcu_hub(hub))
