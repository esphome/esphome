import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLEAR_IMPEDANCE,
    CONF_ID,
    CONF_IMPEDANCE,
    CONF_MAC_ADDRESS,
    CONF_WEIGHT,
    ICON_OMEGA,
    ICON_SCALE_BATHROOM,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOGRAM,
    UNIT_OHM,
)

DEPENDENCIES = ["esp32_ble_tracker"]

xiaomi_miscale_ns = cg.esphome_ns.namespace("xiaomi_miscale")
XiaomiMiscale = xiaomi_miscale_ns.class_(
    "XiaomiMiscale", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiMiscale),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_CLEAR_IMPEDANCE, default=False): cv.boolean,
            cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                icon=ICON_SCALE_BATHROOM,
                accuracy_decimals=2,
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
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_clear_impedance(config[CONF_CLEAR_IMPEDANCE]))

    if CONF_WEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_WEIGHT])
        cg.add(var.set_weight(sens))
    if CONF_IMPEDANCE in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE])
        cg.add(var.set_impedance(sens))
