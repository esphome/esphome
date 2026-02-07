import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@rawsludge"]
DEPENDENCIES = ["ble_client", "esp32_ble_tracker"]
AUTO_LOAD = ["text_sensor", "switch", "number", "select", "light"]
PLATFORMS = ["esp32"]

MULTI_CONF = True

fendt_caravan_ns = cg.esphome_ns.namespace("fendt_caravan")
FendtCaravan = fendt_caravan_ns.class_(
    "FendtCaravan", ble_client.BLEClientNode, cg.Component
)
CaravanDeviceComponent = fendt_caravan_ns.class_("CaravanDeviceComponent", cg.Component)

CONF_PARENT_ID = "parent_id"

DeviceType = fendt_caravan_ns.enum("DeviceType")
DEVICE_TYPE = {
    "mcu": DeviceType.MCU,
}

FendtTextSensor = fendt_caravan_ns.class_("FendtTextSensor", cg.Component)
FendtNumber = fendt_caravan_ns.class_("FendtNumber", cg.Component)
FendtSelect = fendt_caravan_ns.class_("FendtSelect", cg.Component)
FendtSensor = fendt_caravan_ns.class_("FendtSensor", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FendtCaravan),
            cv.Required(ble_client.CONF_BLE_CLIENT_ID): cv.use_id(ble_client.BLEClient),
        }
    ).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
)

CONF_FENDT_CARAVAN_ID = "fendt_caravan_id"
CONFIG_FENDT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FENDT_CARAVAN_ID): cv.use_id(FendtCaravan),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
