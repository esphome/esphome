from esphome import core
import esphome.codegen as cg
from esphome.components import binary_sensor, ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_MAC_ADDRESS, CONF_TIMEOUT, DEVICE_CLASS_MOTION
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "xiaomi_ble"]

xiaomi_mue4094rt_ns = cg.esphome_ns.namespace("xiaomi_mue4094rt")
XiaomiMUE4094RT = xiaomi_mue4094rt_ns.class_(
    "XiaomiMUE4094RT",
    binary_sensor.BinarySensor,
    cg.Component,
    ble_device_base.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_mue4094rt"),
    binary_sensor.binary_sensor_schema(
        XiaomiMUE4094RT, device_class=DEVICE_CLASS_MOTION
    )
    .extend(
        {
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TIMEOUT, default="5s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=65535)),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_time(config[CONF_TIMEOUT]))
