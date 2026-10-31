import esphome.codegen as cg
from esphome.components import ble_client, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_ILLUMINANCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BRIGHTNESS_5,
)
from esphome.types import ConfigType

AUTO_LOAD = ["am43"]
CODEOWNERS = ["@buxtronix"]

am43_ns = cg.esphome_ns.namespace("am43")
Am43 = am43_ns.class_("Am43", ble_client.BLEClientNode, cg.PollingComponent)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Am43),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                kind=sensor.BATTERY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                kind=sensor.PERCENT, icon=ICON_BRIGHTNESS_5
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("120s"))
)


async def to_code(config: ConfigType) -> None:
    var = await ble_client.new_ble_node_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
