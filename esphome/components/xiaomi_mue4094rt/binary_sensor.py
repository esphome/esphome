import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, esp32_ble_tracker
from esphome.const import CONF_MAC_ADDRESS, CONF_DEVICE_CLASS, CONF_TIMEOUT, CONF_ID


DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_mue4094rt_ns = cg.esphome_ns.namespace("xiaomi_mue4094rt")
XiaomiMUE4094RT = xiaomi_mue4094rt_ns.class_(
    "XiaomiMUE4094RT",
    binary_sensor.BinarySensor,
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(XiaomiMUE4094RT),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(
                CONF_DEVICE_CLASS, default="motion"
            ): binary_sensor.device_class,
            cv.Optional(
                CONF_TIMEOUT, default="5s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_time(config[CONF_TIMEOUT]))
