import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CONDUCTIVITY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_MOISTURE,
    ICON_FLOWER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
    UNIT_PERCENT,
)

AUTO_LOAD = ["ble_device_base", "xiaomi_ble"]

xiaomi_hhccpot002_ns = cg.esphome_ns.namespace("xiaomi_hhccpot002")
XiaomiHHCCPOT002 = xiaomi_hhccpot002_ns.class_(
    "XiaomiHHCCPOT002", ble_device_base.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_hhccpot002"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiHHCCPOT002),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROSIEMENS_PER_CENTIMETER,
                icon=ICON_FLOWER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_MOISTURE in config:
        sens = await sensor.new_sensor(config[CONF_MOISTURE])
        cg.add(var.set_moisture(sens))
    if CONF_CONDUCTIVITY in config:
        sens = await sensor.new_sensor(config[CONF_CONDUCTIVITY])
        cg.add(var.set_conductivity(sens))
