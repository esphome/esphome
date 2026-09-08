import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLEAR_IMPEDANCE,
    CONF_ID,
    CONF_IMPEDANCE,
    CONF_MAC_ADDRESS,
    CONF_WEIGHT,
    DEVICE_CLASS_WEIGHT,
    ICON_OMEGA,
    ICON_SCALE_BATHROOM,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOGRAM,
    UNIT_OHM,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]

xiaomi_miscale_ns = cg.esphome_ns.namespace("xiaomi_miscale")
XiaomiMiscale = xiaomi_miscale_ns.class_(
    "XiaomiMiscale", ble_device_base.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_miscale"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiMiscale),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_CLEAR_IMPEDANCE, default=False): cv.boolean,
            cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                icon=ICON_SCALE_BATHROOM,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_WEIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPEDANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_clear_impedance(config[CONF_CLEAR_IMPEDANCE]))

    if CONF_WEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_WEIGHT])
        cg.add(var.set_weight(sens))
    if CONF_IMPEDANCE in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE])
        cg.add(var.set_impedance(sens))
